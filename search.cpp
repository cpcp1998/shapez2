#include <algorithm>
#include <deque>
#include <format>
#include <iostream>
#include <optional>

#include "3ps/ska/bytell_hash_map.hpp"

#include "shapez.hpp"

namespace Shapez {

// Search possible quads. This searcher is conservative, which means that
// it may omit some quads, but any quad found by it is always makeable.
struct ConservativeQuadSearcher {
    using T = Shape::T;
    constexpr static size_t PART = Shape::PART;
    constexpr static size_t LAYER = Shape::LAYER;

    ska::bytell_hash_set<Shape> quads;
    std::deque<Shape> queue;

    void run() {
        quads.insert(Shape());
        queue.push_back(Shape());
        while (!queue.empty()) {
            Shape shape = queue.front();
            queue.pop_front();
            process(shape);
        }
    }

    void enqueue(Shape shape) {
        if (quads.emplace(shape).second) {
            queue.push_back(shape);
        }
    }

    void process(Shape shape) {
        constexpr T mask = repeat<T>(3, 2 * PART, LAYER);
        size_t layers = shape.layers();
        // Fill the other quarters with regular shapes to support the
        // first quarter.
        Shape fill{~mask & repeat<T>(T(Type::Shape), 2, PART * layers)};

        // stack. Regular shapes can float at any layer. Pins can't
        for (size_t layer = layers; layer < LAYER; ++layer) {
            enqueue(shape | Shape(T(Type::Shape) << (2 * PART * layer)));
        }
        if (layers < LAYER) {
            enqueue(shape | Shape(T(Type::Pin) << (2 * PART * layers)));
        }

        // pin pusher
        enqueue((shape | fill).pin() & mask);

        // crystal generator
        enqueue((shape | fill).crystalize() & mask);

        // cut
        for (size_t layer = 0; layer < layers; ++layer) {
            Shape combine = shape | fill;
            combine.set(layer, PART - 1, Type::Crystal);
            enqueue(combine.cut() & mask);
        }
    }
};

// Enumerates all the possible shapes
// We classify shapes into two categories
// 1) There is a method to construct it that the last step is a swapping
// 2) Other shapes
// For the first kind of shapes, only the possible halves are record.
struct Searcher {
    using T = Shape::T;
    constexpr static size_t PART = Shape::PART;
    constexpr static size_t LAYER = Shape::LAYER;

    // all the possible shapes in the second category
    ska::bytell_hash_set<Shape> shapes;
    // all the possible halves
    std::vector<Shape> halves;
    // reverse mapping for `halves`
    ska::bytell_hash_map<Shape, size_t> halvesIdx;
    // all the possible quarters
    ska::bytell_hash_set<Shape> quarters;
    // queue for BFS searching. Because a shape can't be easily removed
    // in the middle of deque, a hash set is used to record all the
    // shapes that haven't be removed.
    std::deque<Shape> queue;
    ska::bytell_hash_set<Shape> queueSet;
    // the next half to be processed
    size_t nextHalf = 0;

    // All the possible connected shapes that consist of pins and regular
    // shapes. They cover all the cases for stacking another shape on top
    // of this shape. Stacking of more complex shapes can be achieved by
    // stacking these simple shapes multiple times
    std::vector<Shape> singleLayerShapes;

    // Total number of shapes explored
    size_t count = 0;
    // When the progress bar will be printed
    static constexpr size_t perLogCount = 10000000;
    size_t nextLogCount = perLogCount;

    Searcher() {
        // Init singleLayerShapes
        for (size_t part = 0; part < PART; ++part) {
            Shape pin;
            pin.set(0, part, Type::Pin);
            singleLayerShapes.push_back(pin);
        }
        for (size_t len = 1; len < PART; ++len) {
            Shape shape;
            for (size_t part = 0; part < len; ++part) {
                shape.set(0, part, Type::Shape);
            }
            for (size_t part = 0; part < PART; ++part) {
                singleLayerShapes.push_back(shape.rotate(part));
            }
        }
        singleLayerShapes.push_back(Shape(repeat<T>(T(Type::Shape), 2, PART)));
        // Move them to the top layer (where a connected part starts falling
        // when it is stacked on another shape)
        for (Shape& shape : singleLayerShapes) {
            shape.value <<= 2 * PART * (LAYER - 1);
        }
    }

    // Whether a shape can be constructed by swapping two halves.
    // Only halves with index less than lastHalf is considered.
    bool combinable(Shape shape,
                    std::optional<size_t> lastHalf = std::nullopt) const {
        constexpr T mask = repeat<T>(repeat<T>(3, 2, PART / 2), 2 * PART,
                                     LAYER);
        for (size_t angle = 0; angle < PART / 2; ++angle) {
            Shape left{shape.rotate(angle).value & mask};
            Shape right{shape.rotate(angle + PART / 2).value & mask};
            left = left.equivalentHalves()[0];
            right = right.equivalentHalves()[0];
            auto itLeft = halvesIdx.find(left);
            if (itLeft == halvesIdx.end()) {
                continue;
            }
            auto itRight = halvesIdx.find(right);
            if (itRight == halvesIdx.end()) {
                continue;
            }
            if (!lastHalf.has_value() ||
                itLeft->second < *lastHalf && itRight->second < *lastHalf) {
                return true;
            }
        }
        return false;
    }

    // Search all the possible shapes.
    // We always process the shapes in the first category first.
    void run() {
        ConservativeQuadSearcher quadSearcher;
        quadSearcher.run();
        std::cout << std::format("Found {} quarters",
                quadSearcher.quads.size()) << std::endl;

        // Estimate possible halves
        if constexpr (PART == 4) {
            std::vector<Shape> quads{quadSearcher.quads.begin(),
                                     quadSearcher.quads.end()};
            size_t numQuads = quads.size();
            size_t total = 1;
            for (size_t i = 0; i < PART / 2; ++i) {
                total *= numQuads;
            }
            for (size_t i = 0; i < total; ++i) {
                size_t idx = i;
                Shape half;
                for (size_t part = 0; part < PART / 2; ++part) {
                    size_t quad = idx % numQuads;
                    idx /= numQuads;
                    half = half | Shape(quads[quad].value << (2 * part));
                }
                half = half.collapse();
                half = half.equivalentHalves()[0];
                if (halvesIdx.emplace(half, halves.size()).second) {
                    halves.push_back(half);
                }
            }
            std::cout << std::format("Pre-calculated {} halves", halves.size())
                << std::endl;
        } else {
            // I don't know if all the shapes generated by the code above can
            // be made when PART > 4. Therefore, take a conservative approach
            halvesIdx.emplace(Shape(), 0);
            halves.push_back(Shape());
        }

        while (!queue.empty() || nextHalf < halves.size()) {
            if (nextHalf < halves.size()) {
                auto variants = halves[nextHalf].equivalentHalves();
                for (auto& shape : variants) {
                    shape = shape.rotate(PART / 2);
                }
                // Swap this new half with existing halves to create a new
                // shape.
                ska::bytell_hash_set<Shape> temp;
                for (size_t i = 0; i <= nextHalf; ++i) {
                    auto other = halves[i];
                    for (Shape a : variants) {
                        Shape combined = a | other;
                        if (combinable(combined, nextHalf)) {
                            continue;
                        }
                        Shape shape = combined.equivalentShapes()[0];
                        if (temp.emplace(shape).second) {
                            if (auto it = queueSet.find(shape);
                                    it != queueSet.end()) {
                                // We thought the shape is in category two,
                                // but it's actually is in category one. We
                                // haven't processed the shape yet, so
                                // remove the shape from the queue and
                                // process it immediately.
                                queueSet.erase(it);
                                shapes.erase(shape);
                                process(shape);
                            } else if (auto it = shapes.find(shape);
                                    it != shapes.end()) {
                                // We thought the shape is in category two,
                                // but it's actually is in category one. We
                                // have processed the shape, so only remove
                                // the shape from category two, and don't
                                // process it again.
                                shapes.erase(it);
                            } else {
                                process(shape);
                            }
                        }
                    }
                }
                ++nextHalf;
            } else {
                Shape shape = queue.front();
                queue.pop_front();
                if (auto it = queueSet.find(shape); it != queueSet.end()) {
                    queueSet.erase(it);
                    process(shape);
                }
            }
        }

        queue.shrink_to_fit();
        queueSet.shrink_to_fit();
    }

    void summarize() const {
        std::cout << "# shapes: " << count << std::endl;
        std::cout << "# halves: " << halves.size() << std::endl;
        std::cout << "# shapes whose halves aren't stable: " << shapes.size()
            << std::endl;
        std::cout << "# quarters: " << quarters.size() << std::endl;
    }

    void process(Shape shape) {
        std::vector<Shape> variants = shape.equivalentShapes();
        count += variants.size();
        if (count >= nextLogCount) {
            nextLogCount += perLogCount;
            std::cout << std::format("Processed {} shapes, {} quarters, "
                    "{}/{} halves, {}/{}/{} shapes", count, quarters.size(),
                    nextHalf, halves.size(), queueSet.size(), queue.size(),
                    shapes.size()) << std::endl;
        }

        // record unique quarter
        for (size_t angle = 0; angle < PART; ++angle) {
            constexpr T mask = repeat<T>(3, 2 * PART, LAYER);
            quarters.insert(shape.rotate(angle) & mask);
        }

        // cut
        for (size_t angle = 0; angle < PART; ++angle) {
            Shape cut = shape.rotate(angle).cut().equivalentHalves()[0];
            if (halvesIdx.find(cut) != halvesIdx.end()) {
                continue;
            }
            halvesIdx[cut] = halves.size();
            halves.push_back(cut);
        }

        // stack
        for (Shape piece : singleLayerShapes) {
            enqueue(shape.stack(piece));
        }

        // pin pusher
        enqueue(shape.pin());

        // crystal generator
        enqueue(shape.crystalize());
    }

    void enqueue(Shape shape) {
        if (combinable(shape)) {
            return;
        }

        shape = shape.equivalentShapes()[0];

        if (shapes.emplace(shape).second){
            queue.push_back(shape);
            queueSet.insert(shape);
        }
    }
};

}

int main(int argc, char* argv[]) {
    Shapez::Searcher searcher;
    searcher.run();
    searcher.summarize();

    if (argc >= 2) {
        Shapez::ShapeSet set;
        set.halves.insert(set.halves.end(), searcher.halves.begin(),
                          searcher.halves.end());
        set.shapes.insert(set.shapes.end(), searcher.shapes.begin(),
                          searcher.shapes.end());
        std::sort(set.halves.begin(), set.halves.end());
        std::sort(set.shapes.begin(), set.shapes.end());
        set.save(argv[1]);
    }
    return 0;
}
