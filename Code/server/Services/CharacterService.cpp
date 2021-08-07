#include <stdafx.h>

#include <Services/CharacterService.h>
#include <Components.h>
#include <GameServer.h>
#include <World.h>

#include <Events/CharacterSpawnedEvent.h>
#include <Events/CharacterExteriorCellChangeEvent.h>
#include <Events/CharacterInteriorCellChangeEvent.h>
#include <Events/PlayerEnterWorldEvent.h>
#include <Events/UpdateEvent.h>
#include <Events/CharacterRemoveEvent.h>
#include <Events/OwnershipTransferEvent.h>
#include <Scripts/Npc.h>

#include <Game/OwnerView.h>

#include <Messages/AssignCharacterRequest.h>
#include <Messages/AssignCharacterResponse.h>
#include <Messages/ServerReferencesMoveRequest.h>
#include <Messages/ClientReferencesMoveRequest.h>
#include <Messages/CharacterSpawnRequest.h>
#include <Messages/RequestFactionsChanges.h>
#include <Messages/NotifyFactionsChanges.h>
#include <Messages/NotifyRemoveCharacter.h>
#include <Messages/RequestSpawnData.h>
#include <Messages/NotifySpawnData.h>
#include <Messages/RequestOwnershipTransfer.h>
#include <Messages/NotifyOwnershipTransfer.h>
#include <Messages/RequestOwnershipClaim.h>

CharacterService::CharacterService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_updateConnection(aDispatcher.sink<UpdateEvent>().connect<&CharacterService::OnUpdate>(this))
    , m_interiorCellChangeEventConnection(aDispatcher.sink<CharacterInteriorCellChangeEvent>().connect<&CharacterService::OnCharacterInteriorCellChange>(this))
    , m_exteriorCellChangeEventConnection(aDispatcher.sink<CharacterExteriorCellChangeEvent>().connect<&CharacterService::OnCharacterExteriorCellChange>(this))
    , m_characterAssignRequestConnection(aDispatcher.sink<PacketEvent<AssignCharacterRequest>>().connect<&CharacterService::OnAssignCharacterRequest>(this))
    , m_transferOwnershipConnection(aDispatcher.sink<PacketEvent<RequestOwnershipTransfer>>().connect<&CharacterService::OnOwnershipTransferRequest>(this))
    , m_ownershipTransferEventConnection(aDispatcher.sink<OwnershipTransferEvent>().connect<&CharacterService::OnOwnershipTransferEvent>(this))
    , m_claimOwnershipConnection(aDispatcher.sink<PacketEvent<RequestOwnershipClaim>>().connect<&CharacterService::OnOwnershipClaimRequest>(this))
    , m_removeCharacterConnection(aDispatcher.sink<CharacterRemoveEvent>().connect<&CharacterService::OnCharacterRemoveEvent>(this))
    , m_characterSpawnedConnection(aDispatcher.sink<CharacterSpawnedEvent>().connect<&CharacterService::OnCharacterSpawned>(this))
    , m_referenceMovementSnapshotConnection(aDispatcher.sink<PacketEvent<ClientReferencesMoveRequest>>().connect<&CharacterService::OnReferencesMoveRequest>(this))
    , m_factionsChangesConnection(aDispatcher.sink<PacketEvent<RequestFactionsChanges>>().connect<&CharacterService::OnFactionsChanges>(this))
    , m_spawnDataConnection(aDispatcher.sink<PacketEvent<RequestSpawnData>>().connect<&CharacterService::OnRequestSpawnData>(this))
{
}

void CharacterService::Serialize(const World& aRegistry, entt::entity aEntity, CharacterSpawnRequest* apSpawnRequest) noexcept
{
    const auto& characterComponent = aRegistry.get<CharacterComponent>(aEntity);

    apSpawnRequest->ServerId = World::ToInteger(aEntity);
    apSpawnRequest->AppearanceBuffer = characterComponent.SaveBuffer;
    apSpawnRequest->ChangeFlags = characterComponent.ChangeFlags;
    apSpawnRequest->FaceTints = characterComponent.FaceTints;
    apSpawnRequest->FactionsContent = characterComponent.FactionsContent;
    apSpawnRequest->IsDead = characterComponent.IsDead;

    const auto* pFormIdComponent = aRegistry.try_get<FormIdComponent>(aEntity);
    if (pFormIdComponent)
    {
        apSpawnRequest->FormId = pFormIdComponent->Id;
    }

    const auto* pInventoryComponent = aRegistry.try_get<InventoryComponent>(aEntity);
    if (pInventoryComponent)
    {
        apSpawnRequest->InventoryContent = pInventoryComponent->Content;
    }

    const auto* pActorValuesComponent = aRegistry.try_get<ActorValuesComponent>(aEntity);
    if (pActorValuesComponent)
    {
        apSpawnRequest->InitialActorValues = pActorValuesComponent->CurrentActorValues;
    }

    if (characterComponent.BaseId)
    {
        apSpawnRequest->BaseId = characterComponent.BaseId.Id;
    }

    const auto* pMovementComponent = aRegistry.try_get<MovementComponent>(aEntity);
    if (pMovementComponent)
    {
        apSpawnRequest->Position = pMovementComponent->Position;
        apSpawnRequest->Rotation.x = pMovementComponent->Rotation.x;
        apSpawnRequest->Rotation.y = pMovementComponent->Rotation.z;
    }

    const auto* pCellIdComponent = aRegistry.try_get<CellIdComponent>(aEntity);
    if (pCellIdComponent)
    {
        apSpawnRequest->CellId = pCellIdComponent->Cell;
    }

    const auto& animationComponent = aRegistry.get<AnimationComponent>(aEntity);
    apSpawnRequest->LatestAction = animationComponent.CurrentAction;
}

void CharacterService::OnUpdate(const UpdateEvent&) const noexcept
{
    ProcessFactionsChanges();
    ProcessMovementChanges();
}

void CharacterService::OnCharacterExteriorCellChange(const CharacterExteriorCellChangeEvent& acEvent) const noexcept
{
    CharacterSpawnRequest spawnMessage;
    Serialize(m_world, acEvent.Entity, &spawnMessage);

    NotifyRemoveCharacter removeMessage;
    removeMessage.ServerId = World::ToInteger(acEvent.Entity);

    for (auto pPlayer : m_world.GetPlayerManager())
    {
        if (acEvent.Owner == pPlayer)
            continue;

        if (pPlayer->GetCellComponent().WorldSpaceId != acEvent.WorldSpaceId ||
            pPlayer->GetCellComponent().WorldSpaceId == acEvent.WorldSpaceId &&
                !GridCellCoords::IsCellInGridCell(acEvent.CurrentCoords, pPlayer->GetCellComponent().CenterCoords))
        {
            pPlayer->Send(removeMessage);
        }
        else if (pPlayer->GetCellComponent().WorldSpaceId == acEvent.WorldSpaceId &&
                 GridCellCoords::IsCellInGridCell(acEvent.CurrentCoords, pPlayer->GetCellComponent().CenterCoords))
        {
            pPlayer->Send(spawnMessage);
        }
    }
}

void CharacterService::OnCharacterInteriorCellChange(const CharacterInteriorCellChangeEvent& acEvent) const noexcept
{
    CharacterSpawnRequest spawnMessage;
    Serialize(m_world, acEvent.Entity, &spawnMessage);

    NotifyRemoveCharacter removeMessage;
    removeMessage.ServerId = World::ToInteger(acEvent.Entity);

    for (auto pPlayer : m_world.GetPlayerManager())
    {
        if (acEvent.Owner == pPlayer)
            continue;

        if (acEvent.NewCell == pPlayer->GetCellComponent().Cell)
            pPlayer->Send(spawnMessage);
        else
            pPlayer->Send(removeMessage);
    }
}

void CharacterService::OnAssignCharacterRequest(const PacketEvent<AssignCharacterRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;
    const auto& refId = message.ReferenceId;

    const auto isCustom = (refId.ModId == 0 && refId.BaseId == 0x14) || refId.ModId == std::numeric_limits<uint32_t>::max();

    // Check if id is the player
    if (!isCustom)
    {
        // Look for the character
        auto view = m_world.view<FormIdComponent, ActorValuesComponent, CharacterComponent, MovementComponent, CellIdComponent>();

        const auto itor = std::find_if(std::begin(view), std::end(view), [view, refId](auto entity)
            {
                const auto& formIdComponent = view.get<FormIdComponent>(entity);

                return formIdComponent.Id == refId;
            });

        if (itor != std::end(view))
        {
            // This entity already has an owner
            spdlog::info("FormId: {:x}:{:x} is already managed", refId.ModId, refId.BaseId);

            const auto* pServer = GameServer::Get();

            auto& actorValuesComponent = view.get<ActorValuesComponent>(*itor);
            auto& characterComponent = view.get<CharacterComponent>(*itor);
            auto& movementComponent = view.get<MovementComponent>(*itor);
            auto& cellIdComponent = view.get<CellIdComponent>(*itor);

            AssignCharacterResponse response;
            response.Cookie = message.Cookie;
            response.ServerId = World::ToInteger(*itor);
            response.Owner = false;
            response.AllActorValues = actorValuesComponent.CurrentActorValues;
            response.IsDead = characterComponent.IsDead;
            response.Position = movementComponent.Position;
            response.CellId = cellIdComponent.Cell;

            acMessage.pPlayer->Send(response);
            return;
        }
    }

    // This entity has no owner create it
    CreateCharacter(acMessage);
}

void CharacterService::OnOwnershipTransferRequest(const PacketEvent<RequestOwnershipTransfer>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    OwnerView<CharacterComponent, CellIdComponent> view(m_world, acMessage.GetSender());

    const auto it = view.find(static_cast<entt::entity>(message.ServerId));
    if (it == view.end())
    {
        spdlog::warn("Client {:X} requested travel of an entity that doesn't exist !", acMessage.pPlayer->GetConnectionId());
        return;
    }

    auto& characterOwnerComponent = view.get<OwnerComponent>(*it);
    characterOwnerComponent.InvalidOwners.push_back(acMessage.pPlayer);

    m_world.GetDispatcher().trigger(OwnershipTransferEvent(*it));
}

void CharacterService::OnOwnershipTransferEvent(const OwnershipTransferEvent& acEvent) const noexcept
{
    const auto view = m_world.view<OwnerComponent, CharacterComponent, CellIdComponent>();

    auto& characterOwnerComponent = view.get<OwnerComponent>(acEvent.Entity);
    auto& characterCellIdComponent = view.get<CellIdComponent>(acEvent.Entity);

    NotifyOwnershipTransfer response;
    response.ServerId = World::ToInteger(acEvent.Entity);
    
    bool foundOwner = false;
    for (auto pPlayer : m_world.GetPlayerManager())
    {
        if (characterOwnerComponent.GetOwner() == pPlayer)
            continue;

        bool isPlayerInvalid = false;
        for (const auto invalidOwner : characterOwnerComponent.InvalidOwners)
        {
            isPlayerInvalid = invalidOwner == pPlayer;
            if (isPlayerInvalid)
                break;
        }

        if (isPlayerInvalid)
            continue;

        if (pPlayer->GetCellComponent().WorldSpaceId == GameId{})
        {
            if (pPlayer->GetCellComponent().Cell != characterCellIdComponent.Cell)
                continue;
        }
        else
        {
            if (!GridCellCoords::IsCellInGridCell(characterCellIdComponent.CenterCoords, pPlayer->GetCellComponent().CenterCoords))
                continue;
        }

        characterOwnerComponent.SetOwner(pPlayer);

        pPlayer->Send(response);

        foundOwner = true;
        break;
    }

    if (!foundOwner)
        m_world.GetDispatcher().trigger(CharacterRemoveEvent(response.ServerId));
}

void CharacterService::OnCharacterRemoveEvent(const CharacterRemoveEvent& acEvent) const noexcept
{
    const auto view = m_world.view<OwnerComponent>();
    const auto it = view.find(static_cast<entt::entity>(acEvent.ServerId));
    const auto& characterOwnerComponent = view.get<OwnerComponent>(*it);

    NotifyRemoveCharacter response;
    response.ServerId = acEvent.ServerId;

    for(auto pPlayer : m_world.GetPlayerManager())
    {
        if (characterOwnerComponent.GetOwner() == pPlayer)
            continue;

        pPlayer->Send(response);
    }

    m_world.destroy(*it);
    spdlog::info("Character destroyed {:X}", acEvent.ServerId);
}

void CharacterService::OnOwnershipClaimRequest(const PacketEvent<RequestOwnershipClaim>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    const OwnerView<CharacterComponent, CellIdComponent> view(m_world, acMessage.GetSender());
    const auto it = view.find(static_cast<entt::entity>(message.ServerId));
    if (it == view.end())
    {
        spdlog::warn("Client {:X} requested travel of an entity that doesn't exist !", acMessage.pPlayer->GetConnectionId());
        return;
    }

    auto& characterOwnerComponent = view.get<OwnerComponent>(*it);
    if (characterOwnerComponent.GetOwner() != acMessage.pPlayer)
    {
        spdlog::warn("Client {:X} requested travel of an entity that they do not own !", acMessage.pPlayer->GetConnectionId());
        return;
    }

    characterOwnerComponent.InvalidOwners.clear();
    spdlog::info("\tOwnership claimed {:X}", message.ServerId);
}

void CharacterService::OnCharacterSpawned(const CharacterSpawnedEvent& acEvent) const noexcept
{
    CharacterSpawnRequest message;
    Serialize(m_world, acEvent.Entity, &message);

    const auto& characterCellIdComponent = m_world.get<CellIdComponent>(acEvent.Entity);
    const auto& characterOwnerComponent = m_world.get<OwnerComponent>(acEvent.Entity);

    if (characterCellIdComponent.WorldSpaceId == GameId{})
    {
        for (auto pPlayer : m_world.GetPlayerManager())
        {
            if (characterOwnerComponent.GetOwner() == pPlayer || characterCellIdComponent.Cell != pPlayer->GetCellComponent().Cell)
                continue;

            pPlayer->Send(message);
        }
    }
    else
    {
        for (auto pPlayer : m_world.GetPlayerManager())
        {
            if (characterOwnerComponent.GetOwner() == pPlayer)
                continue;

            if (pPlayer->GetCellComponent().WorldSpaceId == characterCellIdComponent.WorldSpaceId && 
              GridCellCoords::IsCellInGridCell(pPlayer->GetCellComponent().CenterCoords, characterCellIdComponent.CenterCoords))
                pPlayer->Send(message);
        }
    }
}

void CharacterService::OnRequestSpawnData(const PacketEvent<RequestSpawnData>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    auto view = m_world.view<ActorValuesComponent, InventoryComponent>();
    
    auto itor = view.find(static_cast<entt::entity>(message.Id));

    if (itor != std::end(view))
    {
        NotifySpawnData notifySpawnData;
        notifySpawnData.Id = message.Id;

        const auto* pActorValuesComponent = m_world.try_get<ActorValuesComponent>(*itor);
        if (pActorValuesComponent)
        {
            notifySpawnData.InitialActorValues = pActorValuesComponent->CurrentActorValues;
        }

        const auto* pInventoryComponent = m_world.try_get<InventoryComponent>(*itor);
        if (pInventoryComponent)
        {
            notifySpawnData.InitialInventory = pInventoryComponent->Content;
        }

        notifySpawnData.IsDead = false;
        const auto* pCharacterComponent = m_world.try_get<CharacterComponent>(*itor);
        if (pCharacterComponent)
        {
            notifySpawnData.IsDead = pCharacterComponent->IsDead;
        }

        acMessage.pPlayer->Send(notifySpawnData);
    }
}

void CharacterService::OnReferencesMoveRequest(const PacketEvent<ClientReferencesMoveRequest>& acMessage) const noexcept
{
    OwnerView<AnimationComponent, MovementComponent, CellIdComponent> view(m_world, acMessage.GetSender());

    auto& message = acMessage.Packet;

    for (auto& entry : message.Updates)
    {
        auto itor = view.find(static_cast<entt::entity>(entry.first));
        if (itor == std::end(view))
        {
            spdlog::debug("{:x} requested move of {:x} but does not exist", acMessage.pPlayer->GetConnectionId(), World::ToInteger(*itor));
            continue;
        }

        Script::Npc npc(*itor, m_world);

        auto& movementComponent = view.get<MovementComponent>(*itor);
        auto& cellIdComponent = view.get<CellIdComponent>(*itor);
        auto& animationComponent = view.get<AnimationComponent>(*itor);

        movementComponent.Tick = message.Tick;

        const auto movementCopy = movementComponent;

        auto& update = entry.second;
        auto& movement = update.UpdatedMovement;

        movementComponent.Position = movement.Position;
        movementComponent.Rotation = glm::vec3(movement.Rotation.x, 0.f, movement.Rotation.y);
        movementComponent.Variables = movement.Variables;
        movementComponent.Direction = movement.Direction;

        cellIdComponent.Cell = movement.CellId;
        cellIdComponent.WorldSpaceId = movement.WorldSpaceId;
        cellIdComponent.CenterCoords = GridCellCoords::CalculateGridCellCoords(movement.Position.x, movement.Position.y);

        auto [canceled, reason] = m_world.GetScriptService().HandleMove(npc);

        if (canceled)
        {
            movementComponent = movementCopy;
        }

        for (auto& action : update.ActionEvents)
        {
            //TODO: HandleAction
            //auto [canceled, reason] = apWorld->GetScriptServce()->HandleMove(acMessage.Player.ConnectionId, kvp.first);

            animationComponent.CurrentAction = action;

            animationComponent.Actions.push_back(animationComponent.CurrentAction);
        }

        movementComponent.Sent = false;
    }
}

void CharacterService::OnFactionsChanges(const PacketEvent<RequestFactionsChanges>& acMessage) const noexcept
{
    OwnerView<CharacterComponent> view(m_world, acMessage.GetSender());

    auto& message = acMessage.Packet;

    for (auto& [id, factions] : message.Changes)
    {
        auto itor = view.find(static_cast<entt::entity>(id));

        if (itor == std::end(view) || view.get<OwnerComponent>(*itor).GetOwner() != acMessage.pPlayer)
            continue;

        auto& characterComponent = view.get<CharacterComponent>(*itor);
        characterComponent.FactionsContent = factions;
        characterComponent.DirtyFactions = true;
    }
}

void CharacterService::CreateCharacter(const PacketEvent<AssignCharacterRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    const auto gameId = message.ReferenceId;
    const auto baseId = message.FormId;

    const auto cEntity = m_world.create();
    const auto isTemporary = gameId.ModId == std::numeric_limits<uint32_t>::max();
    const auto isPlayer = (gameId.ModId == 0 && gameId.BaseId == 0x14);
    const auto isCustom = isPlayer || isTemporary;

    // For player characters and temporary forms
    if (!isCustom)
    {
        m_world.emplace<FormIdComponent>(cEntity, gameId.BaseId, gameId.ModId);
    }
    else if (baseId != GameId{} && !isTemporary)
    {
        m_world.destroy(cEntity);
        spdlog::warn("Unexpected NpcId, player {:x} might be forging packets", acMessage.pPlayer->GetConnectionId());
        return;
    }

    auto* const pServer = GameServer::Get();

    m_world.emplace<OwnerComponent>(cEntity, acMessage.pPlayer);

    auto& cellIdComponent = m_world.emplace<CellIdComponent>(cEntity, message.CellId);
    if (message.WorldSpaceId != GameId{})
    {
        cellIdComponent.WorldSpaceId = message.WorldSpaceId;
        auto coords = GridCellCoords::CalculateGridCellCoords(message.Position.x, message.Position.y);
        cellIdComponent.CenterCoords = coords;
    }

    auto& characterComponent = m_world.emplace<CharacterComponent>(cEntity);
    characterComponent.ChangeFlags = message.ChangeFlags;
    characterComponent.SaveBuffer = std::move(message.AppearanceBuffer);
    characterComponent.BaseId = FormIdComponent(message.FormId);
    characterComponent.FaceTints = message.FaceTints;
    characterComponent.FactionsContent = message.FactionsContent;
    characterComponent.IsDead = message.IsDead;

    auto& inventoryComponent = m_world.emplace<InventoryComponent>(cEntity);
    inventoryComponent.Content = message.InventoryContent;

    auto& actorValuesComponent = m_world.emplace<ActorValuesComponent>(cEntity);
    actorValuesComponent.CurrentActorValues = message.AllActorValues;

    spdlog::info("FormId: {:x}:{:x} - NpcId: {:x}:{:x} assigned to {:x}", gameId.ModId, gameId.BaseId, baseId.ModId, baseId.BaseId, acMessage.pPlayer->GetConnectionId());

    auto& movementComponent = m_world.emplace<MovementComponent>(cEntity);
    movementComponent.Tick = pServer->GetTick();
    movementComponent.Position = message.Position;
    movementComponent.Rotation = {message.Rotation.x, 0.f, message.Rotation.y};
    movementComponent.Sent = false;

    auto& animationComponent = m_world.emplace<AnimationComponent>(cEntity);
    animationComponent.CurrentAction = message.LatestAction;

    // If this is a player character store a ref and trigger an event
    if (isPlayer)
    {
        const auto pPlayer = acMessage.pPlayer;

        pPlayer->SetCharacter(cEntity);
        pPlayer->GetQuestLogComponent().QuestContent = message.QuestContent;

        auto& dispatcher = m_world.GetDispatcher();
        dispatcher.trigger(PlayerEnterWorldEvent(pPlayer));
    }

    AssignCharacterResponse response;
    response.Cookie = message.Cookie;
    response.ServerId = World::ToInteger(cEntity);
    response.Owner = true;
    response.AllActorValues = message.AllActorValues;

    pServer->Send(acMessage.pPlayer->GetConnectionId(), response);

    auto& dispatcher = m_world.GetDispatcher();
    dispatcher.trigger(CharacterSpawnedEvent(cEntity));
}


void CharacterService::ProcessFactionsChanges() const noexcept
{
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenSnapshots = 2000ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenSnapshots)
        return;

    lastSendTimePoint = now;

    const auto characterView = m_world.view < CellIdComponent, CharacterComponent, OwnerComponent>();

    Map<Player*, NotifyFactionsChanges> messages;

    for (auto entity : characterView)
    {
        auto& characterComponent = characterView.get<CharacterComponent>(entity);
        auto& cellIdComponent = characterView.get<CellIdComponent>(entity);
        auto& ownerComponent = characterView.get<OwnerComponent>(entity);

        // If we have nothing new to send skip this
        if (characterComponent.DirtyFactions == false)
            continue;

        for (auto pPlayer : m_world.GetPlayerManager())
        {
            if (pPlayer == ownerComponent.GetOwner())
                continue;

            const auto& playerCellIdComponent = pPlayer->GetCellComponent();
            if (cellIdComponent.WorldSpaceId == GameId{})
            {
                if (playerCellIdComponent != cellIdComponent)
                    continue;
            }
            else
            {
                if (cellIdComponent.WorldSpaceId != playerCellIdComponent.WorldSpaceId ||
                    !GridCellCoords::IsCellInGridCell(cellIdComponent.CenterCoords,
                                                      playerCellIdComponent.CenterCoords))
                {
                    continue;
                }
            }

            auto& message = messages[pPlayer];
            auto& change = message.Changes[World::ToInteger(entity)];

            change = characterComponent.FactionsContent;
        }

        characterComponent.DirtyFactions = false;
    }

    for (auto [pPlayer, message] : messages)
    {
        if (!message.Changes.empty())
            pPlayer->Send(message);
    }
}

void CharacterService::ProcessMovementChanges() const noexcept
{
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenSnapshots = 1000ms / 50;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenSnapshots)
        return;

    lastSendTimePoint = now;

    const auto characterView = m_world.view < CellIdComponent, MovementComponent, AnimationComponent, OwnerComponent >();

    Map<Player*, ServerReferencesMoveRequest> messages;

    for (auto pPlayer : m_world.GetPlayerManager())
    {
        auto& message = messages[pPlayer];

        message.Tick = GameServer::Get()->GetTick();
    }

    for (auto entity : characterView)
    {
        auto& movementComponent = characterView.get<MovementComponent>(entity);
        auto& cellIdComponent = characterView.get<CellIdComponent>(entity);
        auto& ownerComponent = characterView.get<OwnerComponent>(entity);
        auto& animationComponent = characterView.get<AnimationComponent>(entity);

        // If we have nothing new to send skip this
        if (movementComponent.Sent == true)
            continue;

        for (auto pPlayer : m_world.GetPlayerManager())
        {
            if (pPlayer == ownerComponent.GetOwner())
                continue;

            const auto& playerCellIdComponent = pPlayer->GetCellComponent();
            if (cellIdComponent.WorldSpaceId == GameId{})
            {
                if (playerCellIdComponent != cellIdComponent)
                {
                    continue;
                }
            }
            else
            {
                if (cellIdComponent.WorldSpaceId != playerCellIdComponent.WorldSpaceId ||
                    !GridCellCoords::IsCellInGridCell(cellIdComponent.CenterCoords,
                                                      playerCellIdComponent.CenterCoords))
                {
                    continue;
                }
            }

            auto& message = messages[pPlayer];
            auto& update = message.Updates[World::ToInteger(entity)];
            auto& movement = update.UpdatedMovement;

            movement.Position = movementComponent.Position;

            movement.Rotation.x = movementComponent.Rotation.x;
            movement.Rotation.y = movementComponent.Rotation.z;

            movement.Direction = movementComponent.Direction;
            movement.Variables = movementComponent.Variables;

            update.ActionEvents = animationComponent.Actions;
        }
    }

    m_world.view<AnimationComponent>().each([](AnimationComponent& animationComponent)
    {
        if (!animationComponent.Actions.empty())
            animationComponent.LastSerializedAction = animationComponent.Actions[animationComponent.Actions.size() - 1];

        animationComponent.Actions.clear();
    });

    m_world.view<MovementComponent>().each([](MovementComponent& movementComponent)
    {
        movementComponent.Sent = true;
    });

    for (auto& [pPlayer, message] : messages)
    {
        if (!message.Updates.empty())
            pPlayer->Send(message);
    }
}
