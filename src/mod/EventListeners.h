#pragma once
#include "ll/api/event/EventBus.h"
#include <memory>
#include "LogManager.h"

// 前向声明钩子类
struct DestroyBlockHook;

class EventListeners {
public:
    EventListeners();
    ~EventListeners();

    void registerAll(ll::event::EventBus& bus, LogManager& logManager);
    void unregisterAll(ll::event::EventBus& bus);

private:
    ll::event::ListenerPtr joinListener_;
    ll::event::ListenerPtr disconnectListener_;
    ll::event::ListenerPtr placedListener_;
    ll::event::ListenerPtr interactListener_;
    DestroyBlockHook* destroyHook_ = nullptr;   // 手动管理
};