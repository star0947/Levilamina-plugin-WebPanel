#include "EventListeners.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/memory/Memory.h"
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

static ll::io::Logger& getLogger() {
    return ll::mod::NativeMod::current()->getLogger();
}

// ===== 原始 Hook：破坏方块 =====
// 代理类，不继承 GameMode，避免构造函数问题
struct GameModeHookProxy {
    GameModeHookProxy() = delete;

    // 函数签名与 GameMode::_sendTryDestroyBlockEvent 完全一致
    std::optional<ItemStack> detour(
        Block const&    block,
        BlockPos const& pos,
        ItemStack       itemBeforeEvent
    ) const;
};

// 保存原始函数的跳板地址
static ll::memory::FuncPtr s_origBreakFunc = nullptr;

std::optional<ItemStack> GameModeHookProxy::detour(
    Block const&    block,
    BlockPos const& pos,
    ItemStack       itemBeforeEvent
) const {
    // 在运行时，this 实际是 const GameMode*
    const GameMode* gm     = reinterpret_cast<const GameMode*>(this);
    Player&         player = gm->mPlayer;   // TypedStorage<8,8,Player&> == Player&

    if (auto* lm = g_logManagerForHook.load()) {
        try {
            ItemStack const& tool = player.getSelectedItem();

            AggregatedBlockAction act;
            act.blockType = block.getTypeName();     // 真正的被破坏方块名
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

    // 调用原始函数，需要把s_origPtr还原成成员函数指针
    using OrigFn = std::optional<ItemStack> (GameModeHookProxy::*)(
        Block const&, BlockPos const&, ItemStack) const;
    OrigFn orig;
    std::memcpy(&orig, &s_origBreakFunc, sizeof(orig));
    return (this->*orig)(block, pos, itemBeforeEvent);
}

// ===== 普通事件监听 =====
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

    // 原始 Hook：安装方块破坏 Hook
    ll::memory::FuncPtr target = ll::memory::toFuncPtr(&GameMode::_sendTryDestroyBlockEvent);
    ll::memory::FuncPtr detour = ll::memory::toFuncPtr(&GameModeHookProxy::detour);
    ll::memory::hook(target, detour, &s_origBreakFunc, ll::memory::HookPriority::Normal);
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

    // 卸载方块破坏 Hook
    ll::memory::FuncPtr target = ll::memory::toFuncPtr(&GameMode::_sendTryDestroyBlockEvent);
    ll::memory::FuncPtr detour = ll::memory::toFuncPtr(&GameModeHookProxy::detour);
    ll::memory::unhook(target, detour);
}