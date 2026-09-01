#pragma once

#include "hysmap/hypergraph.hpp"

#include <cstdint>
#include <string>

namespace hysmap {

enum class GeneratorPreset {
    Potjans,   ///< layered recurrent E/I microcircuit (Potjans–Diesmann inspired)
    Layered,   ///< feed-forward + weak recurrent skip
    Random     ///< Erdős–Rényi directed graph lifted to hyperedges
};

struct GeneratorConfig {
    GeneratorPreset preset = GeneratorPreset::Potjans;
    double scale = 0.002;          ///< population scale vs a ~77k-neuron microcircuit
    int target_neurons = 0;        ///< if >0, overrides scale to hit this size
    std::uint64_t seed = 1;
    double extra_recurrent = 0.02; ///< additional same-layer recurrence
};

[[nodiscard]] const char* preset_name(GeneratorPreset p);

[[nodiscard]] DirectedHypergraph generate_snn(const GeneratorConfig& cfg);

/// Human-readable summary of a generated network.
[[nodiscard]] std::string describe_network(const DirectedHypergraph& g);

}  // namespace hysmap
