// 为 DestroyBlockHook 的基类 GameMode 提供哑元构造函数定义。
// DestroyBlockHook 仅在编译期需要此符号链接，运行时从不实例化 GameMode 子类对象，
// 故该函数体为空，且永远不会被调用。

#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/IGameModeMessenger.h"
#include "mc/world/gamemode/IGameModeTimer.h"

GameMode::GameMode() {}s