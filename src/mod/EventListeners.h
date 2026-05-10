#pragma once
#include "ll/api/event/EventBus.h"
#include <memory>
#include "LogManager.h"

class EventListeners {
public:
    void registerAll(ll::event::EventBus& bus, LogManager& logManager);
    void unregisterAll(ll::event::EventBus& bus);

private:
    ll::event::ListenerPtr joinListener_;
    ll::event::ListenerPtr disconnectListener_;
    ll::event::ListenerPtr placedListener_;
    ll::event::ListenerPtr interactListener_;
    // 方块破坏使用原始 Hook，不通过 EventBus
};