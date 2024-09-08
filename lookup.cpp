#include <algorithm>
#include <iostream>

#include "3ps/ska/bytell_hash_map.hpp"

#include "shapez.hpp"


int main(int argc, char* argv[]) {
    using namespace Shapez;

    if (argc != 3) {
        std::cout << "Usage: lookup dump.bin shape" << std::endl;
        return 1;
    }
    ShapeSet set = ShapeSet::load(argv[1]);

    ska::bytell_hash_set<Shape> halves{set.halves.begin(), set.halves.end()};

    auto creatable = [&](Shape shape) {
        constexpr Shape::T mask = repeat<Shape::T>(
                repeat<Shape::T>(3, 2, Shape::PART / 2),
                2 * Shape::PART, Shape::LAYER);
        for (size_t angle = 0; angle < Shape::PART / 2; ++angle) {
            Shape left{shape.rotate(angle).value & mask};
            Shape right{shape.rotate(angle + Shape::PART / 2).value & mask};
            left = left.equivalentHalves()[0];
            right = right.equivalentHalves()[0];
            if (halves.find(left) != halves.end()
                    && halves.find(right) != halves.end()) {
                return true;
            }
        }

        Shape repr = shape.equivalentShapes()[0];
        return std::binary_search(set.shapes.begin(), set.shapes.end(), repr);
    };

    Shape shape{argv[2]};
    if (creatable(shape)) {
        std::cout << "The shape is creatable" << std::endl;
    } else {
        std::cout << "The shape is not creatable" << std::endl;
    }

    return 0;
}
