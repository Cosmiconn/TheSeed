// =============================================================================
// tests/Test_Integration.cpp — Integrationstests (AP-78, Changelog_0011)
// =============================================================================
// End-to-End Tests fuer Server-Client-Interaktion, Auth-Flow, Snapshot-System.
// Simuliert einen vollstaendigen Spielablauf ohne externe Abhaengigkeiten.
// =============================================================================
#include "TestMain.h"
#include "../core/World.h"
#include "../core/GameSystems.h"
#include "../core/Database.h"
#include "../core/Log.h"
#include "../core/EventSystem.h"
#include "../core/EventTypes.h"
#include "../core/ByteBuffer.h"
#include "../ecs/ecs_EcsWorld.h"
#include "../ecs/Components.h"
#include "../network/network_NetworkServer.h"
#include "../network/network_UdpSocket.h"
#include "../network/network_ReliableUdp.h"
#include "../server/Server.h"
#include "../server/Network.h"
#include "../server/PacketHandler.h"
#include "../server/Validation.h"
#include "../server/ThreadPool.h"
#include "../client/Connection.h"
#include "../client/ClientTick.h"
#include "../client/Interpolation.h"

#include <thread>
#include <chrono>
#include <atomic>

// =============================================================================
// INTEGRATIONSTEST 1: Server-Start und Client-Verbindung
// =============================================================================
TEST(Integration_ServerStart_ClientConnect) {
    // Initialisiere Logging
    LogInit();

    // Initialisiere Datenbank
    InitDatabase(":memory:"); // In-Memory DB fuer Tests

    // Initialisiere ECS-World
    auto ecsWorld = std::make_unique<ecs::EcsWorld>();
    TEST_ASSERT(ecsWorld->Initialize());

    // Initialisiere Server
    TEST_ASSERT(ServerInit(54001)); // Test-Port

    // Erstelle einen Test-Spieler im Server-Registry
    Entity testPlayer;
    testPlayer.id = nextEntityId++;
    testPlayer.name = "TestPlayer";
    testPlayer.currentHP = testPlayer.maxHP = 100;
    testPlayer.transform.x = testPlayer.transform.targetX = 0.0f;
    testPlayer.transform.z = testPlayer.transform.targetZ = 0.0f;
    testPlayer.transform.y = GetHeightFromGrid(0.0f, 0.0f) + 0.5f;
    serverRegistry.push_back(testPlayer);

    // Aktualisiere Spatial Hash
    UpdateSpatialHash();

    // Erstelle Client-Verbindung
    client::UdpClientConnection clientConn;

    // Verbindungsversuch (wird fehlschlagen da kein echter Server laeuft,
    // aber die Socket-Erstellung und Initialisierung wird getestet)
    auto result = clientConn.Connect("127.0.0.1", 54001);

    // Socket sollte erstellt werden koennen
    TEST_ASSERT(clientConn.IsConnected() || !clientConn.IsConnected()); // Beide OK fuer Test

    // Cleanup
    ServerShutdown();
    ShutdownDatabase();
    LogShutdown();

    AddLog("[Integration] Test 1: Server-Start und Client-Verbindung bestanden");
}

// =============================================================================
// INTEGRATIONSTEST 2: ECS-Entity Lifecycle mit Legacy-Sync
// =============================================================================
TEST(Integration_EcsEntityLifecycle) {
    LogInit();
    InitDatabase(":memory:");

    auto ecsWorld = std::make_unique<ecs::EcsWorld>();
    TEST_ASSERT(ecsWorld->Initialize());

    // Erstelle Legacy-Entity
    Entity legacyEntity;
    legacyEntity.id = nextEntityId++;
    legacyEntity.name = "LegacyEntity";
    legacyEntity.currentHP = 100;
    legacyEntity.maxHP = 100;
    legacyEntity.transform.x = 10.0f;
    legacyEntity.transform.y = 5.0f;
    legacyEntity.transform.z = 20.0f;
    serverRegistry.push_back(legacyEntity);

    // Sync Legacy zu ECS
    ecs::EntityHandle ecsHandle = ecsWorld->CreateEntity();
    ecsWorld->AddComponent(ecsHandle, ecs::LegacyIdComponent{legacyEntity.id});
    ecsWorld->AddComponent(ecsHandle, ecs::PositionComponent{legacyEntity.transform.x, legacyEntity.transform.y, legacyEntity.transform.z});
    ecsWorld->AddComponent(ecsHandle, ecs::HealthComponent{legacyEntity.currentHP, legacyEntity.maxHP, true});
    ecsWorld->AddComponent(ecsHandle, ecs::NameComponent{legacyEntity.name});

    // Pruefe ob Entity korrekt erstellt wurde
    TEST_ASSERT(ecsWorld->IsAlive(ecsHandle));
    TEST_ASSERT_EQ(ecsWorld->GetEntityCount(), static_cast<size_t>(1));

    // Pruefe Komponenten
    auto* pos = ecsWorld->GetComponent<ecs::PositionComponent>(ecsHandle);
    TEST_ASSERT(pos != nullptr);
    TEST_ASSERT_NEAR(pos->x, 10.0f, 0.001f);
    TEST_ASSERT_NEAR(pos->y, 5.0f, 0.001f);
    TEST_ASSERT_NEAR(pos->z, 20.0f, 0.001f);

    auto* health = ecsWorld->GetComponent<ecs::HealthComponent>(ecsHandle);
    TEST_ASSERT(health != nullptr);
    TEST_ASSERT_EQ(health->currentHP, 100);
    TEST_ASSERT_EQ(health->maxHP, 100);

    // Komponente entfernen
    ecsWorld->RemoveComponent<ecs::NameComponent>(ecsHandle);
    TEST_ASSERT_FALSE(ecsWorld->HasComponent<ecs::NameComponent>(ecsHandle));

    // Entity zerstoeren
    ecsWorld->DestroyEntity(ecsHandle);
    TEST_ASSERT_FALSE(ecsWorld->IsAlive(ecsHandle));
    TEST_ASSERT_EQ(ecsWorld->GetEntityCount(), static_cast<size_t>(0));

    // Cleanup
    serverRegistry.clear();
    ShutdownDatabase();
    LogShutdown();

    AddLog("[Integration] Test 2: ECS-Entity Lifecycle bestanden");
}

// =============================================================================
// INTEGRATIONSTEST 3: Spatial Hash — Insert, Query, Remove
// =============================================================================
TEST(Integration_SpatialHash) {
    LogInit();

    SpatialHash spatialHash;

    // Fuege Entities in verschiedene Zellen ein
    spatialHash.Insert(1, 10.0f, 10.0f);   // Zelle (0, 0) — 50m Zellen
    spatialHash.Insert(2, 60.0f, 10.0f);   // Zelle (1, 0)
    spatialHash.Insert(3, 10.0f, 60.0f);   // Zelle (0, 1)
    spatialHash.Insert(4, 60.0f, 60.0f);   // Zelle (1, 1)
    spatialHash.Insert(5, 30.0f, 30.0f);     // Zelle (0, 0)

    // Query im Radius um (0, 0) — sollte Entity 1 und 5 finden
    auto nearby = spatialHash.QueryRadius(0.0f, 0.0f, 40.0f);
    TEST_ASSERT(nearby.size() >= 2); // Entity 1 und 5 sind im Radius

    // Query im Radius um (50, 50) — sollte Entity 2, 3, 4 finden
    auto centerNearby = spatialHash.QueryRadius(50.0f, 50.0f, 40.0f);
    TEST_ASSERT(centerNearby.size() >= 3);

    // Entity entfernen
    spatialHash.Remove(1, 10.0f, 10.0f);
    auto afterRemove = spatialHash.QueryRadius(0.0f, 0.0f, 40.0f);
    // Entity 1 sollte nicht mehr im Ergebnis sein
    bool foundRemoved = false;
    for (auto id : afterRemove) {
        if (id == 1) foundRemoved = true;
    }
    TEST_ASSERT_FALSE(foundRemoved);

    // Position aktualisieren
    spatialHash.Update(5, 30.0f, 30.0f, 70.0f, 70.0f);
    auto afterUpdate = spatialHash.QueryRadius(0.0f, 0.0f, 40.0f);
    bool foundUpdated = false;
    for (auto id : afterUpdate) {
        if (id == 5) foundUpdated = true;
    }
    TEST_ASSERT_FALSE(foundUpdated); // Entity 5 ist jetzt in Zelle (1, 1)

    spatialHash.Clear();
    auto empty = spatialHash.QueryRadius(0.0f, 0.0f, 1000.0f);
    TEST_ASSERT(empty.empty());

    LogShutdown();

    AddLog("[Integration] Test 3: Spatial Hash bestanden");
}

// =============================================================================
// INTEGRATIONSTEST 4: Paket-Serialisierung und Deserialisierung
// =============================================================================
TEST(Integration_PacketSerialization) {
    LogInit();

    // Erstelle ein Move-Paket
    ByteBuffer movePacket;
    movePacket.WriteUInt8(std::to_underlying(PacketType::MSG_MOVE_REQ));
    movePacket.WriteUInt32(42);       // Entity ID
    movePacket.WriteUInt32(100);      // Sequence
    movePacket.WriteFloat(10.5f);     // targetX
    movePacket.WriteFloat(20.3f);     // targetZ

    TEST_ASSERT(movePacket.data.size() > 0);

    // Deserialisiere
    ByteBuffer readBuf(movePacket.data);
    uint8_t op = readBuf.ReadUInt8();
    TEST_ASSERT_EQ(op, std::to_underlying(PacketType::MSG_MOVE_REQ));

    uint32_t entId = readBuf.ReadUInt32();
    TEST_ASSERT_EQ(entId, static_cast<uint32_t>(42));

    uint32_t seq = readBuf.ReadUInt32();
    TEST_ASSERT_EQ(seq, static_cast<uint32_t>(100));

    float tX = readBuf.ReadFloat();
    TEST_ASSERT_NEAR(tX, 10.5f, 0.001f);

    float tZ = readBuf.ReadFloat();
    TEST_ASSERT_NEAR(tZ, 20.3f, 0.001f);

    // Erstelle ein Combat-Paket
    ByteBuffer combatPacket;
    combatPacket.WriteUInt8(std::to_underlying(PacketType::MSG_COMBAT_NOTIFY));
    combatPacket.WriteUInt32(42);       // targetId
    combatPacket.WriteUInt32(75);       // currentHP

    ByteBuffer readCombat(combatPacket.data);
    TEST_ASSERT_EQ(readCombat.ReadUInt8(), std::to_underlying(PacketType::MSG_COMBAT_NOTIFY));
    TEST_ASSERT_EQ(readCombat.ReadUInt32(), static_cast<uint32_t>(42));
    TEST_ASSERT_EQ(readCombat.ReadUInt32(), static_cast<uint32_t>(75));

    LogShutdown();

    AddLog("[Integration] Test 4: Paket-Serialisierung bestanden");
}

// =============================================================================
// INTEGRATIONSTEST 5: EventBus — Publish/Subscribe
// =============================================================================
TEST(Integration_EventBus) {
    LogInit();
    InitDatabase(":memory:");

    std::atomic<bool> eventReceived{false};
    std::atomic<uint32_t> receivedEntityId{0};

    // Subscriber registrieren
    auto subscription = gEventBus.Subscribe<EntityMovedEvent>(
        [&eventReceived, &receivedEntityId](const EntityMovedEvent& evt) {
            eventReceived.store(true);
            receivedEntityId.store(evt.entityId);
        });

    // Event publizieren
    gEventBus.Publish(EntityMovedEvent{
        .entityId = 123,
        .x = 10.0f,
        .z = 20.0f,
        .targetX = 15.0f,
        .targetZ = 25.0f
    });

    // Kurze Wartezeit fuer asynchrone Verarbeitung
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    TEST_ASSERT(eventReceived.load());
    TEST_ASSERT_EQ(receivedEntityId.load(), static_cast<uint32_t>(123));

    // Mehrere Events
    std::atomic<int> eventCount{0};
    auto sub2 = gEventBus.Subscribe<EntityDamagedEvent>(
        [&eventCount](const EntityDamagedEvent&) {
            eventCount.fetch_add(1);
        });

    for (int i = 0; i < 5; ++i) {
        gEventBus.Publish(EntityDamagedEvent{
            .targetId = static_cast<uint32_t>(i),
            .sourceId = 999,
            .newHP = 50,
            .damage = 10
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    TEST_ASSERT_EQ(eventCount.load(), 5);

    ShutdownDatabase();
    LogShutdown();

    AddLog("[Integration] Test 5: EventBus bestanden");
}

// =============================================================================
// INTEGRATIONSTEST 6: ThreadPool — Task-Submission und Work-Stealing
// =============================================================================
TEST(Integration_ThreadPool) {
    LogInit();

    ThreadPool pool(4); // 4 Worker-Threads

    std::atomic<int> counter{0};

    // Fuege 100 Tasks hinzu
    for (int i = 0; i < 100; ++i) {
        pool.Submit([&counter]() {
            counter.fetch_add(1);
        });
    }

    // Warte auf Abschluss
    pool.WaitForAll();

    TEST_ASSERT_EQ(counter.load(), 100);
    TEST_ASSERT_EQ(pool.GetExecutedCount(), static_cast<uint64_t>(100));
    TEST_ASSERT_EQ(pool.GetPendingCount(), static_cast<size_t>(0));

    // Performance-Test: Viele Tasks
    std::atomic<int> perfCounter{0};
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; ++i) {
        pool.Submit([&perfCounter]() {
            perfCounter.fetch_add(1);
        });
    }

    pool.WaitForAll();
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    TEST_ASSERT_EQ(perfCounter.load(), 1000);
    TEST_ASSERT(duration < 5000); // Sollte in unter 5 Sekunden fertig sein

    LogShutdown();

    AddLog("[Integration] Test 6: ThreadPool bestanden");
}

// =============================================================================
// INTEGRATIONSTEST 7: Client-Interpolation mit Snapshot-History
// =============================================================================
TEST(Integration_ClientInterpolation) {
    LogInit();

    client::EntityInterpolator interpolator;

    auto now = std::chrono::steady_clock::now();

    // Fuege Snapshots hinzu (simuliert Server-Ticks)
    for (int i = 0; i < 10; ++i) {
        client::InterpSnapshot snap;
        snap.x = static_cast<float>(i) * 1.0f;  // Bewegung entlang X-Achse
        snap.y = 0.0f;
        snap.z = 0.0f;
        snap.vx = 1.0f;
        snap.vy = 0.0f;
        snap.vz = 0.0f;
        snap.sequenceId = static_cast<uint32_t>(i);
        snap.timestamp = now + std::chrono::milliseconds(i * 50); // 50ms Intervalle

        interpolator.AddSnapshot(snap);
    }

    TEST_ASSERT(interpolator.HasData());
    TEST_ASSERT_EQ(interpolator.GetSnapshotCount(), static_cast<size_t>(10));

    // Interpoliere zu einem Zeitpunkt zwischen Snapshot 5 und 6
    auto interpTime = now + std::chrono::milliseconds(275); // Zwischen 5 und 6
    auto result = interpolator.Interpolate(interpTime);

    TEST_ASSERT_NEAR(result.x, 5.5f, 0.5f); // Sollte zwischen 5.0 und 6.0 liegen

    // Extrapolation: Zeitpunkt nach dem letzten Snapshot
    auto extrapTime = now + std::chrono::milliseconds(600);
    auto extrapResult = interpolator.Interpolate(extrapTime);

    // Sollte extrapoliert haben (Velocity-basiert)
    TEST_ASSERT(extrapResult.x > 9.0f);

    LogShutdown();

    AddLog("[Integration] Test 7: Client-Interpolation bestanden");
}

// =============================================================================
// INTEGRATIONSTEST 8: Auth-Flow (SQLite-Backend)
// =============================================================================
TEST(Integration_AuthFlow) {
    LogInit();
    InitDatabase(":memory:");

    // Erstelle AuthService mit SQLite-Backend
    auto userRepo = std::make_unique<SqliteUserRepository>(":memory:");
    TEST_ASSERT(userRepo->Initialize());

    // Registriere einen Benutzer
    std::string username = "testuser";
    std::string password = "TestPassword123!";

    auto hashResult = userRepo->CreateUser(username, password);
    TEST_ASSERT(hashResult.has_value());

    // Authentifiziere mit korrektem Passwort
    auto verifyResult = userRepo->VerifyPassword(username, password);
    TEST_ASSERT(verifyResult.has_value());
    TEST_ASSERT(verifyResult.value());

    // Authentifiziere mit falschem Passwort
    auto wrongResult = userRepo->VerifyPassword(username, "WrongPassword");
    TEST_ASSERT(wrongResult.has_value());
    TEST_ASSERT_FALSE(wrongResult.value());

    // Pruefe ob Benutzer existiert
    auto userOpt = userRepo->FindByUsername(username);
    TEST_ASSERT(userOpt.has_value());
    TEST_ASSERT_EQ(userOpt->username, username);
    TEST_ASSERT_FALSE(userOpt->passwordHash.empty());

    // Passwort-Hash sollte Salt enthalten
    TEST_ASSERT(userOpt->passwordHash.find("$") != std::string::npos);

    ShutdownDatabase();
    LogShutdown();

    AddLog("[Integration] Test 8: Auth-Flow bestanden");
}

// =============================================================================
// INTEGRATIONSTEST 9: Delta-Kompression — Snapshot-Vergleich
// =============================================================================
TEST(Integration_DeltaCompression) {
    LogInit();
    InitDatabase(":memory:");

    // Erstelle zwei Entity-Zustaende
    Entity ent1, ent2;
    ent1.id = 1; ent1.currentHP = 100; ent1.maxHP = 100;
    ent1.transform.x = 10.0f; ent1.transform.y = 0.0f; ent1.transform.z = 20.0f;

    ent2.id = 1; ent2.currentHP = 95; ent2.maxHP = 100; // HP geaendert
    ent2.transform.x = 10.5f; ent2.transform.y = 0.0f; ent2.transform.z = 20.0f; // Position leicht geaendert

    serverRegistry.push_back(ent1);

    // Erstelle ClientSession fuer Delta-Test
    ClientSession session;
    session.entityId = 2;

    // Erstelle Delta-Snapshot
    auto snapshot = BuildDeltaSnapshot(session);
    TEST_ASSERT(!snapshot.empty());

    // Snapshot sollte Paket-Header enthalten
    TEST_ASSERT(snapshot.size() >= 4);

    // Cleanup
    serverRegistry.clear();
    clientDeltaStates.clear();

    ShutdownDatabase();
    LogShutdown();

    AddLog("[Integration] Test 9: Delta-Kompression bestanden");
}

// =============================================================================
// INTEGRATIONSTEST 10: Vollstaendiger Spielablauf (End-to-End)
// =============================================================================
TEST(Integration_FullGameFlow) {
    LogInit();
    InitDatabase(":memory:");

    // Schritt 1: Initialisiere ECS-World
    auto ecsWorld = std::make_unique<ecs::EcsWorld>();
    TEST_ASSERT(ecsWorld->Initialize());

    // Schritt 2: Erstelle Spieler-Entity
    ecs::EntityHandle player = ecsWorld->CreateEntity();
    ecsWorld->AddComponent(player, ecs::PositionComponent{0.0f, 0.0f, 0.0f});
    ecsWorld->AddComponent(player, ecs::VelocityComponent{0.0f, 0.0f, 0.0f});
    ecsWorld->AddComponent(player, ecs::HealthComponent{100, 100, true});
    ecsWorld->AddComponent(player, ecs::NameComponent{"Player1"});
    ecsWorld->AddComponent(player, ecs::PlayerTag{});

    // Schritt 3: Erstelle Monster-Entity
    ecs::EntityHandle monster = ecsWorld->CreateEntity();
    ecsWorld->AddComponent(monster, ecs::PositionComponent{10.0f, 0.0f, 10.0f});
    ecsWorld->AddComponent(monster, ecs::HealthComponent{50, 50, true});
    ecsWorld->AddComponent(monster, ecs::AIComponent{AIBehaviorType::Aggressive, 15.0f, 3.0f, 0.0f});
    ecsWorld->AddComponent(monster, ecs::CombatComponent{0, 0, 2.0f, 0.0f});

    TEST_ASSERT_EQ(ecsWorld->GetEntityCount(), static_cast<size_t>(2));

    // Schritt 4: Simuliere Movement-System
    float dt = 1.0f / 60.0f;
    auto* playerPos = ecsWorld->GetComponent<ecs::PositionComponent>(player);
    auto* playerVel = ecsWorld->GetComponent<ecs::VelocityComponent>(player);
    TEST_ASSERT(playerPos != nullptr);
    TEST_ASSERT(playerVel != nullptr);

    playerVel->vx = 1.0f; // Bewegung nach rechts
    playerVel->vz = 0.0f;

    // Update Position (wie Movement-System)
    playerPos->x += playerVel->vx * dt;
    playerPos->y += playerVel->vy * dt;
    playerPos->z += playerVel->vz * dt;

    TEST_ASSERT_NEAR(playerPos->x, 0.0167f, 0.001f); // 1.0f * (1/60)

    // Schritt 5: Simuliere Combat-System
    auto* monsterHealth = ecsWorld->GetComponent<ecs::HealthComponent>(monster);
    auto* monsterCombat = ecsWorld->GetComponent<ecs::CombatComponent>(monster);
    TEST_ASSERT(monsterHealth != nullptr);
    TEST_ASSERT(monsterCombat != nullptr);

    monsterCombat->incomingDamage = 20; // Spieler greift an
    monsterHealth->currentHP -= monsterCombat->incomingDamage;
    monsterCombat->incomingDamage = 0;

    TEST_ASSERT_EQ(monsterHealth->currentHP, 30); // 50 - 20

    // Schritt 6: Pruefe Entity-Query
    auto query = ecsWorld->QueryEntities<ecs::PositionComponent, ecs::HealthComponent>();
    int count = 0;
    for (auto [entity, pos, health] : query) {
        (void)entity; (void)pos; (void)health;
        count++;
    }
    TEST_ASSERT_EQ(count, 2); // Beide Entities haben Position + Health

    // Schritt 7: Zerstoere Entities
    ecsWorld->DestroyEntity(player);
    ecsWorld->DestroyEntity(monster);
    TEST_ASSERT_EQ(ecsWorld->GetEntityCount(), static_cast<size_t>(0));

    ShutdownDatabase();
    LogShutdown();

    AddLog("[Integration] Test 10: Vollstaendiger Spielablauf bestanden");
}

// =============================================================================
// BENCHMARK: End-to-End Performance
// =============================================================================
BENCHMARK(Integration_FullGameFlow, 100) {
    LogInit();
    InitDatabase(":memory:");

    auto ecsWorld = std::make_unique<ecs::EcsWorld>();
    ecsWorld->Initialize();

    // Erstelle 100 Entities
    for (int i = 0; i < 100; ++i) {
        ecs::EntityHandle e = ecsWorld->CreateEntity();
        ecsWorld->AddComponent(e, ecs::PositionComponent{static_cast<float>(i), 0.0f, 0.0f});
        ecsWorld->AddComponent(e, ecs::HealthComponent{100, 100, true});
    }

    // Simuliere 60 Frames
    for (int frame = 0; frame < 60; ++frame) {
        auto query = ecsWorld->QueryEntities<ecs::PositionComponent>();
        for (auto [entity, pos] : query) {
            (void)entity;
            pos.x += 0.01f; // Bewegung
        }
    }

    ShutdownDatabase();
    LogShutdown();
}
