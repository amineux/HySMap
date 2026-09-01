#include "hysmap/hypergraph.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

namespace hysmap {

NeuronId DirectedHypergraph::add_neuron(std::string label, double activity) {
    const NeuronId id = static_cast<NeuronId>(labels_.size());
    labels_.push_back(std::move(label));
    activity_.push_back(activity);
    outgoing_.push_back(kInvalidHyperedge);
    successors_.emplace_back();
    predecessors_.emplace_back();
    inbound_.emplace_back();
    return id;
}

void DirectedHypergraph::reserve(std::size_t neurons) {
    labels_.reserve(neurons);
    activity_.reserve(neurons);
    outgoing_.reserve(neurons);
    successors_.reserve(neurons);
    predecessors_.reserve(neurons);
    inbound_.reserve(neurons);
}

void DirectedHypergraph::set_activity(NeuronId v, double activity) {
    if (v >= neuron_count()) {
        throw std::out_of_range("neuron id out of range");
    }
    activity_[v] = activity;
    if (outgoing_[v] != kInvalidHyperedge) {
        edges_[outgoing_[v]].activity = activity;
    }
}

HyperedgeId DirectedHypergraph::add_hyperedge(NeuronId source,
                                              std::vector<NeuronId> destinations,
                                              double activity) {
    if (source >= neuron_count()) {
        throw std::out_of_range("source neuron out of range");
    }
    std::sort(destinations.begin(), destinations.end());
    destinations.erase(std::unique(destinations.begin(), destinations.end()),
                       destinations.end());
    destinations.erase(std::remove(destinations.begin(), destinations.end(), source),
                       destinations.end());
    for (NeuronId d : destinations) {
        if (d >= neuron_count()) {
            throw std::out_of_range("destination neuron out of range");
        }
    }

    activity_[source] = activity;

    if (outgoing_[source] != kInvalidHyperedge) {
        const HyperedgeId eid = outgoing_[source];
        edges_[eid].destinations = std::move(destinations);
        edges_[eid].activity = activity;
        successors_[source] = edges_[eid].destinations;
        rebuild_inbound();
        return eid;
    }

    const HyperedgeId eid = static_cast<HyperedgeId>(edges_.size());
    Hyperedge e;
    e.source = source;
    e.destinations = std::move(destinations);
    e.activity = activity;
    successors_[source] = e.destinations;
    outgoing_[source] = eid;
    edges_.push_back(std::move(e));

    for (NeuronId d : successors_[source]) {
        inbound_[d].push_back(eid);
        predecessors_[d].push_back(source);
    }
    return eid;
}

void DirectedHypergraph::rebuild_inbound() {
    for (auto& v : inbound_) {
        v.clear();
    }
    for (auto& v : predecessors_) {
        v.clear();
    }
    for (HyperedgeId e = 0; e < edges_.size(); ++e) {
        for (NeuronId d : edges_[e].destinations) {
            inbound_[d].push_back(e);
            predecessors_[d].push_back(edges_[e].source);
        }
    }
}

std::size_t DirectedHypergraph::synapse_count() const {
    std::size_t n = 0;
    for (const auto& e : edges_) {
        n += e.destinations.size();
    }
    return n;
}

double DirectedHypergraph::mean_fanout() const {
    if (edges_.empty()) {
        return 0.0;
    }
    return static_cast<double>(synapse_count()) / static_cast<double>(edges_.size());
}

std::vector<NeuronId> DirectedHypergraph::affected_sources(NeuronId v) const {
    std::vector<NeuronId> a = predecessors_[v];
    a.push_back(v);
    std::sort(a.begin(), a.end());
    a.erase(std::unique(a.begin(), a.end()), a.end());
    return a;
}

}  // namespace hysmap
