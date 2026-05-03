#pragma once
#include "ll/api/event/EventBus.h"
#include <memory>

class LogManager;

class EventListeners {
public:
    static void registerAll(ll::event::EventBus& bus, LogManager& logManager);
    static void unregisterAll(ll::event::EventBus& bus);

private:
    static ll::event::ListenerPtr joinListener_;
    static ll::event::ListenerPtr disconnectListener_;
    static ll::event::ListenerPtr destroyListener_;
    static ll::event::ListenerPtr placedListener_;
    static ll::event::ListenerPtr interactListener_;
    static LogManager* logManager_;
};