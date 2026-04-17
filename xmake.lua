-- 设置构建模式（debug/release）
add_rules("mode.debug", "mode.release")

-- 添加 LeviLamina 官方仓库
add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- 声明依赖项
add_requires("levilamina", {configs = {target_type = "server"}})
add_requires("nlohmann_json")

add_requires("cpp-httplib")

-- 定义构建目标
target("WebPanel")
    -- 设置为动态库 (.dll)
    set_kind("shared")
    -- 添加源文件
    add_files("src/**.cpp")
    -- 添加头文件搜索路径
    add_includedirs("include") -- 让编译器能找到 httplib.h
    -- 添加依赖包
    add_packages("levilamina", "nlohmann_json")
    -- 设置语言标准
    set_languages("c++20")
    -- 定义编译宏
    add_defines("NOMINMAX", "UNICODE")
    add_packages("levilamina", "cpp-httplib")