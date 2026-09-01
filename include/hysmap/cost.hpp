#pragma once

#include "hysmap/hypergraph.hpp"
#include "hysmap/mesh.hpp"
#include "hysmap/types.hpp"

#include <vector>

namespace hysmap {

[[nodiscard]] std::vector<int> core_occupancies(const Mapping& m, int cores);

[[nodiscard]] bool respects_balance(const Mapping& m, double slack, int capacity);

/// Conservative placement lower bound for a *fixed* partition (see docs/algorithm.md):
/// J ≥ α F_remote + β F_remote / M + δs I_s + δr I_r
/// because any connected route union to d dest cores uses ≥ d links,
/// max load ≥ average load, and variance is nonnegative.
[[nodiscard]] double placement_lower_bound(double remote_fanout, int link_count,
                                           const ObjectiveWeights& w,
                                           double size_imbalance = 0.0,
                                           double rate_imbalance = 0.0);

[[nodiscard]] CostBreakdown evaluate(const DirectedHypergraph& g, const MeshNoC& mesh,
                                     const Mapping& mapping, const ObjectiveWeights& w);

[[nodiscard]] CostBreakdown summarize_loads(double hops, const std::vector<double>& loads,
                                            double edge_cut, double activity_cut,
                                            double remote_fanout, int remote_dest_cores,
                                            const ObjectiveWeights& w,
                                            const Mapping& mapping,
                                            const DirectedHypergraph& g);

}  // namespace hysmap
