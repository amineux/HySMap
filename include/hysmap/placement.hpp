#pragma once

#include "hysmap/hypergraph.hpp"
#include "hysmap/incremental.hpp"
#include "hysmap/mesh.hpp"
#include "hysmap/types.hpp"

#include <random>
#include <vector>

namespace hysmap {

/// Pairwise flow matrix F_ij from the current partition.
[[nodiscard]] std::vector<double> pairwise_flow(const DirectedHypergraph& g,
                                                const Mapping& mapping, int cores,
                                                bool activity_weighted);

double qap_cost(const std::vector<double>& flow, const MeshNoC& mesh,
                const std::vector<CoreId>& placement);

/// Multi-start greedy 2-swap under the pairwise QAP objective.
void place_qap(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& mapping,
               bool activity_weighted, std::mt19937_64& rng, int restarts);

/// Force-directed neighbor swaps (pairwise Manhattan potential).
void place_force(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& mapping,
                 bool activity_weighted, int max_iters);

/// Greedy min-distance constructive placement.
void place_min_distance(const DirectedHypergraph& g, const MeshNoC& mesh,
                        Mapping& mapping, bool activity_weighted);

/// Hypergraph-Laplacian (Fiedler-style) embedding of logical cores, discretized
/// onto the mesh. Inspired by Ronzani & Silvano (arXiv:2601.16118).
void place_spectral(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& mapping);

/// Spectral embedding of *neurons* quantized onto cores (capacity-aware seed).
void seed_spectral_partition(const DirectedHypergraph& g, const MeshNoC& mesh,
                             Mapping& mapping, int capacity);

/// 2-swap placement search under the multicast objective J.
int refine_placement_multicast(const MeshNoC& mesh, Mapping& mapping,
                               IncrementalEvaluator& eval, std::mt19937_64& rng,
                               int restarts, int threads = 1);

}  // namespace hysmap
