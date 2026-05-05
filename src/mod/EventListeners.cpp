#include "EventListeners.h"
#include "LogManager.h"
#include "Config.h"

#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerDestroyBlockEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/BlockSource.h"

#include <chrono>

LogManager* EventListeners::logManager_ = nullptr;

ll::event::ListenerPtr EventListeners::joinListener_;
ll::event::ListenerPtr EventListeners::disconnectListener_;
ll::event::ListenerPtr EventListeners::destroyListener_;
ll::event::ListenerPtr EventListeners::placedListener_;
ll::event::ListenerPtr EventListeners::interactListener_;

void EventListeners::registerAll(ll::event::EventBus& bus, LogManager& lm) {
    logManager_ = &lm;

    joinListener_ = bus.emplaceListener<ll::event::PlayerJoinEvent>(
        [](ll::event::PlayerJoinEvent& ev) {
            try {
                logManager_->onPlayerJoin(ev.self().getUuid());
            } catch (...) {}
        }
    );

    disconnectListener_ = bus.emplaceListener<ll::event::PlayerDisconnectEvent>(
        [](ll::event::PlayerDisconnectEvent& ev) {
            try {
                logManager_->onPlayerLeave(ev.self().getUuid());
            } catch (...) {}
        }
    );

    destroyListener_ = bus.emplaceListener<ll::event::PlayerDestroyBlockEvent>(
        [](ll::event::PlayerDestroyBlockEvent& ev) {
            try {
                auto& player = ev.self();
                ItemStack const& tool = player.getSelectedItem();

                AggregatedBlockAction act;
                // 方块已被破坏，无法获取原类型，记录为 "air"
                act.blockType = "minecraft:air";
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

    placedListener_ = bus.emplaceListener<ll::event::PlayerPlacedBlockEvent>(
        [](ll::event::PlayerPlacedBlockEvent& ev) {
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

    interactListener_ = bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
        [](ll::event::PlayerInteractBlockEvent& ev) {
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
}

void EventListeners::unregisterAll(ll::event::EventBus& bus) {
    bus.removeListener(joinListener_);
    bus.removeListener(disconnectListener_);
    bus.removeListener(destroyListener_);
    bus.removeListener(placedListener_);
    bus.removeListener(interactListener_);
    logManager_ = nullptr;
}