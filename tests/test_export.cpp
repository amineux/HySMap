#include <catch2/catch_test_macros.hpp>

#include <string>
#include <hysmap/export.hpp>
#include <hysmap/mapper.hpp>
#include <hysmap/snn_generator.hpp>

TEST_CASE("Loihi-style export names cores and fanout") {
    hysmap::GeneratorConfig gc;
    gc.target_neurons = 24;
    gc.seed = 4;
    const auto g = hysmap::generate_snn(gc);
    const hysmap::MeshNoC mesh(4, 4, 8);
    hysmap::MapperConfig cfg;
    cfg.kind = hysmap::MapperKind::ActivityQap;
    cfg.edge_passes = 1;
    cfg.placement_restarts = 1;
    cfg.seed = 4;
    const auto r = hysmap::run_mapper(g, mesh, cfg);
    const auto js = hysmap::loihi_style_json(g, mesh, r.mapping);
    REQUIRE(js.find("hysmap-loihi-style") != std::string::npos);
    REQUIRE(js.find("neurocore") != std::string::npos);
    REQUIRE(js.find("Not an official Intel") != std::string::npos);
    const auto stub = hysmap::loihi_style_stub(g, mesh, r.mapping);
    REQUIRE(stub.find("CORE 0") != std::string::npos);
    REQUIRE(stub.find("NEURON") != std::string::npos);
}

TEST_CASE("seed-strategy random and refine greedy produce a feasible map") {
    hysmap::GeneratorConfig gc;
    gc.target_neurons = 32;
    gc.seed = 2;
    const auto g = hysmap::generate_snn(gc);
    const hysmap::MeshNoC mesh(4, 4, 8);
    hysmap::MapperConfig cfg;
    cfg.kind = hysmap::MapperKind::ActivityQap;
    cfg.override_seed = true;
    cfg.seed_strategy = hysmap::SeedStrategy::Random;
    cfg.override_refine = true;
    cfg.refine = hysmap::RefineStrategy::Greedy;
    cfg.edge_passes = 2;
    cfg.seed = 2;
    const auto r = hysmap::run_mapper(g, mesh, cfg);
    REQUIRE(r.mapping.partition.size() == g.neuron_count());
    REQUIRE(r.metrics.hops >= r.metrics.remote_fanout - 1e-6);
}
