#include <doctest/doctest.h>
#include "core/serialize/binary_writer.h"
#include "core/serialize/binary_reader.h"

using namespace seed::serialize;

TEST_CASE("BinaryWriter_BasicTypes") {
    BinaryWriter writer;
    writer.writeUInt8(0xAB);
    writer.writeUInt16(0x1234);
    writer.writeUInt32(0xDEADBEEF);
    writer.writeUInt64(0x0102030405060708);
    writer.writeFloat(3.14159f);
    writer.writeDouble(2.718281828);
    writer.writeBool(true);
    writer.writeBool(false);
    writer.writeString("hello");

    BinaryReader reader(writer.data());
    CHECK(reader.readUInt8() == 0xAB);
    CHECK(reader.readUInt16() == 0x1234);
    CHECK(reader.readUInt32() == 0xDEADBEEF);
    CHECK(reader.readUInt64() == 0x0102030405060708);
    CHECK(reader.readFloat() == doctest::Approx(3.14159f));
    CHECK(reader.readDouble() == doctest::Approx(2.718281828));
    CHECK(reader.readBool() == true);
    CHECK(reader.readBool() == false);
    CHECK(reader.readString() == "hello");
    CHECK(reader.eof());
}

TEST_CASE("BinaryWriter_POD") {
    struct Vec3 { float x, y, z; };
    BinaryWriter writer;
    Vec3 v{1.0f, 2.0f, 3.0f};
    writer.writePOD(v);

    BinaryReader reader(writer.data());
    Vec3 v2 = reader.readPOD<Vec3>();
    CHECK(v2.x == doctest::Approx(1.0f));
    CHECK(v2.y == doctest::Approx(2.0f));
    CHECK(v2.z == doctest::Approx(3.0f));
}

TEST_CASE("BinaryWriter_LittleEndian") {
    BinaryWriter writer;
    writer.writeUInt16(0x0102);

    const auto& data = writer.data();
    REQUIRE(data.size() == 2);
    CHECK(data[0] == 0x02);
    CHECK(data[1] == 0x01);
}

TEST_CASE("BinaryWriter_StringEmpty") {
    BinaryWriter writer;
    writer.writeString("");
    writer.writeString("test");

    BinaryReader reader(writer.data());
    CHECK(reader.readString().empty());
    CHECK(reader.readString() == "test");
}

TEST_CASE("BinaryWriter_Reset") {
    BinaryWriter writer;
    writer.writeUInt32(42);
    writer.clear();
    writer.writeUInt32(99);

    BinaryReader reader(writer.data());
    CHECK(reader.readUInt32() == 99);
}
