#pragma once

#include <Events/PacketEvent.h>
#include <Structs/GameId.h>
#include <Structs/Inventory.h>

struct World;
struct UpdateEvent;
struct RequestObjectInventoryChanges;
struct RequestCharacterInventoryChanges;
struct UpdateEvent;
struct PlayerLeaveCellEvent;

using TiltedPhoques::Map;

class InventoryService
{
public:
    InventoryService(World& aWorld, entt::dispatcher& aDispatcher);

    void OnUpdate(const UpdateEvent&) noexcept;
    void OnObjectInventoryChanges(const PacketEvent<RequestObjectInventoryChanges>& acMessage) noexcept;
    void OnCharacterInventoryChanges(const PacketEvent<RequestCharacterInventoryChanges>& acMessage) noexcept;

private:

    void ProcessObjectInventoryChanges() noexcept;
    void ProcessCharacterInventoryChanges() noexcept;

    World& m_world;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_objectInventoryConnection;
    entt::scoped_connection m_characterInventoryConnection;
};
