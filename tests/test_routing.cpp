#include <catch2/catch_test_macros.hpp>
#include <hysmap/mesh.hpp>

TEST_CASE("mesh geometry and XY hop counts") {
    const hysmap::MeshNoC mesh(4, 4, 8);
    REQUIRE(mesh.core_count() == 16);
    REQUIRE(mesh.link_count() == 48);  // 2*(4*3 + 4*3)
    REQUIRE(mesh.manhattan(0, 15) == 6);

    std::vector<hysmap::LinkId> path;
    mesh.xy_path(0, 3, path);  // (0,0) -> (3,0) three right hops
    REQUIRE(path.size() == 3);
    mesh.xy_path(0, 12, path);  // (0,0) -> (0,3) three down hops
    REQUIRE(path.size() == 3);
    mesh.xy_path(0, 15, path);  // X then Y: 3 + 3
    REQUIRE(path.size() == 6);
}

TEST_CASE("multicast union counts shared XY prefixes once") {
    const hysmap::MeshNoC mesh(3, 3, 4);
    // Source at (0,0). Destinations (2,0) and (2,2) share the two right hops.
    std::vector<hysmap::LinkId> uni;
    mesh.multicast_union(0, {2, 8}, uni);
    // Path to (2,0): 2 links. Path to (2,2): 2 right + 2 down = 4. Union = 4.
    REQUIRE(uni.size() == 4);
}

TEST_CASE("decode_link is consistent with encode of adjacent steps") {
    const hysmap::MeshNoC mesh(3, 3, 4);
    std::vector<hysmap::LinkId> path;
    mesh.xy_path(0, 8, path);
    REQUIRE_FALSE(path.empty());
    auto [a, b] = mesh.decode_link(path.front());
    REQUIRE(a == 0);
    REQUIRE(mesh.manhattan(a, b) == 1);
}
