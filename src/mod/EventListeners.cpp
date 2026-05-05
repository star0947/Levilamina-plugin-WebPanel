#include "EventListeners.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerDestroyBlockEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/BlockSource.h"
#include "ll/api/io/Logger.h"
#include <chrono>

// 辅助函数：通过 event.self() 获取 Player& 并获取 NativeMod 的 Logger
static ll::io::Logger& getLogger() {
    return ll::mod::NativeMod::current()->getLogger();
}

void EventListeners::registerAll(ll::event::EventBus& bus, LogManager& lm) {
    logManager_ = &lm;

    // 注册加入事件
    joinListener_ = bus.emplaceListener<ll::event::PlayerJoinEvent>(
        [this](ll::event::PlayerJoinEvent& ev) {
            try {
                logManager_->onPlayerJoin(ev.self().getUuid());
            } catch (...) {}
        }
    );
    if (!joinListener_) {
        getLogger().error("Failed to register PlayerJoinEvent");
    }

    // 注册离开事件
    disconnectListener_ = bus.emplaceListener<ll::event::PlayerDisconnectEvent>(
        [this](ll::event::PlayerDisconnectEvent& ev) {
            try {
                logManager_->onPlayerLeave(ev.self().getUuid());
            } catch (...) {}
        }
    );
    if (!disconnectListener_) {
        getLogger().error("Failed to register PlayerDisconnectEvent");
    }

    // 注册破坏方块事件
    destroyListener_ = bus.emplaceListener<ll::event::PlayerDestroyBlockEvent>(
        [this](ll::event::PlayerDestroyBlockEvent& ev) {
            try {
                auto& player = ev.self();
                ItemStack const& tool = player.getSelectedItem();
                AggregatedBlockAction act;
                act.blockType = "minecraft:air"; // 方块已被破坏，记录空气
                act.toolType = ((bool)tool) ? tool.getTypeName() : "minecraft:empty";
                act.action = "break";
                act.count = 1;
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                act.firstTime = now;
                act.lastTime = now;
                act.lastPos = ev.pos();
                logManager_->pushAction(player.getUuid(), act);
            } catch (...) {}
        }
    );
    if (!destroyListener_) {
        getLogger().error("Failed to register PlayerDestroyBlockEvent");
    }

    // 注册放置方块事件
    placedListener_ = bus.emplaceListener<ll::event::PlayerPlacedBlockEvent>(
        [this](ll::event::PlayerPlacedBlockEvent& ev) {
            try {
                auto& player = ev.self();
                Block const& block = ev.placedBlock();
                ItemStack const& tool = player.getSelectedItem();
                AggregatedBlockAction act;
                act.blockType = block.getTypeName();
                act.toolType = ((bool)tool) ? tool.getTypeName() : "minecraft:empty";
                act.action = "place";
                act.count = 1;
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                act.firstTime = now;
                act.lastTime = now;
                act.lastPos = ev.pos();
                logManager_->pushAction(player.getUuid(), act);
            } catch (...) {}
        }
    );
    if (!placedListener_) {
        getLogger().error("Failed to register PlayerPlacedBlockEvent");
    }

    // 注册交互方块事件
    interactListener_ = bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
        [this](ll::event::PlayerInteractBlockEvent& ev) {
            try {
                auto& player = ev.self();
                if (auto block = ev.block()) {
                    auto& tool = ev.item();
                    AggregatedBlockAction act;
                    act.blockType = block->getTypeName();
                    act.toolType = ((bool)tool) ? tool.getTypeName() : "minecraft:empty";
                    act.action = "interact";
                    act.count = 1;
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    act.firstTime = now;
                    act.lastTime = now;
                    act.lastPos = ev.blockPos();
                    logManager_->pushAction(player.getUuid(), act);
                }
            } catch (...) {}
        }
    );
    if (!interactListener_) {
        getLogger().error("Failed to register PlayerInteractBlockEvent");
    }
}

void EventListeners::unregisterAll(ll::event::EventBus& bus) {
    bus.removeListener(joinListener_);
    bus.removeListener(disconnectListener_);
    bus.removeListener(destroyListener_);
    bus.removeListener(placedListener_);
    bus.removeListener(interactListener_);

    // 重置指针，允许安全析构
    joinListener_.reset();
    disconnectListener_.reset();
    destroyListener_.reset();
    placedListener_.reset();
    interactListener_.reset();
    logManager_ = nullptr;
}