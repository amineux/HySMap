#include "hysmap/io.hpp"

#include "hysmap/version.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace hysmap {
namespace {

std::string escape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') {
            o.push_back('\\');
        }
        o.push_back(c);
    }
    return o;
}

std::string slurp(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

class JsonParser {
public:
    explicit JsonParser(std::string t) : text_(std::move(t)) {}

    void skip() {
        while (i_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[i_]))) {
            ++i_;
        }
    }

    void expect(char c) {
        skip();
        if (i_ >= text_.size() || text_[i_] != c) {
            throw std::runtime_error(std::string("expected '") + c + "'");
        }
        ++i_;
    }

    bool try_consume(char c) {
        skip();
        if (i_ < text_.size() && text_[i_] == c) {
            ++i_;
            return true;
        }
        return false;
    }

    std::string parse_string() {
        skip();
        expect('"');
        std::string s;
        while (i_ < text_.size() && text_[i_] != '"') {
            if (text_[i_] == '\\' && i_ + 1 < text_.size()) {
                ++i_;
            }
            s.push_back(text_[i_++]);
        }
        expect('"');
        return s;
    }

    double parse_number() {
        skip();
        const std::size_t start = i_;
        if (i_ < text_.size() && (text_[i_] == '-' || text_[i_] == '+')) {
            ++i_;
        }
        while (i_ < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[i_])) || text_[i_] == '.' ||
                text_[i_] == 'e' || text_[i_] == 'E' || text_[i_] == '+' || text_[i_] == '-')) {
            ++i_;
        }
        return std::stod(text_.substr(start, i_ - start));
    }

    void skip_value() {
        skip();
        if (i_ >= text_.size()) {
            return;
        }
        if (text_[i_] == '{') {
            expect('{');
            if (!try_consume('}')) {
                while (true) {
                    parse_string();
                    expect(':');
                    skip_value();
                    if (try_consume('}')) {
                        break;
                    }
                    expect(',');
                }
            }
            return;
        }
        if (text_[i_] == '[') {
            expect('[');
            if (!try_consume(']')) {
                while (true) {
                    skip_value();
                    if (try_consume(']')) {
                        break;
                    }
                    expect(',');
                }
            }
            return;
        }
        if (text_[i_] == '"') {
            parse_string();
            return;
        }
        if (text_.compare(i_, 4, "true") == 0) {
            i_ += 4;
            return;
        }
        if (text_.compare(i_, 5, "false") == 0) {
            i_ += 5;
            return;
        }
        if (text_.compare(i_, 4, "null") == 0) {
            i_ += 4;
            return;
        }
        parse_number();
    }

    std::string text_;
    std::size_t i_ = 0;
};

}  // namespace

void save_network_json(const DirectedHypergraph& g, const std::string& path,
                       const std::string& name) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write " + path);
    }
    out << std::setprecision(17);
    out << "{\n  \"name\": \"" << escape(name) << "\",\n  \"neurons\": [\n";
    for (NeuronId v = 0; v < g.neuron_count(); ++v) {
        out << "    {\"id\": " << v << ", \"label\": \"" << escape(g.label(v))
            << "\", \"activity\": " << g.activity(v) << "}";
        out << (v + 1 == g.neuron_count() ? "\n" : ",\n");
    }
    out << "  ],\n  \"hyperedges\": [\n";
    for (HyperedgeId e = 0; e < g.hyperedge_count(); ++e) {
        const auto& h = g.edge(e);
        out << "    {\"source\": " << h.source << ", \"activity\": " << h.activity
            << ", \"destinations\": [";
        for (std::size_t i = 0; i < h.destinations.size(); ++i) {
            if (i) {
                out << ", ";
            }
            out << h.destinations[i];
        }
        out << "]}";
        out << (e + 1 == g.hyperedge_count() ? "\n" : ",\n");
    }
    out << "  ]\n}\n";
}

DirectedHypergraph load_network_json(const std::string& path) {
    JsonParser p(slurp(path));
    DirectedHypergraph g;
    p.expect('{');
    std::vector<std::tuple<NeuronId, std::string, double>> neurons;
    struct EdgeIn {
        NeuronId src;
        double act;
        std::vector<NeuronId> dests;
    };
    std::vector<EdgeIn> edges;

    while (true) {
        const std::string key = p.parse_string();
        p.expect(':');
        if (key == "neurons") {
            p.expect('[');
            if (!p.try_consume(']')) {
                while (true) {
                    p.expect('{');
                    NeuronId id = 0;
                    std::string label;
                    double act = 1.0;
                    while (true) {
                        const std::string k = p.parse_string();
                        p.expect(':');
                        if (k == "id") {
                            id = static_cast<NeuronId>(p.parse_number());
                        } else if (k == "label") {
                            label = p.parse_string();
                        } else if (k == "activity") {
                            act = p.parse_number();
                        } else {
                            p.skip_value();
                        }
                        if (p.try_consume('}')) {
                            break;
                        }
                        p.expect(',');
                    }
                    neurons.emplace_back(id, label, act);
                    if (p.try_consume(']')) {
                        break;
                    }
                    p.expect(',');
                }
            }
        } else if (key == "hyperedges") {
            p.expect('[');
            if (!p.try_consume(']')) {
                while (true) {
                    p.expect('{');
                    EdgeIn e{};
                    e.act = 1.0;
                    while (true) {
                        const std::string k = p.parse_string();
                        p.expect(':');
                        if (k == "source") {
                            e.src = static_cast<NeuronId>(p.parse_number());
                        } else if (k == "activity") {
                            e.act = p.parse_number();
                        } else if (k == "destinations") {
                            p.expect('[');
                            if (!p.try_consume(']')) {
                                while (true) {
                                    e.dests.push_back(static_cast<NeuronId>(p.parse_number()));
                                    if (p.try_consume(']')) {
                                        break;
                                    }
                                    p.expect(',');
                                }
                            }
                        } else {
                            p.skip_value();
                        }
                        if (p.try_consume('}')) {
                            break;
                        }
                        p.expect(',');
                    }
                    edges.push_back(std::move(e));
                    if (p.try_consume(']')) {
                        break;
                    }
                    p.expect(',');
                }
            }
        } else {
            p.skip_value();
        }
        if (p.try_consume('}')) {
            break;
        }
        p.expect(',');
    }

    NeuronId max_id = 0;
    for (const auto& [id, _, __] : neurons) {
        max_id = std::max(max_id, id);
    }
    const std::size_t n = neurons.empty() ? 0 : static_cast<std::size_t>(max_id) + 1;
    std::vector<std::string> labels(n);
    std::vector<double> acts(n, 1.0);
    for (const auto& [id, lab, act] : neurons) {
        labels[id] = lab;
        acts[id] = act;
    }
    g.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        g.add_neuron(labels[i], acts[i]);
    }
    for (const auto& e : edges) {
        g.add_hyperedge(e.src, e.dests, e.act);
    }
    return g;
}

std::string metrics_json(const MapResult& r, const MeshNoC& mesh,
                         const DirectedHypergraph& g) {
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(6);
    o << "{\n"
      << "  \"hysmap_version\": \"" << version() << "\",\n"
      << "  \"mapper\": \"" << r.mapper << "\",\n"
      << "  \"runtime_ms\": " << r.runtime_ms << ",\n"
      << "  \"network\": {\"neurons\": " << g.neuron_count()
      << ", \"synapses\": " << g.synapse_count()
      << ", \"hyperedges\": " << g.hyperedge_count() << "},\n"
      << "  \"mesh\": {\"rows\": " << mesh.rows() << ", \"cols\": " << mesh.cols()
      << ", \"cores\": " << mesh.core_count() << ", \"links\": " << mesh.link_count() << "},\n"
      << "  \"metrics\": {\n"
      << "    \"multicast_hops\": " << r.metrics.hops << ",\n"
      << "    \"max_link_load\": " << r.metrics.max_load << ",\n"
      << "    \"load_variance\": " << r.metrics.load_variance << ",\n"
      << "    \"objective\": " << r.metrics.objective << ",\n"
      << "    \"edge_cut\": " << r.metrics.edge_cut << ",\n"
      << "    \"activity_cut\": " << r.metrics.activity_cut << ",\n"
      << "    \"remote_fanout\": " << r.metrics.remote_fanout << ",\n"
      << "    \"lower_bound\": " << r.metrics.lower_bound << ",\n"
      << "    \"remote_dest_cores\": " << r.metrics.remote_dest_cores << "\n"
      << "  }";
    if (r.has_timing) {
        o << ",\n  \"incremental\": {\n"
          << "    \"full_ms\": " << r.timing.full_ms << ",\n"
          << "    \"incremental_ms\": " << r.timing.incremental_ms << ",\n"
          << "    \"speedup\": " << r.timing.speedup << ",\n"
          << "    \"avg_affected\": " << r.timing.avg_affected << ",\n"
          << "    \"max_abs_error\": " << r.timing.max_abs_error << ",\n"
          << "    \"evaluations\": " << r.timing.evaluations << "\n"
          << "  }";
    }
    o << ",\n  \"assignment\": [";
    for (std::size_t i = 0; i < r.mapping.partition.size(); ++i) {
        if (i) {
            o << ", ";
        }
        o << r.mapping.physical_core(static_cast<NeuronId>(i));
    }
    o << "]\n}\n";
    return o.str();
}

void save_result_json(const MapResult& r, const MeshNoC& mesh, const DirectedHypergraph& g,
                      const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write " + path);
    }
    out << metrics_json(r, mesh, g);
}

std::string metrics_csv_header() {
    return "job,mapper,rows,cols,neurons,synapses,hops,max_load,variance,objective,"
           "edge_cut,activity_cut,remote_fanout,lower_bound,runtime_ms,inc_speedup,"
           "avg_affected,max_abs_error";
}

std::string metrics_csv_row(const MapResult& r, const MeshNoC& mesh,
                            const DirectedHypergraph& g, const std::string& job) {
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(6);
    o << job << "," << r.mapper << "," << mesh.rows() << "," << mesh.cols() << ","
      << g.neuron_count() << "," << g.synapse_count() << "," << r.metrics.hops << ","
      << r.metrics.max_load << "," << r.metrics.load_variance << "," << r.metrics.objective
      << "," << r.metrics.edge_cut << "," << r.metrics.activity_cut << ","
      << r.metrics.remote_fanout << "," << r.metrics.lower_bound << "," << r.runtime_ms << ","
      << (r.has_timing ? r.timing.speedup : 0.0) << ","
      << (r.has_timing ? r.timing.avg_affected : 0.0) << ","
      << (r.has_timing ? r.timing.max_abs_error : 0.0);
    return o.str();
}

void append_csv(const std::string& path, const std::string& header, const std::string& row) {
    std::ifstream probe(path);
    const bool fresh = !probe.good() || probe.peek() == std::ifstream::traits_type::eof();
    probe.close();
    std::ofstream out(path, std::ios::app);
    if (!out) {
        throw std::runtime_error("cannot write " + path);
    }
    if (fresh) {
        out << header << "\n";
    }
    out << row << "\n";
}

}  // namespace hysmap
