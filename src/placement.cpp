#include "hysmap/placement.hpp"

#include "hysmap/cost.hpp"

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>

namespace hysmap {

std::vector<double> pairwise_flow(const DirectedHypergraph& g, const Mapping& mapping,
                                  int cores, bool activity_weighted) {
    std::vector<double> F(static_cast<std::size_t>(cores * cores), 0.0);
    for (NeuronId u = 0; u < g.neuron_count(); ++u) {
        const CoreId i = mapping.partition[u];
        const double w = activity_weighted ? g.activity(u) : 1.0;
        for (NeuronId v : g.successors(u)) {
            const CoreId j = mapping.partition[v];
            F[static_cast<std::size_t>(i) * static_cast<std::size_t>(cores) + j] += w;
        }
    }
    return F;
}

double qap_cost(const std::vector<double>& flow, const MeshNoC& mesh,
                const std::vector<CoreId>& placement) {
    const int k = static_cast<int>(placement.size());
    double q = 0.0;
    for (int i = 0; i < k; ++i) {
        for (int j = 0; j < k; ++j) {
            q += flow[static_cast<std::size_t>(i * k + j)] *
                 static_cast<double>(mesh.manhattan(placement[static_cast<std::size_t>(i)],
                                                    placement[static_cast<std::size_t>(j)]));
        }
    }
    return q;
}

static void greedy_qap_swaps(const std::vector<double>& flow, const MeshNoC& mesh,
                             std::vector<CoreId>& place) {
    const int k = static_cast<int>(place.size());
    bool improved = true;
    while (improved) {
        improved = false;
        double best = qap_cost(flow, mesh, place);
        int bi = -1, bj = -1;
        for (int i = 0; i < k; ++i) {
            for (int j = i + 1; j < k; ++j) {
                std::swap(place[static_cast<std::size_t>(i)], place[static_cast<std::size_t>(j)]);
                const double q = qap_cost(flow, mesh, place);
                std::swap(place[static_cast<std::size_t>(i)], place[static_cast<std::size_t>(j)]);
                if (q < best - 1e-12) {
                    best = q;
                    bi = i;
                    bj = j;
                }
            }
        }
        if (bi >= 0) {
            std::swap(place[static_cast<std::size_t>(bi)], place[static_cast<std::size_t>(bj)]);
            improved = true;
        }
    }
}

void place_qap(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& mapping,
               bool activity_weighted, std::mt19937_64& rng, int restarts) {
    const int k = mesh.core_count();
    const auto flow = pairwise_flow(g, mapping, k, activity_weighted);

    std::vector<CoreId> best = mapping.placement;
    double best_q = qap_cost(flow, mesh, best);

    auto consider = [&](std::vector<CoreId> p) {
        greedy_qap_swaps(flow, mesh, p);
        const double q = qap_cost(flow, mesh, p);
        if (q < best_q - 1e-12) {
            best_q = q;
            best = std::move(p);
        }
    };

    consider(best);
    std::vector<CoreId> ident(static_cast<std::size_t>(k));
    std::iota(ident.begin(), ident.end(), CoreId{0});
    consider(ident);

    for (int r = 0; r < std::max(0, restarts); ++r) {
        std::vector<CoreId> p(static_cast<std::size_t>(k));
        std::iota(p.begin(), p.end(), CoreId{0});
        std::shuffle(p.begin(), p.end(), rng);
        consider(std::move(p));
    }
    mapping.placement = std::move(best);
}

void place_force(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& mapping,
                 bool activity_weighted, int max_iters) {
    const int k = mesh.core_count();
    const auto flow = pairwise_flow(g, mapping, k, activity_weighted);

    auto potential = [&](CoreId logical, CoreId phys) {
        double pot = 0.0;
        for (int j = 0; j < k; ++j) {
            const double w =
                flow[static_cast<std::size_t>(logical) * static_cast<std::size_t>(k) +
                     static_cast<std::size_t>(j)] +
                flow[static_cast<std::size_t>(j) * static_cast<std::size_t>(k) +
                     static_cast<std::size_t>(logical)];
            if (w == 0.0) {
                continue;
            }
            const int d = std::max(1, mesh.manhattan(phys, mapping.placement[static_cast<std::size_t>(j)]));
            pot += w * static_cast<double>(d);
        }
        return pot;
    };

    for (int it = 0; it < max_iters; ++it) {
        bool moved = false;
        for (int i = 0; i < k; ++i) {
            const CoreId pi = mapping.placement[static_cast<std::size_t>(i)];
            for (CoreId nj : mesh.neighbors(pi)) {
                // Find logical core at neighbor, if any.
                int j = -1;
                for (int t = 0; t < k; ++t) {
                    if (mapping.placement[static_cast<std::size_t>(t)] == nj) {
                        j = t;
                        break;
                    }
                }
                if (j < 0) {
                    // Keep π a permutation of all mesh locations.
                    continue;
                }
                const double force_i =
                    potential(static_cast<CoreId>(i), pi) - potential(static_cast<CoreId>(i), nj);
                const double force_j =
                    potential(static_cast<CoreId>(j), nj) - potential(static_cast<CoreId>(j), pi);
                if (force_i + force_j > 1e-12) {
                    std::swap(mapping.placement[static_cast<std::size_t>(i)],
                              mapping.placement[static_cast<std::size_t>(j)]);
                    moved = true;
                }
            }
        }
        if (!moved) {
            break;
        }
    }
}

void place_min_distance(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& mapping,
                        bool activity_weighted) {
    const int k = mesh.core_count();
    const auto flow = pairwise_flow(g, mapping, k, activity_weighted);

    std::vector<double> weight(static_cast<std::size_t>(k), 0.0);
    for (int i = 0; i < k; ++i) {
        for (int j = 0; j < k; ++j) {
            weight[static_cast<std::size_t>(i)] +=
                flow[static_cast<std::size_t>(i * k + j)] + flow[static_cast<std::size_t>(j * k + i)];
        }
    }
    std::vector<int> order(static_cast<std::size_t>(k));
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return weight[static_cast<std::size_t>(a)] > weight[static_cast<std::size_t>(b)];
    });

    std::vector<char> used(static_cast<std::size_t>(k), 0);
    std::vector<char> placed(static_cast<std::size_t>(k), 0);
    std::vector<CoreId> place(static_cast<std::size_t>(k), kInvalidCore);

    // Seed the heaviest core at the mesh center.
    const CoreId center = mesh.id(mesh.cols() / 2, mesh.rows() / 2);
    place[static_cast<std::size_t>(order[0])] = center;
    used[center] = 1;
    placed[static_cast<std::size_t>(order[0])] = 1;

    for (std::size_t idx = 1; idx < order.size(); ++idx) {
        const int logical = order[idx];
        double best = 1e300;
        CoreId best_c = kInvalidCore;
        for (int phys = 0; phys < k; ++phys) {
            if (used[static_cast<std::size_t>(phys)]) {
                continue;
            }
            double cost = 0.0;
            for (int j = 0; j < k; ++j) {
                if (!placed[static_cast<std::size_t>(j)]) {
                    continue;
                }
                const double w = flow[static_cast<std::size_t>(logical * k + j)] +
                                 flow[static_cast<std::size_t>(j * k + logical)];
                cost += w * static_cast<double>(
                                mesh.manhattan(static_cast<CoreId>(phys), place[static_cast<std::size_t>(j)]));
            }
            if (cost < best) {
                best = cost;
                best_c = static_cast<CoreId>(phys);
            }
        }
        if (best_c == kInvalidCore) {
            for (int phys = 0; phys < k; ++phys) {
                if (!used[static_cast<std::size_t>(phys)]) {
                    best_c = static_cast<CoreId>(phys);
                    break;
                }
            }
        }
        place[static_cast<std::size_t>(logical)] = best_c;
        used[best_c] = 1;
        placed[static_cast<std::size_t>(logical)] = 1;
    }
    mapping.placement = std::move(place);
}

static Eigen::MatrixXd partition_laplacian(const DirectedHypergraph& g, const Mapping& mapping,
                                           int cores) {
    Eigen::MatrixXd W = Eigen::MatrixXd::Zero(cores, cores);
    for (NeuronId u = 0; u < g.neuron_count(); ++u) {
        std::vector<CoreId> members;
        members.push_back(mapping.partition[u]);
        for (NeuronId d : g.successors(u)) {
            members.push_back(mapping.partition[d]);
        }
        std::sort(members.begin(), members.end());
        members.erase(std::unique(members.begin(), members.end()), members.end());
        const double w = g.activity(u);
        for (std::size_t i = 0; i < members.size(); ++i) {
            for (std::size_t j = i + 1; j < members.size(); ++j) {
                W(static_cast<int>(members[i]), static_cast<int>(members[j])) += w;
                W(static_cast<int>(members[j]), static_cast<int>(members[i])) += w;
            }
        }
    }
    Eigen::VectorXd deg = W.rowwise().sum();
    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(cores, cores);
    for (int i = 0; i < cores; ++i) {
        L(i, i) = 1.0;
        const double di = std::max(1e-12, deg(i));
        for (int j = 0; j < cores; ++j) {
            if (i == j) {
                continue;
            }
            const double dj = std::max(1e-12, deg(j));
            L(i, j) = -W(i, j) / std::sqrt(di * dj);
        }
    }
    return L;
}

static void discretize_embedding(const Eigen::MatrixXd& xy, const MeshNoC& mesh,
                                 std::vector<CoreId>& placement) {
    const int k = mesh.core_count();
    placement.resize(static_cast<std::size_t>(k));
    std::vector<char> used(static_cast<std::size_t>(k), 0);

    Eigen::VectorXd xmin = xy.colwise().minCoeff();
    Eigen::VectorXd xmax = xy.colwise().maxCoeff();
    Eigen::MatrixXd norm = xy;
    for (int c = 0; c < 2; ++c) {
        const double span = std::max(1e-12, xmax(c) - xmin(c));
        for (int i = 0; i < k; ++i) {
            norm(i, c) = (xy(i, c) - xmin(c)) / span;
        }
    }

    std::vector<int> order(static_cast<std::size_t>(k));
    std::iota(order.begin(), order.end(), 0);
    // Place from the outside-in so extremes claim corners first.
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const double da = std::hypot(norm(a, 0) - 0.5, norm(a, 1) - 0.5);
        const double db = std::hypot(norm(b, 0) - 0.5, norm(b, 1) - 0.5);
        return da > db;
    });

    for (int idx : order) {
        double best = 1e300;
        CoreId best_c = 0;
        for (int phys = 0; phys < k; ++phys) {
            if (used[static_cast<std::size_t>(phys)]) {
                continue;
            }
            const Coord p = mesh.coord(static_cast<CoreId>(phys));
            const double gx = mesh.cols() <= 1 ? 0.0
                                               : static_cast<double>(p.x) /
                                                     static_cast<double>(mesh.cols() - 1);
            const double gy = mesh.rows() <= 1 ? 0.0
                                               : static_cast<double>(p.y) /
                                                     static_cast<double>(mesh.rows() - 1);
            const double d = std::hypot(norm(idx, 0) - gx, norm(idx, 1) - gy);
            if (d < best) {
                best = d;
                best_c = static_cast<CoreId>(phys);
            }
        }
        placement[static_cast<std::size_t>(idx)] = best_c;
        used[best_c] = 1;
    }
}

void place_spectral(const DirectedHypergraph& g, const MeshNoC& mesh, Mapping& mapping) {
    const int k = mesh.core_count();
    if (k <= 2) {
        std::iota(mapping.placement.begin(), mapping.placement.end(), CoreId{0});
        return;
    }
    const Eigen::MatrixXd L = partition_laplacian(g, mapping, k);
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(L);
    const Eigen::VectorXd ev = es.eigenvalues();
    int a = 1;
    int b = std::min(2, k - 1);
    // Skip numerically-zero modes.
    while (a < k - 1 && ev(a) < 1e-10) {
        ++a;
    }
    b = std::min(k - 1, a + 1);
    Eigen::MatrixXd xy(k, 2);
    xy.col(0) = es.eigenvectors().col(a);
    xy.col(1) = es.eigenvectors().col(b);
    discretize_embedding(xy, mesh, mapping.placement);
}

void seed_spectral_partition(const DirectedHypergraph& g, const MeshNoC& mesh,
                             Mapping& mapping, int capacity) {
    const int n = static_cast<int>(g.neuron_count());
    const int k = mesh.core_count();
    mapping.partition.assign(static_cast<std::size_t>(n), 0);
    mapping.placement.resize(static_cast<std::size_t>(k));
    std::iota(mapping.placement.begin(), mapping.placement.end(), CoreId{0});
    if (n <= 2) {
        return;
    }

    Eigen::MatrixXd W = Eigen::MatrixXd::Zero(n, n);
    for (NeuronId u = 0; u < g.neuron_count(); ++u) {
        const double w = g.activity(u);
        for (NeuronId d : g.successors(u)) {
            W(static_cast<int>(u), static_cast<int>(d)) += w;
            W(static_cast<int>(d), static_cast<int>(u)) += w;
            for (NeuronId d2 : g.successors(u)) {
                if (d2 > d) {
                    W(static_cast<int>(d), static_cast<int>(d2)) += w;
                    W(static_cast<int>(d2), static_cast<int>(d)) += w;
                }
            }
        }
    }
    Eigen::VectorXd deg = W.rowwise().sum();
    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(n, n);
    for (int i = 0; i < n; ++i) {
        L(i, i) = 1.0;
        const double di = std::max(1e-12, deg(i));
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                continue;
            }
            const double dj = std::max(1e-12, deg(j));
            L(i, j) = -W(i, j) / std::sqrt(di * dj);
        }
    }
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(L);
    int a = 1;
    while (a < n - 1 && es.eigenvalues()(a) < 1e-10) {
        ++a;
    }
    const int b = std::min(n - 1, a + 1);
    Eigen::MatrixXd xy(n, 2);
    xy.col(0) = es.eigenvectors().col(a);
    xy.col(1) = es.eigenvectors().col(b);

    Eigen::VectorXd xmin = xy.colwise().minCoeff();
    Eigen::VectorXd xmax = xy.colwise().maxCoeff();
    std::vector<int> occ(static_cast<std::size_t>(k), 0);
    const int cap = std::max(1, capacity);
    for (int v = 0; v < n; ++v) {
        double gx = 0.0;
        double gy = 0.0;
        const double sx = std::max(1e-12, xmax(0) - xmin(0));
        const double sy = std::max(1e-12, xmax(1) - xmin(1));
        gx = (xy(v, 0) - xmin(0)) / sx * std::max(0, mesh.cols() - 1);
        gy = (xy(v, 1) - xmin(1)) / sy * std::max(0, mesh.rows() - 1);
        int best = 0;
        double best_d = 1e300;
        bool found = false;
        for (int c = 0; c < k; ++c) {
            if (occ[static_cast<std::size_t>(c)] >= cap) {
                continue;
            }
            const Coord p = mesh.coord(static_cast<CoreId>(c));
            const double d = std::hypot(gx - p.x, gy - p.y);
            if (d < best_d) {
                best_d = d;
                best = c;
                found = true;
            }
        }
        if (!found) {
            best = static_cast<int>(
                std::min_element(occ.begin(), occ.end()) - occ.begin());
        }
        mapping.partition[static_cast<std::size_t>(v)] = static_cast<CoreId>(best);
        ++occ[static_cast<std::size_t>(best)];
    }
}

int refine_placement_multicast(const MeshNoC& mesh, Mapping& mapping,
                               IncrementalEvaluator& eval, std::mt19937_64& rng,
                               int restarts) {
    const int k = mesh.core_count();
    int applied = 0;

    auto improve = [&]() {
        bool changed = true;
        while (changed) {
            changed = false;
            double best_j = eval.current().objective;
            int bi = -1, bj = -1;
            for (int i = 0; i < k; ++i) {
                for (int j = i + 1; j < k; ++j) {
                    const CostBreakdown trial =
                        eval.peek_placement_swap(static_cast<CoreId>(i), static_cast<CoreId>(j));
                    if (trial.objective < best_j - 1e-12) {
                        best_j = trial.objective;
                        bi = i;
                        bj = j;
                    }
                }
            }
            if (bi >= 0) {
                eval.commit_placement_swap(static_cast<CoreId>(bi), static_cast<CoreId>(bj));
                ++applied;
                changed = true;
            }
        }
    };

    improve();
    Mapping incumbent = mapping;
    double best = eval.current().objective;

    for (int r = 0; r < std::max(0, restarts - 1); ++r) {
        std::vector<CoreId> p(static_cast<std::size_t>(k));
        std::iota(p.begin(), p.end(), CoreId{0});
        std::shuffle(p.begin(), p.end(), rng);
        mapping.placement = p;
        eval.rebuild();
        improve();
        if (eval.current().objective < best - 1e-12) {
            best = eval.current().objective;
            incumbent = mapping;
        }
    }
    mapping = incumbent;
    eval.rebuild();
    return applied;
}

}  // namespace hysmap
