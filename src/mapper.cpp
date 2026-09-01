#include "hysmap/mapper.hpp"

#include "hysmap/cost.hpp"
#include "hysmap/incremental.hpp"
#include "hysmap/parallel.hpp"
#include "hysmap/partition.hpp"
#include "hysmap/placement.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <thread>

namespace hysmap {

Mapping make_identity_mapping(int neurons, int cores) {
    Mapping m;
    m.partition.resize(static_cast<std::size_t>(neurons));
    for (int i = 0; i < neurons; ++i) {
        m.partition[static_cast<std::size_t>(i)] = static_cast<CoreId>(i % cores);
    }
    m.placement.resize(static_cast<std::size_t>(cores));
    std::iota(m.placement.begin(), m.placement.end(), CoreId{0});
    return m;
}

MapperKind parse_mapper(const std::string& name) {
    if (name == "edge-qap" || name == "edge") {
        return MapperKind::EdgeQap;
    }
    if (name == "activity-qap" || name == "activity") {
        return MapperKind::ActivityQap;
    }
    if (name == "spectral") {
        return MapperKind::Spectral;
    }
    if (name == "hysmap-seeded" || name == "seeded") {
        return MapperKind::HySMapSeeded;
    }
    if (name == "hysmap" || name == "portfolio") {
        return MapperKind::HySMap;
    }
    throw std::invalid_argument("unknown mapper: " + name);
}

SeedStrategy parse_seed(const std::string& name) {
    if (name == "random") {
        return SeedStrategy::Random;
    }
    if (name == "balanced") {
        return SeedStrategy::Balanced;
    }
    if (name == "spectral") {
        return SeedStrategy::Spectral;
    }
    if (name == "qap") {
        return SeedStrategy::Qap;
    }
    throw std::invalid_argument("unknown seed strategy: " + name);
}

RefineStrategy parse_refine(const std::string& name) {
    if (name == "none") {
        return RefineStrategy::None;
    }
    if (name == "greedy") {
        return RefineStrategy::Greedy;
    }
    if (name == "multicast") {
        return RefineStrategy::Multicast;
    }
    throw std::invalid_argument("unknown refine strategy: " + name);
}

static int derived_capacity(const DirectedHypergraph& g, const MeshNoC& mesh,
                            const MapperConfig& cfg) {
    if (cfg.capacity > 0) {
        return cfg.capacity;
    }
    const int n = static_cast<int>(g.neuron_count());
    const int k = mesh.core_count();
    const double fair = static_cast<double>(n) / static_cast<double>(k);
    return std::max(1, static_cast<int>(std::ceil((1.0 + cfg.slack) * fair)));
}

static int threads_of(const MapperConfig& cfg) {
    return effective_threads(cfg.threads);
}

static MapResult finalize(const DirectedHypergraph& g, const MeshNoC& mesh,
                          Mapping mapping, const MapperConfig& cfg, const std::string& name,
                          double ms, IncrementalTiming timing, bool has_timing) {
    MapResult r;
    r.mapping = std::move(mapping);
    r.metrics = evaluate(g, mesh, r.mapping, cfg.weights);
    r.mapper = name;
    r.runtime_ms = ms;
    r.timing = timing;
    r.has_timing = has_timing;
    return r;
}

static void apply_seed(const DirectedHypergraph& g, const MeshNoC& mesh,
                       Mapping& m, const MapperConfig& cfg, std::mt19937_64& rng,
                       int cap, bool activity) {
    const SeedStrategy seed =
        cfg.override_seed
            ? cfg.seed_strategy
            : (cfg.kind == MapperKind::Spectral ? SeedStrategy::Spectral : SeedStrategy::Balanced);
    switch (seed) {
        case SeedStrategy::Random:
            assign_random(m, static_cast<int>(g.neuron_count()), mesh.core_count(), rng, cap);
            break;
        case SeedStrategy::Balanced:
            assign_balanced(m, static_cast<int>(g.neuron_count()), mesh.core_count(), rng);
            break;
        case SeedStrategy::Spectral:
            seed_spectral_partition(g, mesh, m, cap);
            break;
        case SeedStrategy::Qap:
            assign_balanced(m, static_cast<int>(g.neuron_count()), mesh.core_count(), rng);
            refine_edge_cut(g, m, mesh, activity, rng, cfg.edge_passes, cfg.slack, cap);
            place_qap(g, mesh, m, activity, rng, cfg.placement_restarts);
            return;
    }
}

static Mapping seed_edge_qap(const DirectedHypergraph& g, const MeshNoC& mesh,
                             const MapperConfig& cfg, std::mt19937_64& rng, bool activity) {
    Mapping m;
    const int cap = derived_capacity(g, mesh, cfg);
    apply_seed(g, mesh, m, cfg, rng, cap, activity);
    if (!cfg.override_seed || cfg.seed_strategy != SeedStrategy::Qap) {
        refine_edge_cut(g, m, mesh, activity, rng, cfg.edge_passes, cfg.slack, cap);
        if (cfg.override_seed && cfg.seed_strategy == SeedStrategy::Spectral) {
            place_spectral(g, mesh, m);
        } else {
            place_qap(g, mesh, m, activity, rng, cfg.placement_restarts);
        }
        place_force(g, mesh, m, activity, 24);
    }
    return m;
}

static Mapping refine_hysmap(const DirectedHypergraph& g, const MeshNoC& mesh,
                             Mapping m, const MapperConfig& cfg, std::mt19937_64& rng,
                             IncrementalTiming* timing) {
    const int cap = derived_capacity(g, mesh, cfg);
    const int th = threads_of(cfg);
    IncrementalEvaluator eval(g, mesh, m, cfg.weights);
    if (timing != nullptr && th > 1) {
        measure_parallel_gains(g, eval, m, *timing, th);
    }
    refine_multicast(g, mesh, m, eval, rng, cfg.hyper_passes, cfg.slack, cap, timing, th);
    refine_placement_multicast(mesh, m, eval, rng, std::max(1, cfg.placement_restarts / 2), th);

    for (int cyc = 0; cyc < cfg.joint_cycles; ++cyc) {
        place_qap(g, mesh, m, true, rng, 2);
        eval.rebuild();
        refine_placement_multicast(mesh, m, eval, rng, 1, th);
        refine_multicast(g, mesh, m, eval, rng, cfg.hyper_passes, cfg.slack, cap, timing, th);
    }
    refine_placement_multicast(mesh, m, eval, rng, 1, th);
    return m;
}

static void apply_refine_override(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& m,
                                  const MapperConfig& cfg, std::mt19937_64& rng,
                                  IncrementalTiming* timing) {
    const int cap = derived_capacity(g, mesh, cfg);
    const int th = threads_of(cfg);
    if (cfg.refine == RefineStrategy::None) {
        return;
    }
    if (cfg.refine == RefineStrategy::Greedy) {
        refine_edge_cut(g, m, mesh, true, rng, cfg.edge_passes, cfg.slack, cap);
        return;
    }
    IncrementalEvaluator eval(g, mesh, m, cfg.weights);
    if (timing != nullptr && th > 1) {
        measure_parallel_gains(g, eval, m, *timing, th);
    }
    refine_multicast(g, mesh, m, eval, rng, cfg.hyper_passes, cfg.slack, cap, timing, th);
}

static MapResult run_one(const DirectedHypergraph& g, MeshNoC mesh, MapperKind kind,
                         const MapperConfig& cfg) {
    const auto t0 = std::chrono::steady_clock::now();
    std::mt19937_64 rng(cfg.seed);
    IncrementalTiming timing{};
    IncrementalTiming* tptr = cfg.time_incremental ? &timing : nullptr;
    const int cap = derived_capacity(g, mesh, cfg);
    mesh.set_capacity(cap);
    const int th = threads_of(cfg);
    timing.threads = th;

    Mapping m;
    switch (kind) {
        case MapperKind::EdgeQap:
            m = seed_edge_qap(g, mesh, cfg, rng, false);
            if (cfg.override_refine) {
                apply_refine_override(g, mesh, m, cfg, rng, tptr);
            }
            break;
        case MapperKind::ActivityQap:
            m = seed_edge_qap(g, mesh, cfg, rng, true);
            if (cfg.override_refine) {
                apply_refine_override(g, mesh, m, cfg, rng, tptr);
            }
            break;
        case MapperKind::Spectral:
            seed_spectral_partition(g, mesh, m, cap);
            refine_edge_cut(g, m, mesh, true, rng, cfg.edge_passes, cfg.slack, cap);
            place_spectral(g, mesh, m);
            if (cfg.override_refine && cfg.refine != RefineStrategy::Multicast) {
                apply_refine_override(g, mesh, m, cfg, rng, tptr);
            } else {
                m = refine_hysmap(g, mesh, std::move(m), cfg, rng, tptr);
            }
            break;
        case MapperKind::HySMapSeeded: {
            m = seed_edge_qap(g, mesh, cfg, rng, true);
            if (cfg.override_refine && cfg.refine != RefineStrategy::Multicast) {
                apply_refine_override(g, mesh, m, cfg, rng, tptr);
            } else {
                IncrementalEvaluator eval(g, mesh, m, cfg.weights);
                if (tptr != nullptr && th > 1) {
                    measure_parallel_gains(g, eval, m, timing, th);
                }
                refine_multicast(g, mesh, m, eval, rng, cfg.hyper_passes, cfg.slack, cap, tptr,
                                 th);
                refine_placement_multicast(mesh, m, eval, rng, 1, th);
            }
            break;
        }
        case MapperKind::HySMap: {
            MapperConfig sub = cfg;
            sub.kind = MapperKind::ActivityQap;
            sub.override_refine = false;
            auto a = run_one(g, mesh, MapperKind::ActivityQap, sub);
            auto s = run_one(g, mesh, MapperKind::HySMapSeeded, sub);
            Mapping spec;
            seed_spectral_partition(g, mesh, spec, cap);
            refine_edge_cut(g, spec, mesh, true, rng, cfg.edge_passes, cfg.slack, cap);
            place_spectral(g, mesh, spec);
            IncrementalTiming t2{};
            Mapping refined = refine_hysmap(g, mesh, spec, cfg, rng,
                                            cfg.time_incremental ? &t2 : nullptr);
            const CostBreakdown ja = evaluate(g, mesh, a.mapping, cfg.weights);
            const CostBreakdown js = evaluate(g, mesh, s.mapping, cfg.weights);
            const CostBreakdown jr = evaluate(g, mesh, refined, cfg.weights);
            m = a.mapping;
            double best = ja.objective;
            if (js.objective < best) {
                best = js.objective;
                m = s.mapping;
                timing = s.timing;
            }
            if (jr.objective < best) {
                m = std::move(refined);
                timing = t2;
            }
            if (cfg.override_refine && cfg.refine != RefineStrategy::Multicast) {
                apply_refine_override(g, mesh, m, cfg, rng, tptr);
            } else {
                m = refine_hysmap(g, mesh, std::move(m), cfg, rng, tptr);
            }
            break;
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return finalize(g, mesh, std::move(m), cfg, mapper_name(kind), ms, timing,
                    cfg.time_incremental);
}

MapResult run_mapper(const DirectedHypergraph& g, const MeshNoC& mesh,
                     const MapperConfig& cfg) {
    return run_one(g, mesh, cfg.kind, cfg);
}

std::vector<MapResult> compare_mappers(const DirectedHypergraph& g, const MeshNoC& mesh,
                                       const MapperConfig& base) {
    const std::vector<MapperKind> kinds{MapperKind::EdgeQap, MapperKind::ActivityQap,
                                        MapperKind::Spectral, MapperKind::HySMapSeeded,
                                        MapperKind::HySMap};
    std::vector<MapResult> out(kinds.size());
    const int th = threads_of(base);
    // Independent mapper pipelines in parallel; each keeps its own RNG seed.
    parallel_for(static_cast<int>(kinds.size()), std::min(th, static_cast<int>(kinds.size())),
                 [&](int i) {
                     MapperConfig c = base;
                     c.kind = kinds[static_cast<std::size_t>(i)];
                     // Avoid 5× oversubscription inside each pipeline.
                     c.threads = std::max(1, th / static_cast<int>(kinds.size()));
                     out[static_cast<std::size_t>(i)] = run_mapper(g, mesh, c);
                 });
    return out;
}

}  // namespace hysmap
