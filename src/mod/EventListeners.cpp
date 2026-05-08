#include "EventListeners.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/io/Logger.h"
#include "ll/api/mod/NativeMod.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/IGameModeMessenger.h"
#include "mc/world/gamemode/IGameModeTimer.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/item/ItemStack.h"

#include "Config.h"
#include "LogManager.h"
#include "PlayerActionLog.h"

#include <chrono>
#include <atomic>
#include <optional>

// 全局 LogManager 指针
extern std::atomic<LogManager*> g_logManagerForHook;

// === 方块破坏 Hook（LL_TYPE_INSTANCE_HOOK + 哑元构造函数） ===
LL_TYPE_INSTANCE_HOOK(
    DestroyBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::_sendTryDestroyBlockEvent,
    ::std::optional<::ItemStack>,
    ::Block const&    block,
    ::BlockPos const& pos,
    ::ItemStack       itemBeforeEvent
) {
    // mPlayer 是 GameMode 的公共成员，TypedStorage<8,8,Player&> 直接等于 Player&
    Player& player = mPlayer;

    if (auto* lm = g_logManagerForHook.load()) {
        try {
            ItemStack const& tool = player.getSelectedItem();

            AggregatedBlockAction act;
            act.blockType = block.getTypeName();      // 真正的被破坏方块名
            act.toolType  = ((bool)tool) ? tool.getTypeName() : "minecraft:empty";
            act.action    = "break";
            act.count     = 1;

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            act.firstTime = now;
            act.lastTime  = now;
            act.lastPos   = pos;

            lm->pushAction(player.getUuid(), act);
        } catch (...) {}
    }

    return origin(block, pos, itemBeforeEvent);
}

static ll::io::Logger& getLogger() {
    return ll::mod::NativeMod::current()->getLogger();
}

void EventListeners::registerAll(ll::event::EventBus& bus, LogManager& lm) {
    // 玩家加入
    joinListener_ = bus.emplaceListener<ll::event::PlayerJoinEvent>(
        [&lm](ll::event::PlayerJoinEvent& ev) {
            try { lm.onPlayerJoin(ev.self().getUuid()); } catch (...) {}
        }
    );
    if (!joinListener_) getLogger().error("Failed to register PlayerJoinEvent");

    // 玩家离开
    disconnectListener_ = bus.emplaceListener<ll::event::PlayerDisconnectEvent>(
        [&lm](ll::event::PlayerDisconnectEvent& ev) {
            try { lm.onPlayerLeave(ev.self().getUuid()); } catch (...) {}
        }
    );
    if (!disconnectListener_) getLogger().error("Failed to register PlayerDisconnectEvent");

    // 方块放置
    placedListener_ = bus.emplaceListener<ll::event::PlayerPlacedBlockEvent>(
        [&lm](ll::event::PlayerPlacedBlockEvent& ev) {
            try {
                auto& player = ev.self();
                Block const& block = ev.placedBlock();
                ItemStack const& tool = player.getSelectedItem();
                AggregatedBlockAction act;
                act.blockType = block.getTypeName();
                act.toolType  = ((bool)tool) ? tool.getTypeName() : "minecraft:empty";
                act.action    = "place";
                act.count     = 1;
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                act.firstTime = now;
                act.lastTime  = now;
                act.lastPos   = ev.pos();
                lm.pushAction(player.getUuid(), act);
            } catch (...) {}
        }
    );
    if (!placedListener_) getLogger().error("Failed to register PlayerPlacedBlockEvent");

    // 方块交互
    interactListener_ = bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
        [&lm](ll::event::PlayerInteractBlockEvent& ev) {
            try {
                if (auto block = ev.block()) {
                    auto& tool = ev.item();
                    AggregatedBlockAction act;
                    act.blockType = block->getTypeName();
                    act.toolType  = ((bool)tool) ? tool.getTypeName() : "minecraft:empty";
                    act.action    = "interact";
                    act.count     = 1;
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    act.firstTime = now;
                    act.lastTime  = now;
                    act.lastPos   = ev.blockPos();
                    lm.pushAction(ev.self().getUuid(), act);
                }
            } catch (...) {}
        }
    );
    if (!interactListener_) getLogger().error("Failed to register PlayerInteractBlockEvent");

    // 启用方块破坏 Hook
    DestroyBlockHook::hook();
    getLogger().info("All event listeners and hooks registered successfully");
}

void EventListeners::unregisterAll(ll::event::EventBus& bus) {
    bus.removeListener(joinListener_);
    bus.removeListener(disconnectListener_);
    bus.removeListener(placedListener_);
    bus.removeListener(interactListener_);

    joinListener_.reset();
    disconnectListener_.reset();
    placedListener_.reset();
    interactListener_.reset();

    // 禁用方块破坏 Hook
    DestroyBlockHook::unhook();
}