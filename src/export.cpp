#include "hysmap/export.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace hysmap {

std::string loihi_style_json(const DirectedHypergraph& g, const MeshNoC& mesh,
                             const Mapping& mapping, const std::string& name) {
    std::ostringstream o;
    o << std::setprecision(17);
    o << "{\n"
      << "  \"format\": \"hysmap-loihi-style\",\n"
      << "  \"format_version\": 1,\n"
      << "  \"disclaimer\": \"Research export inspired by Intel Loihi manycore "
         "layout (cores, axons, compartments). Not an official Intel NxSDK or Lava "
         "artifact and not claimed to compile against a specific SDK revision.\",\n"
      << "  \"name\": \"" << name << "\",\n"
      << "  \"mesh\": {\"rows\": " << mesh.rows() << ", \"cols\": " << mesh.cols()
      << ", \"cores\": " << mesh.core_count() << "},\n"
      << "  \"neuron_count\": " << g.neuron_count() << ",\n"
      << "  \"cores\": [\n";

    const int k = mesh.core_count();
    std::vector<std::vector<NeuronId>> by_core(static_cast<std::size_t>(k));
    for (NeuronId v = 0; v < g.neuron_count(); ++v) {
        by_core[mapping.physical_core(v)].push_back(v);
    }

    for (int c = 0; c < k; ++c) {
        const Coord xy = mesh.coord(static_cast<CoreId>(c));
        o << "    {\n"
          << "      \"core_id\": " << c << ",\n"
          << "      \"x\": " << xy.x << ", \"y\": " << xy.y << ",\n"
          << "      \"role\": \"neurocore\",\n"
          << "      \"neurons\": [\n";
        const auto& members = by_core[static_cast<std::size_t>(c)];
        for (std::size_t i = 0; i < members.size(); ++i) {
            const NeuronId v = members[i];
            o << "        {\"id\": " << v << ", \"label\": \"" << g.label(v)
              << "\", \"compartment\": \"soma\", \"rate_hz\": " << g.activity(v)
              << ", \"fanout\": [";
            const auto& dests = g.successors(v);
            for (std::size_t d = 0; d < dests.size(); ++d) {
                if (d) {
                    o << ", ";
                }
                o << dests[d];
            }
            o << "], \"dest_cores\": [";
            std::vector<CoreId> dcores;
            for (NeuronId t : dests) {
                dcores.push_back(mapping.physical_core(t));
            }
            std::sort(dcores.begin(), dcores.end());
            dcores.erase(std::unique(dcores.begin(), dcores.end()), dcores.end());
            for (std::size_t d = 0; d < dcores.size(); ++d) {
                if (d) {
                    o << ", ";
                }
                o << dcores[d];
            }
            o << "]}";
            o << (i + 1 == members.size() ? "\n" : ",\n");
        }
        o << "      ]\n    }";
        o << (c + 1 == k ? "\n" : ",\n");
    }
    o << "  ]\n}\n";
    return o.str();
}

void save_loihi_style_json(const DirectedHypergraph& g, const MeshNoC& mesh,
                           const Mapping& mapping, const std::string& path,
                           const std::string& name) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write " + path);
    }
    out << loihi_style_json(g, mesh, mapping, name);
}

std::string loihi_style_stub(const DirectedHypergraph& g, const MeshNoC& mesh,
                             const Mapping& mapping) {
    std::ostringstream o;
    o << "# HySMap Loihi-style listing (research stub, not NxSDK)\n";
    o << "# mesh " << mesh.rows() << "x" << mesh.cols() << "\n";
    const int k = mesh.core_count();
    std::vector<std::vector<NeuronId>> by_core(static_cast<std::size_t>(k));
    for (NeuronId v = 0; v < g.neuron_count(); ++v) {
        by_core[mapping.physical_core(v)].push_back(v);
    }
    for (int c = 0; c < k; ++c) {
        const Coord xy = mesh.coord(static_cast<CoreId>(c));
        o << "CORE " << c << " (" << xy.x << "," << xy.y << ")\n";
        for (NeuronId v : by_core[static_cast<std::size_t>(c)]) {
            o << "  NEURON " << v << " rate=" << g.activity(v) << " axon=[";
            const auto& dests = g.successors(v);
            for (std::size_t i = 0; i < dests.size(); ++i) {
                if (i) {
                    o << ",";
                }
                o << dests[i];
            }
            o << "]\n";
        }
    }
    return o.str();
}

}  // namespace hysmap
