#include "hysmap/hysmap.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string cmd;
    std::string input;
    std::string output;
    std::string csv;
    std::string mapper = "hysmap";
    std::string preset = "potjans";
    std::string seed_strategy;
    std::string refine;
    std::string format = "loihi-json";
    int threads = 0;
    int mesh = 4;
    int rows = 0;
    int cols = 0;
    int neurons = 0;
    double scale = 0.002;
    std::uint64_t seed = 1;
    int edge_passes = 2;
    int hyper_passes = 4;
    int placement_restarts = 5;
    int joint_cycles = 2;
    double slack = 0.15;
    bool time_inc = false;
    bool json_stdout = false;
    bool full = false;
    std::vector<int> meshes{4, 5, 6};
    std::vector<int> sizes{80, 108, 160};
    int bench_seeds = 3;
};

void usage() {
    std::cout
        << "HySMap " << hysmap::version()
        << " — activity-weighted multicast hypergraph mapper for SNNs on mesh NoCs\n\n"
        << "Usage:\n"
        << "  hysmap generate [--preset potjans|layered|random] [--scale S] [--neurons N]\n"
        << "                  [--seed R] [--out net.json]\n"
        << "  hysmap map      --input net.json [--mesh N | --rows R --cols C]\n"
        << "                  [--mapper edge-qap|activity-qap|spectral|hysmap-seeded|hysmap]\n"
        << "                  [--seed-strategy random|balanced|spectral|qap]\n"
        << "                  [--refine none|greedy|multicast] [--threads N]\n"
        << "                  [--seed R] [--time-incremental] [--json] [--out result.json]\n"
        << "                  [--csv result.csv]\n"
        << "  hysmap compare  --input net.json [--mesh N] [--seed R] [--threads N]\n"
        << "                  [--time-incremental] [--csv compare.csv]\n"
        << "  hysmap export   --input net.json --format loihi-json|loihi-stub\n"
        << "                  [--mesh N] [--mapper hysmap] [--out out.json]\n"
        << "  hysmap bench    [--quick | --full] [--threads N] [--out results/bench.csv]\n"
        << "  hysmap demo     (Phase 1: tiny simulator + mapper on a 4×4 mesh)\n"
        << "  hysmap version\n\n"
        << "Phases 1–6: docs/phases/   Technical report: docs/technical-report.md\n"
        << "Inspired by arXiv:2601.16118 and arXiv:2608.26223 — see README.md.\n";
}

std::string require(int& i, int argc, char** argv, const char* flag) {
    if (i + 1 >= argc) {
        throw std::invalid_argument(std::string("missing value for ") + flag);
    }
    return argv[++i];
}

hysmap::GeneratorPreset parse_preset(const std::string& s) {
    if (s == "potjans") {
        return hysmap::GeneratorPreset::Potjans;
    }
    if (s == "layered") {
        return hysmap::GeneratorPreset::Layered;
    }
    if (s == "random") {
        return hysmap::GeneratorPreset::Random;
    }
    throw std::invalid_argument("unknown preset: " + s);
}

hysmap::MeshNoC make_mesh(const Args& a) {
    const int rows = a.rows > 0 ? a.rows : a.mesh;
    const int cols = a.cols > 0 ? a.cols : a.mesh;
    return hysmap::MeshNoC(rows, cols);
}

hysmap::MapperConfig make_cfg(const Args& a) {
    hysmap::MapperConfig c;
    c.kind = hysmap::parse_mapper(a.mapper);
    c.edge_passes = a.edge_passes;
    c.hyper_passes = a.hyper_passes;
    c.placement_restarts = a.placement_restarts;
    c.joint_cycles = a.joint_cycles;
    c.slack = a.slack;
    c.seed = a.seed;
    c.time_incremental = a.time_inc;
    c.threads = a.threads;
    if (!a.seed_strategy.empty()) {
        c.seed_strategy = hysmap::parse_seed(a.seed_strategy);
        c.override_seed = true;
    }
    if (!a.refine.empty()) {
        c.refine = hysmap::parse_refine(a.refine);
        c.override_refine = true;
    }
    return c;
}

void print_metrics(const hysmap::MapResult& r, const hysmap::MeshNoC& mesh,
                   const hysmap::DirectedHypergraph& g) {
    std::cout.setf(std::ios::fixed);
    std::cout.precision(3);
    std::cout << "mapper           " << r.mapper << "\n"
              << "network          " << hysmap::describe_network(g) << "\n"
              << "mesh             " << mesh.rows() << "x" << mesh.cols() << " ("
              << mesh.core_count() << " cores, " << mesh.link_count() << " directed links)\n"
              << "multicast hops   " << r.metrics.hops << "\n"
              << "max link load    " << r.metrics.max_load << "\n"
              << "load variance    " << r.metrics.load_variance << "\n"
              << "objective J      " << r.metrics.objective << "\n"
              << "edge cut         " << r.metrics.edge_cut << "\n"
              << "activity cut     " << r.metrics.activity_cut << "\n"
              << "remote fanout    " << r.metrics.remote_fanout << "\n"
              << "lower bound      " << r.metrics.lower_bound << "\n"
              << "runtime          " << r.runtime_ms << " ms\n";
    if (r.has_timing && r.timing.evaluations > 0) {
        std::cout << "inc. speedup     " << r.timing.speedup << "x  (full "
                  << r.timing.full_ms << " ms vs inc " << r.timing.incremental_ms
                  << " ms, avg |A(v)|=" << r.timing.avg_affected
                  << ", max |ΔJ|=" << r.timing.max_abs_error << ")\n";
        if (r.timing.thread_speedup > 0.0) {
            std::cout << "thread speedup  " << r.timing.thread_speedup << "x  ("
                      << r.timing.threads << " threads, serial " << r.timing.thread_serial_ms
                      << " ms vs parallel " << r.timing.thread_parallel_ms << " ms)\n";
        }
    }
}

int cmd_generate(const Args& a) {
    hysmap::GeneratorConfig gc;
    gc.preset = parse_preset(a.preset);
    gc.scale = a.scale;
    gc.target_neurons = a.neurons;
    gc.seed = a.seed;
    const auto g = hysmap::generate_snn(gc);
    const std::string out = a.output.empty() ? "network.json" : a.output;
    hysmap::save_network_json(g, out, a.preset);
    std::cout << "wrote " << out << " (" << hysmap::describe_network(g) << ")\n";
    return 0;
}

int cmd_map(const Args& a) {
    if (a.input.empty()) {
        throw std::invalid_argument("map requires --input");
    }
    const auto g = hysmap::load_network_json(a.input);
    const auto mesh = make_mesh(a);
    const auto r = hysmap::run_mapper(g, mesh, make_cfg(a));
    if (a.json_stdout) {
        std::cout << hysmap::metrics_json(r, mesh, g);
    } else {
        print_metrics(r, mesh, g);
    }
    if (!a.output.empty()) {
        hysmap::save_result_json(r, mesh, g, a.output);
        std::cout << "wrote " << a.output << "\n";
    }
    if (!a.csv.empty()) {
        hysmap::append_csv(a.csv, hysmap::metrics_csv_header(),
                           hysmap::metrics_csv_row(r, mesh, g, a.input));
    }
    return 0;
}

int cmd_compare(const Args& a) {
    if (a.input.empty()) {
        throw std::invalid_argument("compare requires --input");
    }
    const auto g = hysmap::load_network_json(a.input);
    const auto mesh = make_mesh(a);
    const auto results = hysmap::compare_mappers(g, mesh, make_cfg(a));

    std::cout.setf(std::ios::fixed);
    std::cout.precision(2);
    std::cout << "network: " << hysmap::describe_network(g) << "\n"
              << "mesh:    " << mesh.rows() << "x" << mesh.cols() << "\n\n";
    std::cout << "mapper            hops       maxL        J      edge-cut     ms\n";
    const hysmap::MapResult* edge = nullptr;
    const hysmap::MapResult* act = nullptr;
    const hysmap::MapResult* hy = nullptr;
    for (const auto& r : results) {
        std::cout.width(14);
        std::cout << std::left << r.mapper;
        std::cout << "  " << r.metrics.hops << "  " << r.metrics.max_load << "  "
                  << r.metrics.objective << "  " << r.metrics.edge_cut << "  " << r.runtime_ms
                  << "\n";
        if (r.mapper == "edge-qap") {
            edge = &r;
        }
        if (r.mapper == "activity-qap") {
            act = &r;
        }
        if (r.mapper == "hysmap") {
            hy = &r;
        }
        if (!a.csv.empty()) {
            std::ostringstream job;
            job << "n" << g.neuron_count() << "_" << mesh.rows() << "x" << mesh.cols()
                << "_" << r.mapper << "_s" << a.seed;
            hysmap::append_csv(a.csv, hysmap::metrics_csv_header(),
                               hysmap::metrics_csv_row(r, mesh, g, job.str()));
        }
    }
    if (hy && edge && act) {
        const double vs_e = 100.0 * (1.0 - hy->metrics.hops / std::max(1e-12, edge->metrics.hops));
        const double vs_a = 100.0 * (1.0 - hy->metrics.hops / std::max(1e-12, act->metrics.hops));
        std::cout << "\nHySMap hop reduction vs Edge+QAP: " << vs_e
                  << "%   vs Activity+QAP: " << vs_a << "%\n";
    }
    return 0;
}

int cmd_export(const Args& a) {
    if (a.input.empty()) {
        throw std::invalid_argument("export requires --input");
    }
    const auto g = hysmap::load_network_json(a.input);
    const auto mesh = make_mesh(a);
    const auto r = hysmap::run_mapper(g, mesh, make_cfg(a));
    const std::string fmt = a.format;
    const std::string out = a.output.empty()
                                ? (fmt == "loihi-stub" ? "loihi_stub.txt" : "loihi.json")
                                : a.output;
    if (fmt == "loihi-json" || fmt == "loihi" || fmt == "json") {
        hysmap::save_loihi_style_json(g, mesh, r.mapping, out, "hysmap");
    } else if (fmt == "loihi-stub" || fmt == "stub") {
        std::ofstream f(out);
        if (!f) {
            throw std::runtime_error("cannot write " + out);
        }
        f << hysmap::loihi_style_stub(g, mesh, r.mapping);
    } else {
        throw std::invalid_argument("unknown export format: " + fmt +
                                    " (use loihi-json or loihi-stub)");
    }
    std::cout << "wrote " << out << " (" << fmt << ", " << g.neuron_count() << " neurons, "
              << mesh.rows() << "x" << mesh.cols() << ")\n";
    return 0;
}

int cmd_demo(const Args& a) {
    hysmap::GeneratorConfig gc;
    gc.preset = hysmap::GeneratorPreset::Potjans;
    gc.target_neurons = a.neurons > 0 ? a.neurons : 80;
    gc.seed = a.seed;
    const auto g = hysmap::generate_snn(gc);
    const auto mesh = make_mesh(a);
    auto cfg = make_cfg(a);
    cfg.kind = hysmap::parse_mapper(a.mapper);
    cfg.time_incremental = true;
    const auto r = hysmap::run_mapper(g, mesh, cfg);
    std::cout << "HySMap demo  (seed=" << a.seed << ")\n";
    print_metrics(r, mesh, g);
    return 0;
}

int cmd_bench(const Args& a) {
    const std::string out = a.output.empty() ? "results/bench.csv" : a.output;
    const int seeds = a.full ? 5 : a.bench_seeds;
    const std::vector<int> meshes = a.full ? std::vector<int>{4, 5, 6, 7} : a.meshes;
    const std::vector<int> sizes = a.full ? std::vector<int>{80, 108, 160} : std::vector<int>{80, 108};

    std::cout << "HySMap benchmark  seeds=" << seeds << (a.full ? " (full)" : " (quick)") << "\n";
    for (int n : sizes) {
        for (int m : meshes) {
            if (m == 7 && n > 108) {
                continue;
            }
            for (int s = 0; s < seeds; ++s) {
                hysmap::GeneratorConfig gc;
                gc.preset = hysmap::GeneratorPreset::Potjans;
                gc.target_neurons = n;
                gc.seed = a.seed + static_cast<std::uint64_t>(s) * 17 + static_cast<std::uint64_t>(n);
                const auto g = hysmap::generate_snn(gc);
                const hysmap::MeshNoC mesh(m, m);
                hysmap::MapperConfig cfg = make_cfg(a);
                cfg.time_incremental = true;
                cfg.seed = gc.seed;
                if (m >= 7) {
                    cfg.hyper_passes = 3;
                    cfg.placement_restarts = 3;
                    cfg.joint_cycles = 1;
                }
                const auto results = hysmap::compare_mappers(g, mesh, cfg);
                std::ostringstream job;
                job << "n" << n << "_m" << m << "x" << m << "_s" << gc.seed;
                std::cout << job.str() << "\n";
                for (const auto& r : results) {
                    std::cout << "  " << r.mapper << "  hops=" << r.metrics.hops
                              << "  J=" << r.metrics.objective << "  " << r.runtime_ms << "ms\n";
                    hysmap::append_csv(out, hysmap::metrics_csv_header(),
                                       hysmap::metrics_csv_row(r, mesh, g, job.str()));
                }
            }
        }
    }
    std::cout << "wrote " << out << "\n";
    return 0;
}

Args parse(int argc, char** argv) {
    Args a;
    if (argc < 2) {
        a.cmd = "help";
        return a;
    }
    a.cmd = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string f = argv[i];
        if (f == "--input" || f == "-i") {
            a.input = require(i, argc, argv, "--input");
        } else if (f == "--out" || f == "--output" || f == "-o") {
            a.output = require(i, argc, argv, "--out");
        } else if (f == "--csv") {
            a.csv = require(i, argc, argv, "--csv");
        } else if (f == "--mapper") {
            a.mapper = require(i, argc, argv, "--mapper");
        } else if (f == "--preset") {
            a.preset = require(i, argc, argv, "--preset");
        } else if (f == "--mesh") {
            a.mesh = std::stoi(require(i, argc, argv, "--mesh"));
        } else if (f == "--rows") {
            a.rows = std::stoi(require(i, argc, argv, "--rows"));
        } else if (f == "--cols") {
            a.cols = std::stoi(require(i, argc, argv, "--cols"));
        } else if (f == "--neurons") {
            a.neurons = std::stoi(require(i, argc, argv, "--neurons"));
        } else if (f == "--scale") {
            a.scale = std::stod(require(i, argc, argv, "--scale"));
        } else if (f == "--seed") {
            a.seed = static_cast<std::uint64_t>(std::stoull(require(i, argc, argv, "--seed")));
        } else if (f == "--edge-passes") {
            a.edge_passes = std::stoi(require(i, argc, argv, "--edge-passes"));
        } else if (f == "--hyper-passes") {
            a.hyper_passes = std::stoi(require(i, argc, argv, "--hyper-passes"));
        } else if (f == "--placement-restarts") {
            a.placement_restarts = std::stoi(require(i, argc, argv, "--placement-restarts"));
        } else if (f == "--joint-cycles") {
            a.joint_cycles = std::stoi(require(i, argc, argv, "--joint-cycles"));
        } else if (f == "--slack") {
            a.slack = std::stod(require(i, argc, argv, "--slack"));
        } else if (f == "--seed-strategy") {
            a.seed_strategy = require(i, argc, argv, "--seed-strategy");
        } else if (f == "--refine") {
            a.refine = require(i, argc, argv, "--refine");
        } else if (f == "--threads") {
            a.threads = std::stoi(require(i, argc, argv, "--threads"));
        } else if (f == "--format") {
            a.format = require(i, argc, argv, "--format");
        } else if (f == "--time-incremental") {
            a.time_inc = true;
        } else if (f == "--json") {
            a.json_stdout = true;
        } else if (f == "--full") {
            a.full = true;
        } else if (f == "--quick") {
            a.full = false;
            a.bench_seeds = 1;
            a.meshes = {4, 5};
            a.sizes = {80};
        } else if (f == "--help" || f == "-h") {
            a.cmd = "help";
        } else {
            throw std::invalid_argument("unknown flag: " + f);
        }
    }
    return a;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Args a = parse(argc, argv);
        if (a.cmd == "help" || a.cmd == "--help" || a.cmd == "-h") {
            usage();
            return 0;
        }
        if (a.cmd == "version" || a.cmd == "--version") {
            std::cout << "hysmap " << hysmap::version() << "\n";
            return 0;
        }
        if (a.cmd == "generate") {
            return cmd_generate(a);
        }
        if (a.cmd == "map") {
            return cmd_map(a);
        }
        if (a.cmd == "compare") {
            return cmd_compare(a);
        }
        if (a.cmd == "export") {
            return cmd_export(a);
        }
        if (a.cmd == "demo") {
            return cmd_demo(a);
        }
        if (a.cmd == "bench") {
            return cmd_bench(a);
        }
        usage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "hysmap: " << ex.what() << "\n";
        return 1;
    }
}
