#include "hysmap/cost.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace hysmap {

std::vector<int> core_occupancies(const Mapping& m, int cores) {
    std::vector<int> occ(static_cast<std::size_t>(cores), 0);
    for (CoreId c : m.partition) {
        if (static_cast<int>(c) < cores) {
            ++occ[c];
        }
    }
    return occ;
}

bool respects_balance(const Mapping& m, double slack, int capacity) {
    const int n = static_cast<int>(m.partition.size());
    const int k = static_cast<int>(m.placement.size());
    if (k == 0) {
        return false;
    }
    const double fair = static_cast<double>(n) / static_cast<double>(k);
    const int lo = static_cast<int>(std::floor((1.0 - slack) * fair));
    int hi = static_cast<int>(std::ceil((1.0 + slack) * fair));
    hi = std::min(hi, capacity);
    const auto occ = core_occupancies(m, k);
    for (int c : occ) {
        if (c < lo || c > hi) {
            return false;
        }
    }
    return true;
}

double placement_lower_bound(double remote_fanout, int link_count,
                             const ObjectiveWeights& w, double size_imbalance,
                             double rate_imbalance) {
    const double m = std::max(1, link_count);
    return w.alpha * remote_fanout + w.beta * (remote_fanout / m) +
           w.delta_size * size_imbalance + w.delta_rate * rate_imbalance;
}

static void imbalance_terms(const Mapping& mapping, const DirectedHypergraph& g,
                            double& size_imbalance, double& rate_imbalance) {
    const int k = static_cast<int>(mapping.placement.size());
    const int n = static_cast<int>(g.neuron_count());
    std::vector<int> occ(static_cast<std::size_t>(k), 0);
    std::vector<double> rate(static_cast<std::size_t>(k), 0.0);
    double total_rate = 0.0;
    for (NeuronId v = 0; v < g.neuron_count(); ++v) {
        const CoreId c = mapping.partition[v];
        ++occ[c];
        rate[c] += g.activity(v);
        total_rate += g.activity(v);
    }
    const double fair_n = static_cast<double>(n) / static_cast<double>(k);
    const double fair_r = total_rate / static_cast<double>(k);
    size_imbalance = 0.0;
    rate_imbalance = 0.0;
    for (int i = 0; i < k; ++i) {
        size_imbalance += std::abs(static_cast<double>(occ[static_cast<std::size_t>(i)]) - fair_n);
        rate_imbalance += std::abs(rate[static_cast<std::size_t>(i)] - fair_r);
    }
}

CostBreakdown summarize_loads(double hops, const std::vector<double>& loads,
                              double edge_cut, double activity_cut,
                              double remote_fanout, int remote_dest_cores,
                              const ObjectiveWeights& w, const Mapping& mapping,
                              const DirectedHypergraph& g) {
    CostBreakdown out;
    out.hops = hops;
    out.edge_cut = edge_cut;
    out.activity_cut = activity_cut;
    out.remote_fanout = remote_fanout;
    out.remote_dest_cores = remote_dest_cores;

    double max_l = 0.0;
    double sum = 0.0;
    int used = 0;
    for (double l : loads) {
        max_l = std::max(max_l, l);
        sum += l;
        if (l > 0.0) {
            ++used;
        }
    }
    out.max_load = max_l;
    out.used_links = used;
    const double m = static_cast<double>(std::max<std::size_t>(1, loads.size()));
    const double mean = sum / m;
    double var = 0.0;
    for (double l : loads) {
        const double d = l - mean;
        var += d * d;
    }
    out.load_variance = var / m;

    imbalance_terms(mapping, g, out.size_imbalance, out.rate_imbalance);
    out.objective = w.alpha * out.hops + w.beta * out.max_load + w.gamma * out.load_variance +
                    w.delta_size * out.size_imbalance + w.delta_rate * out.rate_imbalance;
    out.lower_bound = placement_lower_bound(out.remote_fanout, static_cast<int>(loads.size()),
                                            w, out.size_imbalance, out.rate_imbalance);
    return out;
}

CostBreakdown evaluate(const DirectedHypergraph& g, const MeshNoC& mesh,
                       const Mapping& mapping, const ObjectiveWeights& w) {
    std::vector<double> loads(static_cast<std::size_t>(mesh.link_count()), 0.0);
    std::vector<LinkId> links;
    links.reserve(static_cast<std::size_t>(mesh.rows() + mesh.cols()));

    double hops = 0.0;
    double edge_cut = 0.0;
    double activity_cut = 0.0;
    double remote_fanout = 0.0;
    int remote_dest_cores = 0;

    std::vector<CoreId> dests;
    dests.reserve(16);

    for (NeuronId u = 0; u < g.neuron_count(); ++u) {
        const CoreId src_log = mapping.partition[u];
        const CoreId src_phys = mapping.placement[src_log];
        dests.clear();
        for (NeuronId d : g.successors(u)) {
            const CoreId dst_log = mapping.partition[d];
            if (dst_log != src_log) {
                dests.push_back(dst_log);
                ++edge_cut;
                activity_cut += g.activity(u);
            }
        }
        std::sort(dests.begin(), dests.end());
        dests.erase(std::unique(dests.begin(), dests.end()), dests.end());

        std::vector<CoreId> dest_phys;
        dest_phys.reserve(dests.size());
        for (CoreId c : dests) {
            dest_phys.push_back(mapping.placement[c]);
        }
        mesh.multicast_union(src_phys, dest_phys, links);

        const double r = g.activity(u);
        hops += r * static_cast<double>(links.size());
        remote_fanout += r * static_cast<double>(dests.size());
        remote_dest_cores += static_cast<int>(dests.size());
        for (LinkId l : links) {
            loads[l] += r;
        }
    }

    return summarize_loads(hops, loads, edge_cut, activity_cut, remote_fanout,
                           remote_dest_cores, w, mapping, g);
}

}  // namespace hysmap
