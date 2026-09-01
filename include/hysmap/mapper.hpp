#pragma once

#include "hysmap/hypergraph.hpp"
#include "hysmap/mesh.hpp"
#include "hysmap/types.hpp"

#include <string>
#include <vector>

namespace hysmap {

[[nodiscard]] Mapping make_identity_mapping(int neurons, int cores);

[[nodiscard]] MapResult run_mapper(const DirectedHypergraph& g, const MeshNoC& mesh,
                                   const MapperConfig& cfg);

/// Run Edge+QAP, Activity+QAP, Spectral, HySMap-seeded, and full HySMap.
[[nodiscard]] std::vector<MapResult> compare_mappers(const DirectedHypergraph& g,
                                                     const MeshNoC& mesh,
                                                     const MapperConfig& base);

[[nodiscard]] MapperKind parse_mapper(const std::string& name);

}  // namespace hysmap
