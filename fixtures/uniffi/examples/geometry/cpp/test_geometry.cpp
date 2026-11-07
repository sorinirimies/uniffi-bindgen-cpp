// Port of uniffi-rs v0.31.2 examples/geometry/tests/bindings/test_geometry.py
#include "check.hpp"
#include "geometry.hpp"

using namespace geometry;

int main() {
    Line ln1{Point{0, 0}, Point{1, 2}};
    Line ln2{Point{1, 1}, Point{2, 2}};
    CHECK_EQ(gradient(ln1), 2.0);
    CHECK_EQ(gradient(ln2), 1.0);
    CHECK(intersection(ln1, ln2) == std::optional<Point>(Point{0, 0}));
    CHECK(!intersection(ln1, ln1).has_value());
    return check::report();
}
