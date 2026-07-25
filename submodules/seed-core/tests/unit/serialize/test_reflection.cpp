#include <ostream>  // required for doctest CHECK with std::string_view on MSVC
#include <doctest/doctest.h>
#include "core/serialize/reflection.h"
#include "core/serialize/binary_writer.h"

using namespace seed::serialize;

struct Vec3 {
    float x;
    float y;
    float z;
};

SEED_REFLECT_STRUCT(Vec3, 1001,
    SEED_FIELD(x),
    SEED_FIELD(y),
    SEED_FIELD(z)
)

struct PlayerData {
    int32_t id;
    float health;
    float mana;
};

SEED_REFLECT_STRUCT(PlayerData, 1002,
    SEED_FIELD(id),
    SEED_FIELD(health),
    SEED_FIELD(mana)
)

TEST_CASE("TypeRegistry_RegisterAndLookup") {
    TypeRegistry::instance().registerType<Vec3>();
    TypeRegistry::instance().registerType<PlayerData>();

    const TypeInfo* vecInfo = TypeRegistry::instance().getType(1001);
    REQUIRE(vecInfo != nullptr);
    CHECK(vecInfo->name == "Vec3");
    CHECK(vecInfo->size == sizeof(Vec3));
    CHECK(vecInfo->alignment == alignof(Vec3));
    CHECK(vecInfo->version == 1);
    CHECK(vecInfo->fields.size() == 3);

    const TypeInfo* playerInfo = TypeRegistry::instance().getType(1002);
    REQUIRE(playerInfo != nullptr);
    CHECK(playerInfo->name == "PlayerData");
    CHECK(playerInfo->fields.size() == 3);
}

TEST_CASE("TypeRegistry_LookupByName") {
    TypeRegistry::instance().registerType<Vec3>();

    const TypeInfo* info = TypeRegistry::instance().getType("Vec3");
    REQUIRE(info != nullptr);
    CHECK(info->typeId == 1001);
}

TEST_CASE("TypeRegistry_FieldOffsets") {
    TypeRegistry::instance().registerType<Vec3>();

    const TypeInfo* info = TypeRegistry::instance().getType(1001);
    REQUIRE(info != nullptr);
    REQUIRE(info->fields.size() == 3);

    CHECK(info->fields[0].name == "x");
    CHECK(info->fields[0].offset == offsetof(Vec3, x));
    CHECK(info->fields[0].size == sizeof(float));

    CHECK(info->fields[1].name == "y");
    CHECK(info->fields[1].offset == offsetof(Vec3, y));

    CHECK(info->fields[2].name == "z");
    CHECK(info->fields[2].offset == offsetof(Vec3, z));
}

TEST_CASE("TypeRegistry_NeedsMigration") {
    TypeRegistry::instance().registerType<Vec3>();

    CHECK(TypeRegistry::instance().needsMigration(1001, 1) == false);
    CHECK(TypeRegistry::instance().needsMigration(1001, 2) == true);
}

TEST_CASE("TypeRegistry_SerializeType") {
    TypeRegistry::instance().registerType<Vec3>();

    BinaryWriter writer;
    TypeRegistry::instance().serializeType(1001, writer);

    CHECK(writer.size() > 0);
}
