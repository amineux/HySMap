#include <catch2/catch_test_macros.hpp>
#include <hysmap/io.hpp>
#include <hysmap/mapper.hpp>
#include <hysmap/snn_generator.hpp>

#include <filesystem>
#include <fstream>

TEST_CASE("identity mapping covers every neuron exactly once") {
    const auto m = hysmap::make_identity_mapping(10, 4);
    REQUIRE(m.neuron_count() == 10);
    REQUIRE(m.core_count() == 4);
    REQUIRE(m.physical_core(0) == 0);
    REQUIRE(m.physical_core(4) == 0);
}

TEST_CASE("end-to-end mappers produce feasible mappings") {
    hysmap::GeneratorConfig gc;
    gc.target_neurons = 36;
    gc.seed = 1;
    const auto g = hysmap::generate_snn(gc);
    const hysmap::MeshNoC mesh(4, 4, 8);
    hysmap::MapperConfig cfg;
    cfg.edge_passes = 1;
    cfg.hyper_passes = 2;
    cfg.placement_restarts = 2;
    cfg.joint_cycles = 1;
    cfg.seed = 1;

    for (auto kind : {hysmap::MapperKind::EdgeQap, hysmap::MapperKind::ActivityQap,
                      hysmap::MapperKind::HySMapSeeded}) {
        cfg.kind = kind;
        const auto r = hysmap::run_mapper(g, mesh, cfg);
        REQUIRE(r.mapping.partition.size() == g.neuron_count());
        REQUIRE(r.mapping.placement.size() == static_cast<std::size_t>(mesh.core_count()));
        REQUIRE(r.metrics.hops >= r.metrics.remote_fanout - 1e-9);
        REQUIRE(r.metrics.lower_bound <= r.metrics.objective + 1e-6);
    }
}

TEST_CASE("network JSON round-trip") {
    hysmap::GeneratorConfig gc;
    gc.target_neurons = 20;
    gc.seed = 9;
    const auto g = hysmap::generate_snn(gc);
    const auto path = (std::filesystem::temp_directory_path() / "hysmap_roundtrip.json").string();
    hysmap::save_network_json(g, path, "roundtrip");
    const auto h = hysmap::load_network_json(path);
    REQUIRE(h.neuron_count() == g.neuron_count());
    REQUIRE(h.synapse_count() == g.synapse_count());
    REQUIRE(h.activity(0) == g.activity(0));
}
