#include "hysmap/partition.hpp"

#include "hysmap/cost.hpp"
#include "hysmap/parallel.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <unordered_set>

namespace hysmap {

BalanceBounds balance_bounds(int neurons, int cores, double slack, int capacity) {
    const double fair = static_cast<double>(neurons) / static_cast<double>(std::max(1, cores));
    BalanceBounds b;
    b.lo = static_cast<int>(std::floor((1.0 - slack) * fair));
    b.hi = static_cast<int>(std::ceil((1.0 + slack) * fair));
    b.hi = std::min(b.hi, capacity);
    b.lo = std::max(0, b.lo);
    if (b.lo > b.hi) {
        b.lo = 0;
    }
    return b;
}

void assign_balanced(Mapping& mapping, int neurons, int cores, std::mt19937_64& rng) {
    mapping.partition.assign(static_cast<std::size_t>(neurons), 0);
    mapping.placement.resize(static_cast<std::size_t>(cores));
    std::iota(mapping.placement.begin(), mapping.placement.end(), CoreId{0});

    std::vector<NeuronId> order(static_cast<std::size_t>(neurons));
    std::iota(order.begin(), order.end(), NeuronId{0});
    std::shuffle(order.begin(), order.end(), rng);
    for (int i = 0; i < neurons; ++i) {
        mapping.partition[order[static_cast<std::size_t>(i)]] =
            static_cast<CoreId>(i % cores);
    }
}

void assign_random(Mapping& mapping, int neurons, int cores, std::mt19937_64& rng,
                   int capacity) {
    mapping.partition.assign(static_cast<std::size_t>(neurons), 0);
    mapping.placement.resize(static_cast<std::size_t>(cores));
    std::iota(mapping.placement.begin(), mapping.placement.end(), CoreId{0});
    std::uniform_int_distribution<int> dist(0, std::max(0, cores - 1));
    std::vector<int> occ(static_cast<std::size_t>(cores), 0);
    const int cap = std::max(1, capacity);
    for (int i = 0; i < neurons; ++i) {
        int c = dist(rng);
        int guard = 0;
        while (occ[static_cast<std::size_t>(c)] >= cap && guard < cores * 2) {
            c = (c + 1) % cores;
            ++guard;
        }
        mapping.partition[static_cast<std::size_t>(i)] = static_cast<CoreId>(c);
        ++occ[static_cast<std::size_t>(c)];
    }
}

bool is_boundary(const DirectedHypergraph& g, const Mapping& m, NeuronId v) {
    const CoreId c = m.partition[v];
    for (NeuronId u : g.predecessors(v)) {
        if (m.partition[u] != c) {
            return true;
        }
    }
    for (NeuronId u : g.successors(v)) {
        if (m.partition[u] != c) {
            return true;
        }
    }
    return false;
}

static bool move_feasible(const std::vector<int>& occ, CoreId from, CoreId to,
                          const BalanceBounds& b) {
    if (from == to) {
        return false;
    }
    const int nf = occ[from] - 1;
    const int nt = occ[to] + 1;
    return nf >= b.lo && nt <= b.hi;
}

static std::vector<CoreId> candidate_cores(const DirectedHypergraph& g, const Mapping& m,
                                           NeuronId v, int cores, std::mt19937_64& rng) {
    std::unordered_set<CoreId> s;
    for (NeuronId u : g.predecessors(v)) {
        s.insert(m.partition[u]);
    }
    for (NeuronId u : g.successors(v)) {
        s.insert(m.partition[u]);
    }
    s.erase(m.partition[v]);

    std::vector<CoreId> out(s.begin(), s.end());
    // For modest meshes, also consider every other core so fanout can collapse.
    if (cores <= 64) {
        out.clear();
        for (int c = 0; c < cores; ++c) {
            if (static_cast<CoreId>(c) != m.partition[v]) {
                out.push_back(static_cast<CoreId>(c));
            }
        }
        return out;
    }
    if (out.size() < 4) {
        std::uniform_int_distribution<int> dist(0, cores - 1);
        while (static_cast<int>(out.size()) < std::min(6, cores - 1)) {
            const CoreId extra = static_cast<CoreId>(dist(rng));
            if (extra != m.partition[v] &&
                std::find(out.begin(), out.end(), extra) == out.end()) {
                out.push_back(extra);
            }
        }
    }
    return out;
}

static double edge_cut_delta(const DirectedHypergraph& g, const Mapping& m, NeuronId v,
                             CoreId to, bool activity_weighted) {
    const CoreId from = m.partition[v];
    double delta = 0.0;
    auto contrib = [&](NeuronId src, CoreId src_core, CoreId dst_core) {
        const double w = activity_weighted ? g.activity(src) : 1.0;
        const bool old_cut = src_core != dst_core;
        return old_cut ? w : 0.0;
    };

    // outgoing synapses of v
    for (NeuronId d : g.successors(v)) {
        const CoreId dc = (d == v) ? to : m.partition[d];
        delta -= contrib(v, from, m.partition[d]);
        delta += contrib(v, to, dc);
    }
    // incoming synapses to v
    for (NeuronId u : g.predecessors(v)) {
        if (u == v) {
            continue;
        }
        delta -= contrib(u, m.partition[u], from);
        delta += contrib(u, m.partition[u], to);
    }
    return delta;
}

int refine_edge_cut(const DirectedHypergraph& g, Mapping& mapping, const MeshNoC& mesh,
                    bool activity_weighted, std::mt19937_64& rng, int passes,
                    double slack, int capacity) {
    const int n = static_cast<int>(g.neuron_count());
    const int k = mesh.core_count();
    const BalanceBounds bounds = balance_bounds(n, k, slack, capacity);
    auto occ = core_occupancies(mapping, k);
    int applied = 0;

    for (int pass = 0; pass < passes; ++pass) {
        std::vector<NeuronId> nodes(static_cast<std::size_t>(n));
        std::iota(nodes.begin(), nodes.end(), NeuronId{0});
        std::shuffle(nodes.begin(), nodes.end(), rng);
        bool any = false;
        for (NeuronId v : nodes) {
            if (!is_boundary(g, mapping, v)) {
                continue;
            }
            const auto cands = candidate_cores(g, mapping, v, k, rng);
            double best = 0.0;
            CoreId best_c = kInvalidCore;
            for (CoreId c : cands) {
                if (!move_feasible(occ, mapping.partition[v], c, bounds)) {
                    continue;
                }
                const double d = edge_cut_delta(g, mapping, v, c, activity_weighted);
                if (d < best - 1e-12) {
                    best = d;
                    best_c = c;
                }
            }
            if (best_c != kInvalidCore) {
                --occ[mapping.partition[v]];
                ++occ[best_c];
                mapping.partition[v] = best_c;
                ++applied;
                any = true;
            }
        }
        if (!any) {
            break;
        }
    }
    return applied;
}

void measure_parallel_gains(const DirectedHypergraph& g, IncrementalEvaluator& eval,
                            Mapping& mapping, IncrementalTiming& timing, int threads) {
    timing.threads = threads;
    std::vector<NeuronId> nodes;
    for (NeuronId v = 0; v < g.neuron_count(); ++v) {
        if (is_boundary(g, mapping, v)) {
            nodes.push_back(v);
        }
        if (nodes.size() >= 12) {
            break;
        }
    }
    if (nodes.empty()) {
        return;
    }
    const int k = static_cast<int>(mapping.placement.size());
    std::vector<std::pair<NeuronId, CoreId>> jobs;
    for (NeuronId v : nodes) {
        for (int c = 0; c < k; ++c) {
            if (static_cast<CoreId>(c) != mapping.partition[v]) {
                jobs.emplace_back(v, static_cast<CoreId>(c));
            }
        }
    }
    if (jobs.size() < 32) {
        return;
    }

    const auto t0 = std::chrono::steady_clock::now();
    double sink = 0.0;
    for (const auto& [v, c] : jobs) {
        sink += eval.peek_partition_move(v, c).objective;
    }
    const auto t1 = std::chrono::steady_clock::now();
    std::vector<double> par(jobs.size(), 0.0);
    const auto p0 = std::chrono::steady_clock::now();
    parallel_for(static_cast<int>(jobs.size()), threads, [&](int i) {
        const auto [v, c] = jobs[static_cast<std::size_t>(i)];
        par[static_cast<std::size_t>(i)] = eval.peek_partition_move(v, c).objective;
    });
    const auto p1 = std::chrono::steady_clock::now();
    (void)sink;
    (void)par;
    timing.thread_serial_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    timing.thread_parallel_ms = std::chrono::duration<double, std::milli>(p1 - p0).count();
    timing.thread_speedup = (timing.thread_parallel_ms > 1e-9)
                                ? (timing.thread_serial_ms / timing.thread_parallel_ms)
                                : 0.0;
}

int refine_multicast(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& mapping,
                     IncrementalEvaluator& eval, std::mt19937_64& rng, int passes,
                     double slack, int capacity, IncrementalTiming* timing, int threads) {
    const int n = static_cast<int>(g.neuron_count());
    const int k = mesh.core_count();
    const BalanceBounds bounds = balance_bounds(n, k, slack, capacity);
    auto occ = core_occupancies(mapping, k);
    int applied = 0;

    double full_ms = 0.0;
    double inc_ms = 0.0;
    double max_err = 0.0;
    double affected_sum = 0.0;
    std::uint64_t evals = 0;

    for (int pass = 0; pass < passes; ++pass) {
        std::vector<NeuronId> nodes(static_cast<std::size_t>(n));
        std::iota(nodes.begin(), nodes.end(), NeuronId{0});
        std::shuffle(nodes.begin(), nodes.end(), rng);
        bool any = false;
        for (NeuronId v : nodes) {
            if (!is_boundary(g, mapping, v)) {
                continue;
            }
            const auto raw = candidate_cores(g, mapping, v, k, rng);
            std::vector<CoreId> cands;
            cands.reserve(raw.size());
            for (CoreId c : raw) {
                if (move_feasible(occ, mapping.partition[v], c, bounds)) {
                    cands.push_back(c);
                }
            }
            if (cands.empty()) {
                continue;
            }

            std::vector<CostBreakdown> trials(cands.size());
            const auto t0 = std::chrono::steady_clock::now();
            parallel_for(static_cast<int>(cands.size()), threads, [&](int i) {
                trials[static_cast<std::size_t>(i)] =
                    eval.peek_partition_move(v, cands[static_cast<std::size_t>(i)]);
            });
            const auto t1 = std::chrono::steady_clock::now();
            inc_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
            evals += static_cast<std::uint64_t>(cands.size());
            affected_sum += static_cast<double>(g.affected_sources(v).size()) *
                            static_cast<double>(cands.size());

            double best_j = eval.current().objective;
            CoreId best_c = kInvalidCore;
            for (std::size_t i = 0; i < cands.size(); ++i) {
                if (timing != nullptr) {
                    const CoreId old = mapping.partition[v];
                    mapping.partition[v] = cands[i];
                    const auto f0 = std::chrono::steady_clock::now();
                    const CostBreakdown full = eval.full_recompute();
                    const auto f1 = std::chrono::steady_clock::now();
                    mapping.partition[v] = old;
                    full_ms += std::chrono::duration<double, std::milli>(f1 - f0).count();
                    max_err = std::max(max_err, std::abs(full.objective - trials[i].objective));
                }
                if (trials[i].objective < best_j - 1e-12) {
                    best_j = trials[i].objective;
                    best_c = cands[i];
                }
            }
            if (best_c != kInvalidCore) {
                --occ[mapping.partition[v]];
                ++occ[best_c];
                eval.commit_partition_move(v, best_c);
                ++applied;
                any = true;
            }
        }
        if (!any) {
            break;
        }
    }

    if (timing != nullptr) {
        timing->incremental_ms += inc_ms;
        timing->full_ms += full_ms;
        timing->evaluations += evals;
        if (evals > 0) {
            timing->avg_affected = affected_sum / static_cast<double>(evals);
        }
        timing->max_abs_error = std::max(timing->max_abs_error, max_err);
        timing->speedup = (timing->incremental_ms > 1e-12)
                              ? (timing->full_ms / timing->incremental_ms)
                              : 0.0;
    }
    return applied;
}

}  // namespace hysmap
