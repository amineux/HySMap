#include "hysmap/snn_generator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <sstream>

namespace hysmap {

const char* preset_name(GeneratorPreset p) {
    switch (p) {
        case GeneratorPreset::Potjans: return "potjans";
        case GeneratorPreset::Layered: return "layered";
        case GeneratorPreset::Random: return "random";
    }
    return "unknown";
}

namespace {

struct Population {
    const char* name;
    double fraction;
    double base_rate;  ///< synthetic mean spike rate (Hz)
    bool excitatory;
};

// Relative sizes follow Potjans & Diesmann, Cerebral Cortex 24:785–806 (2014).
constexpr std::array<Population, 8> kPotjans = {{
    {"L23e", 0.2681, 5.5, true},
    {"L23i", 0.0756, 11.0, false},
    {"L4e", 0.2840, 8.5, true},
    {"L4i", 0.0710, 14.0, false},
    {"L5e", 0.0629, 7.0, true},
    {"L5i", 0.0138, 12.0, false},
    {"L6e", 0.1865, 3.5, true},
    {"L6i", 0.0382, 9.0, false},
}};

// Connection probabilities P(from → to). Potjans-inspired (Table 5 style).
constexpr double kP[8][8] = {
    // to: 23e    23i    4e     4i     5e     5i     6e     6i
    {0.101, 0.135, 0.008, 0.069, 0.100, 0.055, 0.016, 0.036},  // 23e
    {0.169, 0.137, 0.006, 0.003, 0.062, 0.027, 0.007, 0.001},  // 23i
    {0.088, 0.032, 0.079, 0.100, 0.051, 0.009, 0.003, 0.000},  // 4e
    {0.082, 0.052, 0.080, 0.130, 0.007, 0.000, 0.000, 0.000},  // 4i
    {0.032, 0.075, 0.007, 0.006, 0.083, 0.060, 0.004, 0.015},  // 5e
    {0.000, 0.000, 0.001, 0.000, 0.000, 0.000, 0.000, 0.000},  // 5i
    {0.008, 0.004, 0.045, 0.000, 0.028, 0.000, 0.016, 0.007},  // 6e
    {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000},  // 6i
};

int scaled_count(double fraction, int total, int min_n = 1) {
    return std::max(min_n, static_cast<int>(std::lround(fraction * total)));
}

double sample_rate(std::mt19937_64& rng, double mean) {
    std::lognormal_distribution<double> dist(std::log(std::max(0.5, mean)), 0.35);
    return std::max(0.05, dist(rng));
}

void add_random_synapses(DirectedHypergraph& g, std::mt19937_64& rng,
                         const std::vector<NeuronId>& srcs,
                         const std::vector<NeuronId>& dsts, double p) {
    if (srcs.empty() || dsts.empty() || p <= 0.0) {
        return;
    }
    std::bernoulli_distribution coin(std::min(1.0, p));
    std::vector<std::vector<NeuronId>> fanout(srcs.size());
    for (std::size_t i = 0; i < srcs.size(); ++i) {
        for (NeuronId d : dsts) {
            if (d == srcs[i]) {
                continue;
            }
            if (coin(rng)) {
                fanout[i].push_back(d);
            }
        }
    }
    for (std::size_t i = 0; i < srcs.size(); ++i) {
        if (fanout[i].empty()) {
            continue;
        }
        std::vector<NeuronId> dests = g.successors(srcs[i]);
        dests.insert(dests.end(), fanout[i].begin(), fanout[i].end());
        g.add_hyperedge(srcs[i], std::move(dests), g.activity(srcs[i]));
    }
}

DirectedHypergraph generate_potjans(int target, std::uint64_t seed, double extra_rec) {
    std::mt19937_64 rng(seed);
    std::vector<int> counts(8);
    int sum = 0;
    for (int i = 0; i < 8; ++i) {
        counts[static_cast<std::size_t>(i)] = scaled_count(kPotjans[static_cast<std::size_t>(i)].fraction, target);
        sum += counts[static_cast<std::size_t>(i)];
    }
    // Nudge the largest population so we land near the requested size.
    counts[2] += std::max(0, target - sum);
    if (sum > target) {
        counts[2] = std::max(1, counts[2] - (sum - target));
    }

    DirectedHypergraph g;
    g.reserve(static_cast<std::size_t>(target + 8));
    std::array<std::vector<NeuronId>, 8> pop;
    for (int p = 0; p < 8; ++p) {
        pop[static_cast<std::size_t>(p)].reserve(static_cast<std::size_t>(counts[static_cast<std::size_t>(p)]));
        for (int i = 0; i < counts[static_cast<std::size_t>(p)]; ++i) {
            const double r = sample_rate(rng, kPotjans[static_cast<std::size_t>(p)].base_rate);
            pop[static_cast<std::size_t>(p)].push_back(
                g.add_neuron(kPotjans[static_cast<std::size_t>(p)].name, r));
        }
    }

    // Finite-size correction: inflate tiny-population probabilities so scaled
    // nets still have meaningful fanout (otherwise P * n_dst ≈ 0).
    const double inflate = std::clamp(18.0 / std::sqrt(static_cast<double>(std::max(target, 8))),
                                      1.0, 8.0);

    for (int s = 0; s < 8; ++s) {
        for (int d = 0; d < 8; ++d) {
            double p = kP[s][d] * inflate;
            if (s == d) {
                p += extra_rec;
            }
            add_random_synapses(g, rng, pop[static_cast<std::size_t>(s)],
                                pop[static_cast<std::size_t>(d)], p);
        }
    }
    return g;
}

DirectedHypergraph generate_layered(int target, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    const int layers = 4;
    const int per = std::max(4, target / layers);
    DirectedHypergraph g;
    std::vector<std::vector<NeuronId>> L(static_cast<std::size_t>(layers));
    for (int li = 0; li < layers; ++li) {
        const int n = (li == layers - 1) ? std::max(4, target - per * (layers - 1)) : per;
        for (int i = 0; i < n; ++i) {
            const double rate = sample_rate(rng, 6.0 + 2.0 * li);
            L[static_cast<std::size_t>(li)].push_back(
                g.add_neuron("L" + std::to_string(li), rate));
        }
    }
    for (int li = 0; li + 1 < layers; ++li) {
        add_random_synapses(g, rng, L[static_cast<std::size_t>(li)],
                            L[static_cast<std::size_t>(li + 1)], 0.28);
    }
    add_random_synapses(g, rng, L[1], L[0], 0.06);
    add_random_synapses(g, rng, L[2], L[1], 0.05);
    add_random_synapses(g, rng, L[3], L[2], 0.04);
    return g;
}

DirectedHypergraph generate_random(int target, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    DirectedHypergraph g;
    g.reserve(static_cast<std::size_t>(target));
    std::vector<NeuronId> all;
    all.reserve(static_cast<std::size_t>(target));
    for (int i = 0; i < target; ++i) {
        all.push_back(g.add_neuron("n", sample_rate(rng, 6.0)));
    }
    const double p = std::min(0.35, 6.0 / std::max(4.0, static_cast<double>(target)));
    add_random_synapses(g, rng, all, all, p);
    return g;
}

}  // namespace

DirectedHypergraph generate_snn(const GeneratorConfig& cfg) {
    int target = cfg.target_neurons;
    if (target <= 0) {
        target = std::max(16, static_cast<int>(std::lround(77169.0 * cfg.scale)));
    }
    switch (cfg.preset) {
        case GeneratorPreset::Potjans:
            return generate_potjans(target, cfg.seed, cfg.extra_recurrent);
        case GeneratorPreset::Layered:
            return generate_layered(target, cfg.seed);
        case GeneratorPreset::Random:
            return generate_random(target, cfg.seed);
    }
    return generate_potjans(target, cfg.seed, cfg.extra_recurrent);
}

std::string describe_network(const DirectedHypergraph& g) {
    std::ostringstream oss;
    oss << g.neuron_count() << " neurons, " << g.synapse_count() << " synapses, "
        << g.hyperedge_count() << " hyperedges, mean fanout " << g.mean_fanout();
    return oss.str();
}

}  // namespace hysmap
