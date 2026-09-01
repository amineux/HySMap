#pragma once

#include "hysmap/hypergraph.hpp"
#include "hysmap/mesh.hpp"
#include "hysmap/types.hpp"

#include <vector>

namespace hysmap {

/// Cached per-source multicast contribution. Moving neuron v invalidates
/// only sources in A(v) = {v} ∪ N⁻(v); every other cache entry stays exact.
struct SourceCache {
    CoreId source_logical = kInvalidCore;
    std::vector<CoreId> dest_logical;  ///< distinct remote destination cores
    std::vector<LinkId> links;         ///< union of XY routes
    double hops = 0.0;
};

class IncrementalEvaluator {
public:
    IncrementalEvaluator(const DirectedHypergraph& graph, const MeshNoC& mesh,
                         Mapping& mapping, ObjectiveWeights weights);

    void rebuild();

    [[nodiscard]] const CostBreakdown& current() const { return current_; }
    [[nodiscard]] const std::vector<double>& link_loads() const { return loads_; }

    [[nodiscard]] CostBreakdown peek_partition_move(NeuronId v, CoreId new_logical) const;
    void commit_partition_move(NeuronId v, CoreId new_logical);

    [[nodiscard]] CostBreakdown peek_placement_swap(CoreId a, CoreId b) const;
    void commit_placement_swap(CoreId a, CoreId b);

    /// Independent full recomputation (used to validate incremental exactness).
    [[nodiscard]] CostBreakdown full_recompute() const;

    [[nodiscard]] const SourceCache& cache(NeuronId u) const { return cache_[u]; }

private:
    [[nodiscard]] CoreId logical_of(NeuronId v, NeuronId moved, CoreId new_logical) const;
    [[nodiscard]] SourceCache compute_source(NeuronId u, NeuronId moved,
                                             CoreId new_logical) const;
    [[nodiscard]] SourceCache compute_source_placed(NeuronId u,
                                                    const std::vector<CoreId>& place) const;

    const DirectedHypergraph& graph_;
    const MeshNoC& mesh_;
    Mapping& mapping_;
    ObjectiveWeights weights_;
    std::vector<SourceCache> cache_;
    std::vector<double> loads_;
    CostBreakdown current_{};
};

}  // namespace hysmap
