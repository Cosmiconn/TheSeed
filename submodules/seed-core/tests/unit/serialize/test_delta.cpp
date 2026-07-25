#include <doctest/doctest.h>
#include "core/serialize/delta.h"

using namespace seed::serialize;

TEST_CASE("Delta_FloatArray_XOR") {
    float oldArr[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float newArr[5] = {1.0f, 2.5f, 3.0f, 4.0f, 5.1f};

    auto compressed = DeltaCompressor::compressFloatArray(oldArr, newArr, 5);
    CHECK(compressed.size() > 0);

    float outArr[5] = {};
    DeltaCompressor::decompressFloatArray(compressed.data(), compressed.size(), oldArr, outArr, 5);

    CHECK(outArr[0] == doctest::Approx(1.0f));
    CHECK(outArr[1] == doctest::Approx(2.5f));
    CHECK(outArr[2] == doctest::Approx(3.0f));
    CHECK(outArr[3] == doctest::Approx(4.0f));
    CHECK(outArr[4] == doctest::Approx(5.1f));
}

TEST_CASE("Delta_FloatArray_Unchanged") {
    float arr[3] = {1.0f, 2.0f, 3.0f};

    auto compressed = DeltaCompressor::compressFloatArray(arr, arr, 3);
    CHECK(compressed.size() == 8); // count + changedCount(0)

    float outArr[3] = {0.0f, 0.0f, 0.0f};
    DeltaCompressor::decompressFloatArray(compressed.data(), compressed.size(), arr, outArr, 3);

    CHECK(outArr[0] == doctest::Approx(1.0f));
    CHECK(outArr[1] == doctest::Approx(2.0f));
    CHECK(outArr[2] == doctest::Approx(3.0f));
}

TEST_CASE("Delta_FullFallback_SmallOld") {
    std::vector<uint8_t> oldData = {1, 2, 3};
    std::vector<uint8_t> newData = {10, 20, 30, 40, 50};

    auto delta = DeltaCompressor::compute(oldData, newData);
    auto result = DeltaCompressor::apply(oldData, delta);

    CHECK(result == newData);
}

TEST_CASE("Delta_ByteRange_Diff") {
    std::vector<uint8_t> oldData(100, 0);
    std::vector<uint8_t> newData(100, 0);
    newData[10] = 1;
    newData[11] = 2;
    newData[12] = 3;

    auto delta = DeltaCompressor::compute(oldData, newData);
    auto result = DeltaCompressor::apply(oldData, delta);

    CHECK(result == newData);
    CHECK(delta.size() < newData.size() / 2);
}

TEST_CASE("Delta_EmptyOld") {
    std::vector<uint8_t> oldData;
    std::vector<uint8_t> newData = {1, 2, 3, 4, 5};

    auto delta = DeltaCompressor::compute(oldData, newData);
    auto result = DeltaCompressor::apply(oldData, delta);

    CHECK(result == newData);
}
