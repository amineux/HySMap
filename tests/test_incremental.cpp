#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <hysmap/cost.hpp>
#include <hysmap/incremental.hpp>
#include <hysmap/partition.hpp>
#include <hysmap/snn_generator.hpp>

#include <algorithm>
#include <cmath>
#include <random>

TEST_CASE("incremental partition move matches full recompute") {
    hysmap::GeneratorConfig gc;
    gc.preset = hysmap::GeneratorPreset::Potjans;
    gc.target_neurons = 48;
    gc.seed = 7;
    const auto g = hysmap::generate_snn(gc);
    hysmap::MeshNoC mesh(4, 4, 8);
    std::mt19937_64 rng(7);
    hysmap::Mapping m;
    hysmap::assign_balanced(m, static_cast<int>(g.neuron_count()), mesh.core_count(), rng);

    hysmap::ObjectiveWeights w;
    hysmap::IncrementalEvaluator eval(g, mesh, m, w);
    const auto full0 = eval.full_recompute();
    REQUIRE(eval.current().objective == Catch::Approx(full0.objective).margin(1e-9));
    REQUIRE(eval.current().hops == Catch::Approx(full0.hops).margin(1e-9));

    double max_err = 0.0;
    for (hysmap::NeuronId v = 0; v < g.neuron_count(); ++v) {
        for (int c = 0; c < mesh.core_count(); ++c) {
            const auto peek = eval.peek_partition_move(v, static_cast<hysmap::CoreId>(c));
            const auto old = m.partition[v];
            m.partition[v] = static_cast<hysmap::CoreId>(c);
            const auto full = eval.full_recompute();
            m.partition[v] = old;
            max_err = std::max(max_err, std::abs(peek.objective - full.objective));
            max_err = std::max(max_err, std::abs(peek.hops - full.hops));
        }
    }
    REQUIRE(max_err < 1e-9);
}

TEST_CASE("commit then rebuild stays consistent") {
    hysmap::GeneratorConfig gc;
    gc.target_neurons = 32;
    gc.seed = 3;
    const auto g = hysmap::generate_snn(gc);
    hysmap::MeshNoC mesh(4, 4, 6);
    std::mt19937_64 rng(3);
    hysmap::Mapping m;
    hysmap::assign_balanced(m, static_cast<int>(g.neuron_count()), 16, rng);
    hysmap::IncrementalEvaluator eval(g, mesh, m, {});
    eval.commit_partition_move(0, (m.partition[0] + 1) % 16);
    const auto a = eval.current();
    const auto b = eval.full_recompute();
    REQUIRE(a.objective == Catch::Approx(b.objective).margin(1e-9));
}

TEST_CASE("placement lower bound never exceeds hops contribution") {
    hysmap::GeneratorConfig gc;
    gc.target_neurons = 40;
    gc.seed = 11;
    const auto g = hysmap::generate_snn(gc);
    const hysmap::MeshNoC mesh(4, 4, 8);
    std::mt19937_64 rng(11);
    hysmap::Mapping m;
    hysmap::assign_balanced(m, static_cast<int>(g.neuron_count()), 16, rng);
    const auto cost = hysmap::evaluate(g, mesh, m, {});
    REQUIRE(cost.lower_bound <= cost.objective + 1e-9);
    REQUIRE(cost.remote_fanout <= cost.hops + 1e-9);
}
