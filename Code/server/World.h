#pragma once

#include "Services/AdminService.h"

#include <Services/ScriptService.h>
#include <Services/PlayerService.h>
#include <Services/CharacterService.h>
#include <Services/EnvironmentService.h>
#include <Services/QuestService.h>

#include "Game/PlayerManager.h"

struct World : entt::registry
{
    World();
    ~World() noexcept = default;

    TP_NOCOPYMOVE(World);

    entt::dispatcher& GetDispatcher() noexcept { return m_dispatcher; }
    const entt::dispatcher& GetDispatcher() const noexcept { return m_dispatcher; }
    CharacterService& GetCharacterService() noexcept { return ctx<CharacterService>(); }
    const CharacterService& GetCharacterService() const noexcept { return ctx<const CharacterService>(); }
    PlayerService& GetPlayerService() noexcept { return ctx<PlayerService>(); }
    const PlayerService& GetPlayerService() const noexcept { return ctx<const PlayerService>(); }
    ScriptService& GetScriptService() noexcept { return *m_scriptService; }
    const ScriptService& GetScriptService() const noexcept { return *m_scriptService; }
    EnvironmentService& GetEnvironmentService() noexcept { return ctx<EnvironmentService>(); }
    const EnvironmentService& GetEnvironmentService() const noexcept { return ctx<const EnvironmentService>(); }
    QuestService& GetQuestService() noexcept { return ctx<QuestService>(); }
    const QuestService& GetQuestService() const noexcept { return ctx<const QuestService>(); }
    PlayerManager& GetPlayerManager() noexcept { return m_playerManager; }
    const PlayerManager& GetPlayerManager() const noexcept { return m_playerManager; }

    [[nodiscard]] static uint32_t ToInteger(entt::entity aEntity) { return to_integral(aEntity); }

private:
    entt::dispatcher m_dispatcher;

    std::shared_ptr<AdminService> m_spAdminService;
    std::unique_ptr<ScriptService> m_scriptService;
    PlayerManager m_playerManager;
};
