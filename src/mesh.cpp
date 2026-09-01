#include "hysmap/mesh.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace hysmap {

MeshNoC::MeshNoC(int rows, int cols, int capacity)
    : rows_(rows), cols_(cols), capacity_(capacity) {
    if (rows < 1 || cols < 1) {
        throw std::invalid_argument("mesh dimensions must be positive");
    }
    if (capacity < 1) {
        throw std::invalid_argument("core capacity must be positive");
    }
    horiz_ = rows_ * std::max(0, cols_ - 1);
    const int vert = cols_ * std::max(0, rows_ - 1);
    link_count_ = 2 * (horiz_ + vert);
}

Coord MeshNoC::coord(CoreId id) const {
    if (static_cast<int>(id) >= core_count()) {
        throw std::out_of_range("core id out of range");
    }
    return Coord{static_cast<int>(id) % cols_, static_cast<int>(id) / cols_};
}

CoreId MeshNoC::id(int x, int y) const {
    return static_cast<CoreId>(y * cols_ + x);
}

bool MeshNoC::in_bounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < cols_ && y < rows_;
}

int MeshNoC::manhattan(CoreId a, CoreId b) const {
    const Coord ca = coord(a);
    const Coord cb = coord(b);
    return std::abs(ca.x - cb.x) + std::abs(ca.y - cb.y);
}

LinkId MeshNoC::encode_step(int x0, int y0, int x1, int y1) const {
    if (y0 == y1 && x1 == x0 + 1) {
        // right
        return static_cast<LinkId>(y0 * (cols_ - 1) + x0);
    }
    if (y0 == y1 && x1 == x0 - 1) {
        // left
        return static_cast<LinkId>(horiz_ + y0 * (cols_ - 1) + x1);
    }
    const int vert_base = 2 * horiz_;
    const int vert_one_way = cols_ * std::max(0, rows_ - 1);
    if (x0 == x1 && y1 == y0 + 1) {
        // down (+y)
        return static_cast<LinkId>(vert_base + x0 * (rows_ - 1) + y0);
    }
    if (x0 == x1 && y1 == y0 - 1) {
        // up (-y)
        return static_cast<LinkId>(vert_base + vert_one_way + x0 * (rows_ - 1) + y1);
    }
    throw std::logic_error("encode_step requires adjacent cores");
}

std::pair<CoreId, CoreId> MeshNoC::decode_link(LinkId link) const {
    const int L = static_cast<int>(link);
    const int vert_one_way = cols_ * std::max(0, rows_ - 1);
    if (L < horiz_) {
        const int y = L / std::max(1, cols_ - 1);
        const int x = L % std::max(1, cols_ - 1);
        return {id(x, y), id(x + 1, y)};
    }
    if (L < 2 * horiz_) {
        const int idx = L - horiz_;
        const int y = idx / std::max(1, cols_ - 1);
        const int x = idx % std::max(1, cols_ - 1);
        return {id(x + 1, y), id(x, y)};
    }
    const int v0 = L - 2 * horiz_;
    if (v0 < vert_one_way) {
        const int x = v0 / std::max(1, rows_ - 1);
        const int y = v0 % std::max(1, rows_ - 1);
        return {id(x, y), id(x, y + 1)};
    }
    const int idx = v0 - vert_one_way;
    const int x = idx / std::max(1, rows_ - 1);
    const int y = idx % std::max(1, rows_ - 1);
    return {id(x, y + 1), id(x, y)};
}

void MeshNoC::xy_path(CoreId from, CoreId to, std::vector<LinkId>& out) const {
    out.clear();
    if (from == to) {
        return;
    }
    Coord a = coord(from);
    const Coord b = coord(to);
    while (a.x != b.x) {
        const int nx = a.x + (b.x > a.x ? 1 : -1);
        out.push_back(encode_step(a.x, a.y, nx, a.y));
        a.x = nx;
    }
    while (a.y != b.y) {
        const int ny = a.y + (b.y > a.y ? 1 : -1);
        out.push_back(encode_step(a.x, a.y, a.x, ny));
        a.y = ny;
    }
}

void MeshNoC::multicast_union(CoreId src, const std::vector<CoreId>& dests,
                              std::vector<LinkId>& out) const {
    out.clear();
    if (dests.empty()) {
        return;
    }
    std::vector<char> used(static_cast<std::size_t>(link_count_), 0);
    std::vector<LinkId> path;
    path.reserve(static_cast<std::size_t>(rows_ + cols_));
    for (CoreId d : dests) {
        if (d == src) {
            continue;
        }
        xy_path(src, d, path);
        for (LinkId l : path) {
            used[l] = 1;
        }
    }
    for (int i = 0; i < link_count_; ++i) {
        if (used[static_cast<std::size_t>(i)]) {
            out.push_back(static_cast<LinkId>(i));
        }
    }
}

std::vector<CoreId> MeshNoC::neighbors(CoreId c) const {
    const Coord p = coord(c);
    std::vector<CoreId> n;
    n.reserve(4);
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    for (int i = 0; i < 4; ++i) {
        const int x = p.x + dx[i];
        const int y = p.y + dy[i];
        if (in_bounds(x, y)) {
            n.push_back(id(x, y));
        }
    }
    return n;
}

}  // namespace hysmap
