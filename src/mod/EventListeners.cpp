#include "EventListeners.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/io/Logger.h"
#include "ll/api/mod/NativeMod.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/IGameModeMessenger.h"   // GameMode 成员完整类型
#include "mc/world/gamemode/IGameModeTimer.h"       // GameMode 成员完整类型
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

// === 自定义 Hook：在方块被破坏 *之前* 捕获方块信息 ===
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
    // 从全局指针获取 LogManager（多线程安全读取）
    if (auto* lm = g_logManagerForHook.load()) {
        try {
            auto& player = this->mPlayer;                    // GameMode 内部持有的玩家引用
            ItemStack const& tool = player.getSelectedItem(); // 玩家当前手持物品

            AggregatedBlockAction act;
            act.blockType = block.getTypeName();              // 被破坏的真实方块名
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

    // 始终调用原始函数
    return origin(block, pos, itemBeforeEvent);
}

// === EventListeners 生命周期 ===

EventListeners::EventListeners() = default;

EventListeners::~EventListeners() {
    if (destroyHook_) {
        destroyHook_->unhook();
        delete destroyHook_;
        destroyHook_ = nullptr;
    }
}

static ll::io::Logger& getLogger() {
    return ll::mod::NativeMod::current()->getLogger();
}

void EventListeners::registerAll(ll::event::EventBus& bus, LogManager& lm) {
    // 1. 玩家加入/离开事件
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

    // 2. 方块放置事件（直接使用 LeviLamina 封装事件）
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

    // 4. 方块破坏 Hook（替换 EventBus 监听，因为原事件不暴露被破坏的方块信息）
    if (!destroyHook_) {
        destroyHook_ = new DestroyBlockHook();
        destroyHook_->hook();
    }
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

    // 卸载方块破坏 Hook
    if (destroyHook_) {
        destroyHook_->unhook();
        delete destroyHook_;
        destroyHook_ = nullptr;
    }
}