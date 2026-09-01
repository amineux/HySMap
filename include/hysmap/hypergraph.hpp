#pragma once

#include "hysmap/types.hpp"

#include <string>
#include <vector>

namespace hysmap {

/// Source-rooted directed hyperedge: one axon / spike event.
/// Destinations are the postsynaptic fanout; `activity` is the source rate r_u.
struct Hyperedge {
    NeuronId source = kInvalidNeuron;
    std::vector<NeuronId> destinations;
    double activity = 1.0;
};

/// Activity-weighted directed hypergraph of an SNN.
/// Each neuron owns at most one outgoing hyperedge (its axon).
class DirectedHypergraph {
public:
    NeuronId add_neuron(std::string label = {}, double activity = 1.0);

    /// Adds or replaces the axon of `source`. Destinations are uniqued.
    /// Self-loops are dropped. Activity also updates the source neuron rate.
    HyperedgeId add_hyperedge(NeuronId source, std::vector<NeuronId> destinations,
                              double activity);

    void set_activity(NeuronId v, double activity);

    [[nodiscard]] std::size_t neuron_count() const { return labels_.size(); }
    [[nodiscard]] std::size_t hyperedge_count() const { return edges_.size(); }

    [[nodiscard]] const std::string& label(NeuronId v) const { return labels_[v]; }
    [[nodiscard]] double activity(NeuronId v) const { return activity_[v]; }

    [[nodiscard]] const Hyperedge& edge(HyperedgeId e) const { return edges_[e]; }
    [[nodiscard]] HyperedgeId outgoing_id(NeuronId v) const { return outgoing_[v]; }
    [[nodiscard]] bool has_outgoing(NeuronId v) const {
        return outgoing_[v] != kInvalidHyperedge;
    }

    [[nodiscard]] const std::vector<NeuronId>& successors(NeuronId v) const {
        return successors_[v];
    }
    [[nodiscard]] const std::vector<NeuronId>& predecessors(NeuronId v) const {
        return predecessors_[v];
    }
    [[nodiscard]] const std::vector<HyperedgeId>& inbound(NeuronId v) const {
        return inbound_[v];
    }

    [[nodiscard]] std::size_t synapse_count() const;
    [[nodiscard]] double mean_fanout() const;

    /// Neurons u such that a move of v can change T_u: {v} ∪ N⁻(v).
    [[nodiscard]] std::vector<NeuronId> affected_sources(NeuronId v) const;

    void reserve(std::size_t neurons);

private:
    void rebuild_inbound();

    std::vector<std::string> labels_;
    std::vector<double> activity_;
    std::vector<Hyperedge> edges_;
    std::vector<HyperedgeId> outgoing_;
    std::vector<std::vector<NeuronId>> successors_;
    std::vector<std::vector<NeuronId>> predecessors_;
    std::vector<std::vector<HyperedgeId>> inbound_;
};

}  // namespace hysmap
