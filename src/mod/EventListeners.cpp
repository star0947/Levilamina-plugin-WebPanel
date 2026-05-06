#include "EventListeners.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/memory/Hook.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/item/ItemStack.h"
#include "ll/api/io/Logger.h"
#include "ll/api/mod/NativeMod.h"
#include "Config.h"
#include <chrono>
#include <optional>
#include <atomic>

// 全局 LogManager 指针，由 WebPanelMod::enable/disable 设置
extern std::atomic<LogManager*> g_logManagerForHook;

static ll::io::Logger& getLogger() {
    return ll::mod::NativeMod::current()->getLogger();
}

// 自定义钩子：在方块破坏前捕获方块信息
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
    // 获取当前玩家（GameMode 的 mPlayer 成员）
    auto& player = this->mPlayer;
    if (auto* lm = g_logManagerForHook.load()) {
        try {
            // 获取主手物品（破坏时手中的实际工具）
            ItemStack const& tool = player.getSelectedItem();
            AggregatedBlockAction act;
            act.blockType = block.getTypeName();  // 真正的破坏前方块
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

EventListeners::EventListeners() = default;
EventListeners::~EventListeners() {
    // 确保钩子被卸载
    if (destroyHook_) {
        destroyHook_->unhook();
        delete destroyHook_;
        destroyHook_ = nullptr;
    }
}

void EventListeners::registerAll(ll::event::EventBus& bus, LogManager& lm) {
    // 注册加入/离开/放置/交互事件（和之前一样，但移除了 destroyListener）
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

    // 注册方块破坏钩子（替代原来的 EventBus 监听）
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

    // 卸载方块破坏钩子
    if (destroyHook_) {
        destroyHook_->unhook();
        delete destroyHook_;
        destroyHook_ = nullptr;
    }
}