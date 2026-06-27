#pragma once
// =============================================================================
// ecs/ECS.h — ECS Integration Header (AP-20 Complete)
// Includes all ECS modules and provides migration helpers
// =============================================================================
#include "Types.h"
#include "ComponentTraits.h"
#include "Chunk.h"
#include "Archetype.h"
#include "EntityManager.h"
#include "Query.h"
#include "EcsWorld.h"

// Migration helpers for legacy code
namespace ecs {

// Helper to convert legacy Entity to ECS handle
// [[deprecated("Use ecs::EntityHandle directly after AP-23")]]
// inline EntityHandle LegacyToEcs(const ::Entity& legacy) {
//     return EntityHandle{legacy.id, 1}; // Generation 1 for all legacy entities
// }

} // namespace ecs
