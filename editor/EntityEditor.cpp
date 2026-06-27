// =============================================================================
// editor/EntityEditor.cpp  —  EntityEditor Implementation
// =============================================================================
#include "EntityEditor.h"

int  gOutlinerSelectedEntityId = -1;
char gEntityFilter[32] = "";

// =============================================================================
// DRAW WORLD OUTLINER
// =============================================================================
void DrawWorldOutliner() {
    ImGui::SetNextWindowPos (ImVec2(320, 0));
    ImGui::SetNextWindowSize(ImVec2(300, 400));
    ImGui::Begin("World Outliner", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::InputText("Filter", gEntityFilter, sizeof(gEntityFilter));

    if (ImGui::Button("Spawn NPC Slimy")) {
        Entity e;
        e.id = nextEntityId++;
        e.isMonster = true;
        e.name = "Slimy";
        e.monsterTemplateId = 101;
        e.currentHP = 100;
        e.transform.x = e.transform.targetX = e.transform.lerpX = 0.0f;
        e.transform.z = e.transform.targetZ = e.transform.lerpZ = 0.0f;
        e.transform.y = GetHeightFromGrid(0.0f, 0.0f) + 0.5f;
        e.render = {"mat_slimy", 0.6f, "cube"};
        CmdExecute(std::make_unique<CmdSpawnEntity>(e));
    }
    ImGui::SameLine();
    if (ImGui::Button("Spawn NPC Orc")) {
        Entity e;
        e.id = nextEntityId++;
        e.isMonster = true;
        e.name = "Orc";
        e.monsterTemplateId = 102;
        e.currentHP = 100;
        e.transform.x = e.transform.targetX = e.transform.lerpX = 2.0f;
        e.transform.z = e.transform.targetZ = e.transform.lerpZ = 2.0f;
        e.transform.y = GetHeightFromGrid(2.0f, 2.0f) + 0.5f;
        e.render = {"mat_orc", 1.2f, "pyramid"};
        CmdExecute(std::make_unique<CmdSpawnEntity>(e));
    }

    ImGui::Separator();

    for (const auto& ent : serverRegistry) {
        if (gEntityFilter[0] != '\0' &&
            ent.name.find(gEntityFilter) == std::string::npos) continue;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
        if (gOutlinerSelectedEntityId == static_cast<int>(ent.id))
            flags |= ImGuiTreeNodeFlags_Selected;

        std::string label = std::format("{}{}##{}",
            ent.isMonster ? "[M] " : "", ent.name, ent.id);

        bool open = ImGui::TreeNodeEx(label.c_str(), flags);
        if (ImGui::IsItemClicked())
            gOutlinerSelectedEntityId = static_cast<int>(ent.id);
        if (open) ImGui::TreePop();
    }

    ImGui::End();
}

// =============================================================================
// DRAW COMPONENT INSPECTOR
// =============================================================================
void DrawComponentInspector() {
    ImGui::SetNextWindowPos (ImVec2(320, 400));
    ImGui::SetNextWindowSize(ImVec2(300, 320));
    ImGui::Begin("Inspector", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    if (gOutlinerSelectedEntityId < 0) {
        ImGui::TextDisabled("Select an entity in the Outliner.");
        ImGui::End();
        return;
    }

    auto it = std::ranges::find_if(serverRegistry,
        [](const Entity& e){ return e.id == gOutlinerSelectedEntityId; });
    if (it == serverRegistry.end()) {
        ImGui::TextDisabled("Entity not found.");
        ImGui::End();
        return;
    }

    Entity* ent = &(*it);

    ImGui::Text("ID: %u | Name: %s", ent->id, ent->name.c_str());
    if (ImGui::Button("Delete Entity")) {
        CmdExecute(std::make_unique<CmdDeleteEntity>(*ent));
        gOutlinerSelectedEntityId = -1;
        ImGui::End();
        return;
    }
    ImGui::Separator();

    // Transform
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        static TransformComponent pending;
        static bool pendingInit = false;
        if (!pendingInit || pending.x != ent->transform.x) {
            pending = ent->transform;
            pendingInit = true;
        }

        bool changed = false;
        changed |= ImGui::DragFloat("Pos X", &pending.x, 0.1f);
        changed |= ImGui::DragFloat("Pos Y", &pending.y, 0.1f);
        changed |= ImGui::DragFloat("Pos Z", &pending.z, 0.1f);
        changed |= ImGui::DragFloat("Target X", &pending.targetX, 0.1f);
        changed |= ImGui::DragFloat("Target Z", &pending.targetZ, 0.1f);
        changed |= ImGui::DragFloat("Look X", &pending.lookX, 0.01f, -1.0f, 1.0f);
        changed |= ImGui::DragFloat("Look Z", &pending.lookZ, 0.01f, -1.0f, 1.0f);

        if (ImGui::Button("Apply Transform")) {
            CmdExecute(std::make_unique<CmdSetTransform>(ent->id, ent->transform, pending));
            ent->transform = pending;
            ent->persistence.isDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) { pending = ent->transform; }
    }

    // Render
    if (ImGui::CollapsingHeader("Render")) {
        char bufMat[32]; std::strncpy(bufMat, ent->render.materialId.c_str(), 31); bufMat[31]='\0';
        if (ImGui::InputText("Material", bufMat, sizeof(bufMat), ImGuiInputTextFlags_EnterReturnsTrue)) {
            ent->render.materialId = bufMat;
            ent->persistence.isDirty = true;
            ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_ENTITY_SPAWN));
            pkt.WriteUInt32(ent->id); pkt.WriteString(ent->name);
            pkt.WriteUInt8(ent->isMonster ? 1 : 0);
            pkt.WriteFloat(ent->transform.x); pkt.WriteFloat(ent->transform.z);
            pkt.WriteString(ent->render.materialId);
            pkt.WriteFloat(ent->render.scaleY); pkt.WriteString(ent->render.meshId);
            BroadcastToAll(std::span(pkt.data));
        }
        ImGui::DragFloat("Scale Y", &ent->render.scaleY, 0.05f);
        char bufMesh[32]; std::strncpy(bufMesh, ent->render.meshId.c_str(), 31); bufMesh[31]='\0';
        if (ImGui::InputText("Mesh", bufMesh, sizeof(bufMesh), ImGuiInputTextFlags_EnterReturnsTrue)) {
            ent->render.meshId = bufMesh;
            ent->persistence.isDirty = true;
        }
    }

    // Persistence
    if (ImGui::CollapsingHeader("Persistence")) {
        ImGui::Text("Level: %u", ent->persistence.level);
        ImGui::Text("Gold:  %u", ent->persistence.gold);
        ImGui::Checkbox("Is Dirty", &ent->persistence.isDirty);
    }

    // Combat
    if (ImGui::CollapsingHeader("Combat")) {
        int hp = ent->currentHP;
        if (ImGui::SliderInt("HP", &hp, 0, 100)) {
            ent->currentHP = hp;
            ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_COMBAT_NOTIFY));
            pkt.WriteUInt32(ent->id); pkt.WriteUInt32(static_cast<uint32_t>(hp));
            BroadcastToAll(std::span(pkt.data));
        }
        ImGui::Text("Monster Template: %u", ent->monsterTemplateId);
    }

    ImGui::End();
}
