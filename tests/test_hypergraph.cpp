#include <catch2/catch_test_macros.hpp>
#include <hysmap/hypergraph.hpp>

TEST_CASE("hypergraph records axons and predecessor locality") {
    hysmap::DirectedHypergraph g;
    const auto a = g.add_neuron("a", 2.0);
    const auto b = g.add_neuron("b", 1.0);
    const auto c = g.add_neuron("c", 1.0);
    const auto d = g.add_neuron("d", 0.5);
    g.add_hyperedge(a, {b, c, d}, 2.0);
    g.add_hyperedge(b, {c}, 1.0);

    REQUIRE(g.neuron_count() == 4);
    REQUIRE(g.synapse_count() == 4);
    REQUIRE(g.successors(a).size() == 3);
    REQUIRE(g.predecessors(c).size() == 2);
    REQUIRE(g.activity(a) == 2.0);

    const auto affected = g.affected_sources(c);
    REQUIRE(affected.size() == 3);  // {c} ∪ {a,b}
    REQUIRE(affected.front() == a);
}

TEST_CASE("hyperedge uniquifies destinations and drops self-loops") {
    hysmap::DirectedHypergraph g;
    const auto a = g.add_neuron();
    const auto b = g.add_neuron();
    g.add_hyperedge(a, {b, b, a, b}, 3.5);
    REQUIRE(g.successors(a).size() == 1);
    REQUIRE(g.successors(a)[0] == b);
    REQUIRE(g.activity(a) == 3.5);
}
