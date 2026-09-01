#pragma once

#include "hysmap/hypergraph.hpp"
#include "hysmap/incremental.hpp"
#include "hysmap/mesh.hpp"
#include "hysmap/types.hpp"

#include <random>

namespace hysmap {

struct BalanceBounds {
    int lo = 0;
    int hi = 0;
};

[[nodiscard]] BalanceBounds balance_bounds(int neurons, int cores, double slack,
                                           int capacity);

void assign_balanced(Mapping& mapping, int neurons, int cores, std::mt19937_64& rng);

/// Independent uniform core assignment, then repair capacity/balance.
void assign_random(Mapping& mapping, int neurons, int cores, std::mt19937_64& rng,
                   int capacity);

[[nodiscard]] bool is_boundary(const DirectedHypergraph& g, const Mapping& m, NeuronId v);

/// Greedy FM-style boundary refinement under pairwise edge cut
/// (unit weights or source activity). Used for Edge+QAP / Activity+QAP seeds.
int refine_edge_cut(const DirectedHypergraph& g, Mapping& mapping, const MeshNoC& mesh,
                    bool activity_weighted, std::mt19937_64& rng, int passes,
                    double slack, int capacity);

/// Route-aware multicast refinement with exact incremental gains.
int refine_multicast(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& mapping,
                     IncrementalEvaluator& eval, std::mt19937_64& rng, int passes,
                     double slack, int capacity, IncrementalTiming* timing = nullptr,
                     int threads = 1);

/// Wall-clock serial vs parallel peek of all feasible cores for one boundary neuron.
void measure_parallel_gains(const DirectedHypergraph& g, IncrementalEvaluator& eval,
                            Mapping& mapping, IncrementalTiming& timing, int threads);

}  // namespace hysmap
