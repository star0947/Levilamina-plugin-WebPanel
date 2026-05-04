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
            logManager_->onPlayerJoin(ev.self().getUuid());
        }
    );

    disconnectListener_ = bus.emplaceListener<ll::event::PlayerDisconnectEvent>(
        [](ll::event::PlayerDisconnectEvent& ev) {
            logManager_->onPlayerLeave(ev.self().getUuid());
        }
    );

    destroyListener_ = bus.emplaceListener<ll::event::PlayerDestroyBlockEvent>(
        [](ll::event::PlayerDestroyBlockEvent& ev) {
            auto& player = ev.self();
            auto& bs = player.getDimensionBlockSource();
            Block const& block = bs.getBlock(ev.pos());
            ItemStack const& tool = player.getSelectedItem();

            AggregatedBlockAction act;
            act.blockType = block.getTypeName();
            act.toolType = ((bool)tool) ? tool.getTypeName() : "minecraft:empty";
            act.action = "break";
            act.count = 1;
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            act.firstTime = now;
            act.lastTime = now;
            act.lastPos = ev.pos();
            logManager_->pushAction(player.getUuid(), act);
        }
    );

    placedListener_ = bus.emplaceListener<ll::event::PlayerPlacedBlockEvent>(
        [](ll::event::PlayerPlacedBlockEvent& ev) {
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
        }
    );

    interactListener_ = bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
        [](ll::event::PlayerInteractBlockEvent& ev) {
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