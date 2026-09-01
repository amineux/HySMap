#include <catch2/catch_test_macros.hpp>
#include <hysmap/cost.hpp>
#include <hysmap/partition.hpp>
#include <hysmap/snn_generator.hpp>

#include <algorithm>
#include <random>

TEST_CASE("balance bounds and occupancy are respected") {
    hysmap::GeneratorConfig gc;
    gc.target_neurons = 64;
    gc.seed = 2;
    const auto g = hysmap::generate_snn(gc);
    hysmap::MeshNoC mesh(4, 4, 16);
    std::mt19937_64 rng(2);
    hysmap::Mapping m;
    hysmap::assign_balanced(m, static_cast<int>(g.neuron_count()), 16, rng);
    REQUIRE(hysmap::respects_balance(m, 0.15, 16));

    hysmap::refine_edge_cut(g, m, mesh, true, rng, 3, 0.15, 16);
    REQUIRE(hysmap::respects_balance(m, 0.15, 16));

    const auto occ = hysmap::core_occupancies(m, 16);
    const int n = static_cast<int>(g.neuron_count());
    int sum = 0;
    for (int c : occ) {
        sum += c;
        REQUIRE(c <= 16);
    }
    REQUIRE(sum == n);
}

TEST_CASE("edge-cut refinement does not increase cut") {
    hysmap::GeneratorConfig gc;
    gc.target_neurons = 48;
    gc.seed = 5;
    const auto g = hysmap::generate_snn(gc);
    hysmap::MeshNoC mesh(4, 4, 8);
    std::mt19937_64 rng(5);
    hysmap::Mapping m;
    hysmap::assign_balanced(m, static_cast<int>(g.neuron_count()), 16, rng);
    const double before = hysmap::evaluate(g, mesh, m, {}).edge_cut;
    hysmap::refine_edge_cut(g, m, mesh, false, rng, 4, 0.15, 8);
    const double after = hysmap::evaluate(g, mesh, m, {}).edge_cut;
    REQUIRE(after <= before + 1e-9);
}
