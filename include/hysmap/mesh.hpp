#pragma once

#include "hysmap/types.hpp"

#include <utility>
#include <vector>

namespace hysmap {

struct Coord {
    int x = 0;
    int y = 0;

    [[nodiscard]] friend bool operator==(Coord a, Coord b) {
        return a.x == b.x && a.y == b.y;
    }
};

/// Rectangular mesh NoC. Cores are numbered row-major: id = y * cols + x.
class MeshNoC {
public:
    MeshNoC(int rows, int cols, int capacity = 1024);

    [[nodiscard]] int rows() const { return rows_; }
    [[nodiscard]] int cols() const { return cols_; }
    [[nodiscard]] int capacity() const { return capacity_; }
    [[nodiscard]] int core_count() const { return rows_ * cols_; }

    /// Number of directed adjacent links: 2 (R(C-1) + C(R-1)).
    [[nodiscard]] int link_count() const { return link_count_; }

    void set_capacity(int c) { capacity_ = c; }

    [[nodiscard]] Coord coord(CoreId id) const;
    [[nodiscard]] CoreId id(int x, int y) const;
    [[nodiscard]] bool in_bounds(int x, int y) const;
    [[nodiscard]] int manhattan(CoreId a, CoreId b) const;

    /// Deterministic X-then-Y path (empty if a == b).
    void xy_path(CoreId from, CoreId to, std::vector<LinkId>& out) const;

    /// Union of XY routes from `src` to each distinct destination core.
    void multicast_union(CoreId src, const std::vector<CoreId>& dests,
                         std::vector<LinkId>& out) const;

    [[nodiscard]] std::pair<CoreId, CoreId> decode_link(LinkId link) const;

    /// Four-neighbor cores (physical).
    [[nodiscard]] std::vector<CoreId> neighbors(CoreId id) const;

private:
    [[nodiscard]] LinkId encode_step(int x0, int y0, int x1, int y1) const;

    int rows_ = 0;
    int cols_ = 0;
    int capacity_ = 0;
    int horiz_ = 0;
    int link_count_ = 0;
};

}  // namespace hysmap
