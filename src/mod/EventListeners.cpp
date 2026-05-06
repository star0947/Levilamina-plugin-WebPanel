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

// === 全局 LogManager 指针（由 WebPanelMod 在 enable/disable 时设置） ===
extern std::atomic<LogManager*> g_logManagerForHook;

// === 方块破坏 Hook（不继承 GameMode，用强转访问成员） ===
LL_AUTO_INSTANCE_HOOK(
    DestroyBlockHook,
    ll::memory::HookPriority::Normal,
    &GameMode::_sendTryDestroyBlockEvent,
    ::std::optional<::ItemStack>,
    ::Block const&    block,
    ::BlockPos const& pos,
    ::ItemStack       itemBeforeEvent
) {
    // this 实际是 GameMode*，安全强转
    GameMode* gm = reinterpret_cast<GameMode*>(this);
    Player& player = gm->mPlayer; // TypedStorage 隐式转换为 Player&

    // 从全局指针获取 LogManager（多线程安全读取）
    if (auto* lm = g_logManagerForHook.load()) {
        try {
            ItemStack const& tool = player.getSelectedItem();

            AggregatedBlockAction act;
            act.blockType = block.getTypeName();   // 真实的被破坏方块名
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

// === EventListeners 成员函数 ===

static ll::io::Logger& getLogger() {
    return ll::mod::NativeMod::current()->getLogger();
}

void EventListeners::registerAll(ll::event::EventBus& bus, LogManager& lm) {
    // 1. 玩家加入/离开
    joinListener_ = bus.emplaceListener<ll::event::PlayerJoinEvent>(
        [&lm](ll::event::PlayerJoinEvent& ev) {
            try { lm.onPlayerJoin(ev.self().getUuid()); } catch (...) {}
        }
    );
    if (!joinListener_) getLogger().error("Failed to register PlayerJoinEvent");

    disconnectListener_ = bus.emplaceListener<ll::event::PlayerDisconnectEvent>(
        [&lm](ll::event::PlayerDisconnectEvent& ev) {
            try { lm.onPlayerLeave(ev.self().getUuid()); } catch (...) {}
        }
    );
    if (!disconnectListener_) getLogger().error("Failed to register PlayerDisconnectEvent");

    // 2. 方块放置事件
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

    // 3. 方块交互事件
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

    // 4. 方块破坏 Hook 由 LL_AUTO_INSTANCE_HOOK 自动注册，无需手动操作
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

    // LL_AUTO_INSTANCE_HOOK 会自动在插件卸载时取消 Hook，无需手动 unhook
}