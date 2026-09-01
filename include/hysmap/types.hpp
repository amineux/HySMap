#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace hysmap {

using NeuronId = std::uint32_t;
using CoreId = std::uint32_t;
using LinkId = std::uint32_t;
using HyperedgeId = std::uint32_t;

inline constexpr NeuronId kInvalidNeuron = std::numeric_limits<NeuronId>::max();
inline constexpr CoreId kInvalidCore = std::numeric_limits<CoreId>::max();
inline constexpr LinkId kInvalidLink = std::numeric_limits<LinkId>::max();
inline constexpr HyperedgeId kInvalidHyperedge = std::numeric_limits<HyperedgeId>::max();

/// Weights of the route-aware multicast objective
/// J = α C_hop + β max_ℓ L_ℓ + γ Var(L) + δs I_s + δr I_r
/// Defaults match the M-HySMap preprint experimental setting.
struct ObjectiveWeights {
    double alpha = 1.0;       ///< routed multicast hops
    double beta = 0.10;       ///< worst-link congestion
    double gamma = 0.01;      ///< load variance
    double delta_size = 0.0;  ///< optional size imbalance (off by default)
    double delta_rate = 0.0;  ///< optional activity imbalance (off by default)
};

/// Partition p : neuron → logical core, placement π : logical core → mesh core.
struct Mapping {
    std::vector<CoreId> partition;
    std::vector<CoreId> placement;

    [[nodiscard]] CoreId physical_core(NeuronId v) const {
        return placement[partition[v]];
    }

    [[nodiscard]] std::size_t neuron_count() const { return partition.size(); }
    [[nodiscard]] std::size_t core_count() const { return placement.size(); }
};

struct CostBreakdown {
    double hops = 0.0;            ///< C_hop = Σ_u r_u |T_u|
    double max_load = 0.0;        ///< max_ℓ L_ℓ
    double load_variance = 0.0;   ///< Var_ℓ(L_ℓ)
    double size_imbalance = 0.0;
    double rate_imbalance = 0.0;
    double objective = 0.0;       ///< J
    double edge_cut = 0.0;        ///< unweighted remote synapses
    double activity_cut = 0.0;    ///< activity-weighted remote synapses
    double remote_fanout = 0.0;   ///< F_remote = Σ_u r_u |D_u|
    double lower_bound = 0.0;     ///< conservative placement bound for fixed p
    int remote_dest_cores = 0;    ///< Σ_u |D_u|
    int used_links = 0;
};

struct IncrementalTiming {
    double full_ms = 0.0;
    double incremental_ms = 0.0;
    double speedup = 0.0;
    double avg_affected = 0.0;
    double max_abs_error = 0.0;
    std::uint64_t evaluations = 0;
};

enum class MapperKind {
    EdgeQap,
    ActivityQap,
    Spectral,
    HySMapSeeded,
    HySMap
};

[[nodiscard]] inline const char* mapper_name(MapperKind k) {
    switch (k) {
        case MapperKind::EdgeQap: return "edge-qap";
        case MapperKind::ActivityQap: return "activity-qap";
        case MapperKind::Spectral: return "spectral";
        case MapperKind::HySMapSeeded: return "hysmap-seeded";
        case MapperKind::HySMap: return "hysmap";
    }
    return "unknown";
}

struct MapperConfig {
    MapperKind kind = MapperKind::HySMap;
    int edge_passes = 2;
    int hyper_passes = 4;
    int placement_restarts = 5;
    int joint_cycles = 2;
    double slack = 0.15;
    ObjectiveWeights weights{};
    std::uint64_t seed = 1;
    bool time_incremental = false;
    int capacity = 0;  ///< 0 = derive from slack and n/k
};

struct MapResult {
    Mapping mapping;
    CostBreakdown metrics;
    std::string mapper;
    double runtime_ms = 0.0;
    IncrementalTiming timing{};
    bool has_timing = false;
};

}  // namespace hysmap
