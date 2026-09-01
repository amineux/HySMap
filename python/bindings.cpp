#include <hysmap/hysmap.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <sstream>

namespace py = pybind11;

PYBIND11_MODULE(hysmap, m) {
    m.doc() = "HySMap: activity-weighted multicast hypergraph mapper for SNNs on mesh NoCs";
    m.attr("__version__") = hysmap::version();

    py::enum_<hysmap::MapperKind>(m, "MapperKind")
        .value("EdgeQap", hysmap::MapperKind::EdgeQap)
        .value("ActivityQap", hysmap::MapperKind::ActivityQap)
        .value("Spectral", hysmap::MapperKind::Spectral)
        .value("HySMapSeeded", hysmap::MapperKind::HySMapSeeded)
        .value("HySMap", hysmap::MapperKind::HySMap);

    py::enum_<hysmap::SeedStrategy>(m, "SeedStrategy")
        .value("Random", hysmap::SeedStrategy::Random)
        .value("Balanced", hysmap::SeedStrategy::Balanced)
        .value("Spectral", hysmap::SeedStrategy::Spectral)
        .value("Qap", hysmap::SeedStrategy::Qap);

    py::enum_<hysmap::RefineStrategy>(m, "RefineStrategy")
        .value("None", hysmap::RefineStrategy::None)
        .value("Greedy", hysmap::RefineStrategy::Greedy)
        .value("Multicast", hysmap::RefineStrategy::Multicast);

    py::enum_<hysmap::GeneratorPreset>(m, "GeneratorPreset")
        .value("Potjans", hysmap::GeneratorPreset::Potjans)
        .value("Layered", hysmap::GeneratorPreset::Layered)
        .value("Random", hysmap::GeneratorPreset::Random);

    py::class_<hysmap::ObjectiveWeights>(m, "ObjectiveWeights")
        .def(py::init<>())
        .def_readwrite("alpha", &hysmap::ObjectiveWeights::alpha)
        .def_readwrite("beta", &hysmap::ObjectiveWeights::beta)
        .def_readwrite("gamma", &hysmap::ObjectiveWeights::gamma);

    py::class_<hysmap::CostBreakdown>(m, "CostBreakdown")
        .def_readonly("hops", &hysmap::CostBreakdown::hops)
        .def_readonly("max_load", &hysmap::CostBreakdown::max_load)
        .def_readonly("load_variance", &hysmap::CostBreakdown::load_variance)
        .def_readonly("objective", &hysmap::CostBreakdown::objective)
        .def_readonly("edge_cut", &hysmap::CostBreakdown::edge_cut)
        .def_readonly("remote_fanout", &hysmap::CostBreakdown::remote_fanout)
        .def_readonly("lower_bound", &hysmap::CostBreakdown::lower_bound)
        .def("__repr__", [](const hysmap::CostBreakdown& c) {
            std::ostringstream o;
            o << "CostBreakdown(hops=" << c.hops << ", J=" << c.objective << ")";
            return o.str();
        });

    py::class_<hysmap::Mapping>(m, "Mapping")
        .def_readwrite("partition", &hysmap::Mapping::partition)
        .def_readwrite("placement", &hysmap::Mapping::placement)
        .def("physical_core", &hysmap::Mapping::physical_core)
        .def("assignment", [](const hysmap::Mapping& map) {
            std::vector<hysmap::CoreId> a(map.neuron_count());
            for (hysmap::NeuronId v = 0; v < map.neuron_count(); ++v) {
                a[v] = map.physical_core(v);
            }
            return a;
        });

    py::class_<hysmap::MapperConfig>(m, "MapperConfig")
        .def(py::init<>())
        .def_readwrite("kind", &hysmap::MapperConfig::kind)
        .def_readwrite("edge_passes", &hysmap::MapperConfig::edge_passes)
        .def_readwrite("hyper_passes", &hysmap::MapperConfig::hyper_passes)
        .def_readwrite("placement_restarts", &hysmap::MapperConfig::placement_restarts)
        .def_readwrite("joint_cycles", &hysmap::MapperConfig::joint_cycles)
        .def_readwrite("slack", &hysmap::MapperConfig::slack)
        .def_readwrite("seed", &hysmap::MapperConfig::seed)
        .def_readwrite("threads", &hysmap::MapperConfig::threads)
        .def_readwrite("time_incremental", &hysmap::MapperConfig::time_incremental)
        .def_readwrite("seed_strategy", &hysmap::MapperConfig::seed_strategy)
        .def_readwrite("refine", &hysmap::MapperConfig::refine)
        .def_readwrite("override_seed", &hysmap::MapperConfig::override_seed)
        .def_readwrite("override_refine", &hysmap::MapperConfig::override_refine);

    py::class_<hysmap::MapResult>(m, "MapResult")
        .def_readonly("mapping", &hysmap::MapResult::mapping)
        .def_readonly("metrics", &hysmap::MapResult::metrics)
        .def_readonly("mapper", &hysmap::MapResult::mapper)
        .def_readonly("runtime_ms", &hysmap::MapResult::runtime_ms)
        .def_readonly("has_timing", &hysmap::MapResult::has_timing)
        .def_readonly("timing", &hysmap::MapResult::timing);

    py::class_<hysmap::IncrementalTiming>(m, "IncrementalTiming")
        .def_readonly("speedup", &hysmap::IncrementalTiming::speedup)
        .def_readonly("thread_speedup", &hysmap::IncrementalTiming::thread_speedup)
        .def_readonly("threads", &hysmap::IncrementalTiming::threads)
        .def_readonly("max_abs_error", &hysmap::IncrementalTiming::max_abs_error)
        .def_readonly("avg_affected", &hysmap::IncrementalTiming::avg_affected);

    py::class_<hysmap::DirectedHypergraph>(m, "DirectedHypergraph")
        .def(py::init<>())
        .def("add_neuron", &hysmap::DirectedHypergraph::add_neuron,
             py::arg("label") = "", py::arg("activity") = 1.0)
        .def("add_hyperedge", &hysmap::DirectedHypergraph::add_hyperedge)
        .def("neuron_count", &hysmap::DirectedHypergraph::neuron_count)
        .def("synapse_count", &hysmap::DirectedHypergraph::synapse_count)
        .def("activity", &hysmap::DirectedHypergraph::activity)
        .def("successors", &hysmap::DirectedHypergraph::successors)
        .def("describe", [](const hysmap::DirectedHypergraph& g) {
            return hysmap::describe_network(g);
        });

    py::class_<hysmap::MeshNoC>(m, "MeshNoC")
        .def(py::init<int, int, int>(), py::arg("rows"), py::arg("cols"),
             py::arg("capacity") = 1024)
        .def("rows", &hysmap::MeshNoC::rows)
        .def("cols", &hysmap::MeshNoC::cols)
        .def("core_count", &hysmap::MeshNoC::core_count)
        .def("link_count", &hysmap::MeshNoC::link_count)
        .def("manhattan", &hysmap::MeshNoC::manhattan);

    py::class_<hysmap::GeneratorConfig>(m, "GeneratorConfig")
        .def(py::init<>())
        .def_readwrite("preset", &hysmap::GeneratorConfig::preset)
        .def_readwrite("scale", &hysmap::GeneratorConfig::scale)
        .def_readwrite("target_neurons", &hysmap::GeneratorConfig::target_neurons)
        .def_readwrite("seed", &hysmap::GeneratorConfig::seed);

    m.def("generate_snn", &hysmap::generate_snn);
    m.def("evaluate", &hysmap::evaluate);
    m.def("run_mapper", &hysmap::run_mapper);
    m.def("compare_mappers", &hysmap::compare_mappers);
    m.def("load_network_json", &hysmap::load_network_json);
    m.def("save_network_json", &hysmap::save_network_json, py::arg("graph"), py::arg("path"),
          py::arg("name") = "network");
    m.def("loihi_style_json", &hysmap::loihi_style_json, py::arg("graph"), py::arg("mesh"),
          py::arg("mapping"), py::arg("name") = "hysmap");
    m.def("parse_mapper", &hysmap::parse_mapper);
}
