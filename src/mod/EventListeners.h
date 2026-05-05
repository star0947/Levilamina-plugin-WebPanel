#pragma once
#include "ll/api/event/EventBus.h"
#include <memory>
#include "LogManager.h"

class EventListeners {
public:
    EventListeners() = default;
    ~EventListeners() = default;

    void registerAll(ll::event::EventBus& bus, LogManager& logManager);
    void unregisterAll(ll::event::EventBus& bus);

private:
    ll::event::ListenerPtr joinListener_;
    ll::event::ListenerPtr disconnectListener_;
    ll::event::ListenerPtr destroyListener_;
    ll::event::ListenerPtr placedListener_;
    ll::event::ListenerPtr interactListener_;
    LogManager* logManager_ = nullptr;
};