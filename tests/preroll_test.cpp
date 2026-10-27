#include <doctest/doctest.h>

#include "audio/PreRollBuffer.h"

#include <vector>

TEST_CASE("pre-roll buffer keeps the most recent samples in order") {
    PreRollBuffer ring(4);
    const float a[] = {1.0f, 2.0f, 3.0f};
    ring.Write(a, 3);
    std::vector<float> out = ring.Snapshot();
    REQUIRE(out.size() == 3);
    CHECK(out[0] == 1.0f);
    CHECK(out[1] == 2.0f);
    CHECK(out[2] == 3.0f);
}

TEST_CASE("pre-roll buffer wraps and drops the oldest samples") {
    PreRollBuffer ring(4);
    const float a[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    ring.Write(a, 6);
    CHECK(ring.Size() == 4);
    std::vector<float> out = ring.Snapshot();
    REQUIRE(out.size() == 4);
    CHECK(out[0] == 3.0f);
    CHECK(out[1] == 4.0f);
    CHECK(out[2] == 5.0f);
    CHECK(out[3] == 6.0f);
}

TEST_CASE("pre-roll buffer snapshot of an empty ring is empty") {
    PreRollBuffer ring(300);
    CHECK(ring.Snapshot().empty());
    CHECK(ring.Size() == 0);
    CHECK(ring.Capacity() == 300);
}

TEST_CASE("pre-roll buffer clear resets contents") {
    PreRollBuffer ring(4);
    const float a[] = {9.0f, 8.0f};
    ring.Write(a, 2);
    ring.Clear();
    CHECK(ring.Size() == 0);
    CHECK(ring.Snapshot().empty());
    const float b[] = {7.0f};
    ring.Write(b, 1);
    std::vector<float> out = ring.Snapshot();
    REQUIRE(out.size() == 1);
    CHECK(out[0] == 7.0f);
}
