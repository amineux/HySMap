#pragma once

#include "hysmap/hypergraph.hpp"
#include "hysmap/mesh.hpp"
#include "hysmap/types.hpp"

#include <string>

namespace hysmap {

/// Loihi-inspired research export (JSON). Documents cores as axon/compartment
/// groups with fanout lists and mesh coordinates. This is **not** an official
/// Intel NxSDK / Lava artifact.
[[nodiscard]] std::string loihi_style_json(const DirectedHypergraph& g, const MeshNoC& mesh,
                                           const Mapping& mapping,
                                           const std::string& name = "hysmap");

void save_loihi_style_json(const DirectedHypergraph& g, const MeshNoC& mesh,
                           const Mapping& mapping, const std::string& path,
                           const std::string& name = "hysmap");

/// Human-readable stub in the spirit of an NxNet listing (research only).
[[nodiscard]] std::string loihi_style_stub(const DirectedHypergraph& g, const MeshNoC& mesh,
                                           const Mapping& mapping);

}  // namespace hysmap
