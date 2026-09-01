#include "hysmap/incremental.hpp"

#include "hysmap/cost.hpp"

#include <algorithm>
#include <cmath>

namespace hysmap {

IncrementalEvaluator::IncrementalEvaluator(const DirectedHypergraph& graph,
                                           const MeshNoC& mesh, Mapping& mapping,
                                           ObjectiveWeights weights)
    : graph_(graph), mesh_(mesh), mapping_(mapping), weights_(weights) {
    rebuild();
}

void IncrementalEvaluator::rebuild() {
    cache_.assign(graph_.neuron_count(), {});
    loads_.assign(static_cast<std::size_t>(mesh_.link_count()), 0.0);

    double hops = 0.0;
    double edge_cut = 0.0;
    double activity_cut = 0.0;
    double remote_fanout = 0.0;
    int remote_dest_cores = 0;

    for (NeuronId u = 0; u < graph_.neuron_count(); ++u) {
        cache_[u] = compute_source(u, kInvalidNeuron, kInvalidCore);
        hops += cache_[u].hops;
        remote_fanout += graph_.activity(u) * static_cast<double>(cache_[u].dest_logical.size());
        remote_dest_cores += static_cast<int>(cache_[u].dest_logical.size());
        for (LinkId l : cache_[u].links) {
            loads_[l] += graph_.activity(u);
        }
        for (NeuronId d : graph_.successors(u)) {
            if (mapping_.partition[d] != mapping_.partition[u]) {
                ++edge_cut;
                activity_cut += graph_.activity(u);
            }
        }
    }

    current_ = summarize_loads(hops, loads_, edge_cut, activity_cut, remote_fanout,
                               remote_dest_cores, weights_, mapping_, graph_);
}

CoreId IncrementalEvaluator::logical_of(NeuronId v, NeuronId moved,
                                        CoreId new_logical) const {
    if (v == moved) {
        return new_logical;
    }
    return mapping_.partition[v];
}

SourceCache IncrementalEvaluator::compute_source(NeuronId u, NeuronId moved,
                                                 CoreId new_logical) const {
    SourceCache c;
    c.source_logical = (moved == kInvalidNeuron) ? mapping_.partition[u]
                                                 : logical_of(u, moved, new_logical);
    const CoreId src_phys = mapping_.placement[c.source_logical];

    c.dest_logical.reserve(graph_.successors(u).size());
    for (NeuronId d : graph_.successors(u)) {
        const CoreId dst =
            (moved == kInvalidNeuron) ? mapping_.partition[d] : logical_of(d, moved, new_logical);
        if (dst != c.source_logical) {
            c.dest_logical.push_back(dst);
        }
    }
    std::sort(c.dest_logical.begin(), c.dest_logical.end());
    c.dest_logical.erase(std::unique(c.dest_logical.begin(), c.dest_logical.end()),
                         c.dest_logical.end());

    std::vector<CoreId> dest_phys;
    dest_phys.reserve(c.dest_logical.size());
    for (CoreId log : c.dest_logical) {
        dest_phys.push_back(mapping_.placement[log]);
    }
    mesh_.multicast_union(src_phys, dest_phys, c.links);
    c.hops = graph_.activity(u) * static_cast<double>(c.links.size());
    return c;
}

SourceCache IncrementalEvaluator::compute_source_placed(
    NeuronId u, const std::vector<CoreId>& place) const {
    SourceCache c;
    c.source_logical = mapping_.partition[u];
    const CoreId src_phys = place[c.source_logical];
    for (NeuronId d : graph_.successors(u)) {
        const CoreId dst = mapping_.partition[d];
        if (dst != c.source_logical) {
            c.dest_logical.push_back(dst);
        }
    }
    std::sort(c.dest_logical.begin(), c.dest_logical.end());
    c.dest_logical.erase(std::unique(c.dest_logical.begin(), c.dest_logical.end()),
                         c.dest_logical.end());
    std::vector<CoreId> dest_phys;
    dest_phys.reserve(c.dest_logical.size());
    for (CoreId log : c.dest_logical) {
        dest_phys.push_back(place[log]);
    }
    mesh_.multicast_union(src_phys, dest_phys, c.links);
    c.hops = graph_.activity(u) * static_cast<double>(c.links.size());
    return c;
}

static void cut_stats(const DirectedHypergraph& g, const Mapping& mapping, NeuronId moved,
                      CoreId new_logical, double& edge_cut, double& activity_cut) {
    edge_cut = 0.0;
    activity_cut = 0.0;
    for (NeuronId u = 0; u < g.neuron_count(); ++u) {
        const CoreId su = (u == moved) ? new_logical : mapping.partition[u];
        for (NeuronId d : g.successors(u)) {
            const CoreId sd = (d == moved) ? new_logical : mapping.partition[d];
            if (su != sd) {
                ++edge_cut;
                activity_cut += g.activity(u);
            }
        }
    }
}

CostBreakdown IncrementalEvaluator::peek_partition_move(NeuronId v,
                                                        CoreId new_logical) const {
    const auto affected = graph_.affected_sources(v);
    auto loads = loads_;
    double hops = current_.hops;
    double remote_fanout = current_.remote_fanout;
    int remote_dest_cores = current_.remote_dest_cores;

    for (NeuronId u : affected) {
        hops -= cache_[u].hops;
        remote_fanout -= graph_.activity(u) * static_cast<double>(cache_[u].dest_logical.size());
        remote_dest_cores -= static_cast<int>(cache_[u].dest_logical.size());
        for (LinkId l : cache_[u].links) {
            loads[l] -= graph_.activity(u);
        }
    }

    for (NeuronId u : affected) {
        const SourceCache neu = compute_source(u, v, new_logical);
        hops += neu.hops;
        remote_fanout += graph_.activity(u) * static_cast<double>(neu.dest_logical.size());
        remote_dest_cores += static_cast<int>(neu.dest_logical.size());
        for (LinkId l : neu.links) {
            loads[l] += graph_.activity(u);
        }
    }

    double edge_cut = 0.0;
    double activity_cut = 0.0;
    cut_stats(graph_, mapping_, v, new_logical, edge_cut, activity_cut);

    Mapping tmp = mapping_;
    tmp.partition[v] = new_logical;
    return summarize_loads(hops, loads, edge_cut, activity_cut, remote_fanout,
                           remote_dest_cores, weights_, tmp, graph_);
}

void IncrementalEvaluator::commit_partition_move(NeuronId v, CoreId new_logical) {
    const auto affected = graph_.affected_sources(v);
    for (NeuronId u : affected) {
        for (LinkId l : cache_[u].links) {
            loads_[l] -= graph_.activity(u);
        }
    }
    mapping_.partition[v] = new_logical;
    for (NeuronId u : affected) {
        cache_[u] = compute_source(u, kInvalidNeuron, kInvalidCore);
        for (LinkId l : cache_[u].links) {
            loads_[l] += graph_.activity(u);
        }
    }

    double hops = 0.0;
    double remote_fanout = 0.0;
    int remote_dest_cores = 0;
    double edge_cut = 0.0;
    double activity_cut = 0.0;
    for (NeuronId u = 0; u < graph_.neuron_count(); ++u) {
        hops += cache_[u].hops;
        remote_fanout += graph_.activity(u) * static_cast<double>(cache_[u].dest_logical.size());
        remote_dest_cores += static_cast<int>(cache_[u].dest_logical.size());
        for (NeuronId d : graph_.successors(u)) {
            if (mapping_.partition[d] != mapping_.partition[u]) {
                ++edge_cut;
                activity_cut += graph_.activity(u);
            }
        }
    }
    current_ = summarize_loads(hops, loads_, edge_cut, activity_cut, remote_fanout,
                               remote_dest_cores, weights_, mapping_, graph_);
}

CostBreakdown IncrementalEvaluator::peek_placement_swap(CoreId a, CoreId b) const {
    auto place = mapping_.placement;
    std::swap(place[a], place[b]);

    std::vector<double> loads(loads_.size(), 0.0);
    double hops = 0.0;
    double remote_fanout = 0.0;
    int remote_dest_cores = 0;
    double edge_cut = current_.edge_cut;
    double activity_cut = current_.activity_cut;

    for (NeuronId u = 0; u < graph_.neuron_count(); ++u) {
        const SourceCache c = compute_source_placed(u, place);
        hops += c.hops;
        remote_fanout += graph_.activity(u) * static_cast<double>(c.dest_logical.size());
        remote_dest_cores += static_cast<int>(c.dest_logical.size());
        for (LinkId l : c.links) {
            loads[l] += graph_.activity(u);
        }
    }

    Mapping tmp = mapping_;
    tmp.placement = place;
    return summarize_loads(hops, loads, edge_cut, activity_cut, remote_fanout,
                           remote_dest_cores, weights_, tmp, graph_);
}

void IncrementalEvaluator::commit_placement_swap(CoreId a, CoreId b) {
    std::swap(mapping_.placement[a], mapping_.placement[b]);
    rebuild();
}

CostBreakdown IncrementalEvaluator::full_recompute() const {
    return evaluate(graph_, mesh_, mapping_, weights_);
}

}  // namespace hysmap
