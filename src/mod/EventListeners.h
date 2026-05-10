#pragma once
#include "ll/api/event/EventBus.h"
#include <memory>
#include "LogManager.h"
#include "DeathLogger.h"

class EventListeners {
public:
    void registerAll(ll::event::EventBus& bus, LogManager& logManager, DeathLogger& deathLogger);
    void unregisterAll(ll::event::EventBus& bus);

private:
    ll::event::ListenerPtr joinListener_;
    ll::event::ListenerPtr disconnectListener_;
    ll::event::ListenerPtr destroyListener_;
    ll::event::ListenerPtr placedListener_;
    ll::event::ListenerPtr interactListener_;
    ll::event::ListenerPtr dieListener_;               // 新增
};