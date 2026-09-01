#pragma once

#include "hysmap/hypergraph.hpp"
#include "hysmap/mapper.hpp"
#include "hysmap/mesh.hpp"
#include "hysmap/types.hpp"

#include <string>
#include <vector>

namespace hysmap {

void save_network_json(const DirectedHypergraph& g, const std::string& path,
                       const std::string& name = "network");

[[nodiscard]] DirectedHypergraph load_network_json(const std::string& path);

[[nodiscard]] std::string metrics_json(const MapResult& r, const MeshNoC& mesh,
                                       const DirectedHypergraph& g);

void save_result_json(const MapResult& r, const MeshNoC& mesh,
                      const DirectedHypergraph& g, const std::string& path);

[[nodiscard]] std::string metrics_csv_header();
[[nodiscard]] std::string metrics_csv_row(const MapResult& r, const MeshNoC& mesh,
                                          const DirectedHypergraph& g,
                                          const std::string& job = "");

void append_csv(const std::string& path, const std::string& header,
                const std::string& row);

}  // namespace hysmap
