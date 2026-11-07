// Port of uniffi-rs v0.31.2 examples/sprites/tests/bindings/test_sprites.py
#include "check.hpp"
#include "sprites.hpp"

using namespace sprites;

int main() {
    auto sempty = Sprite::new_(std::nullopt);
    CHECK(sempty->get_position() == (Point{0, 0}));

    auto s = Sprite::new_(Point{0, 1});
    CHECK(s->get_position() == (Point{0, 1}));
    s->move_to(Point{1, 2});
    CHECK(s->get_position() == (Point{1, 2}));
    s->move_by(Vector{-4, 2});
    CHECK(s->get_position() == (Point{-3, 4}));

    auto srel = Sprite::new_relative_to(Point{0, 1}, Vector{1, 1.5});
    CHECK(srel->get_position() == (Point{1, 2.5}));
    return check::report();
}
