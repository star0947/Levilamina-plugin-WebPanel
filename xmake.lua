-- 1. 配置规则和仓库
add_rules("mode.debug", "mode.release")
add_repositories("levilamina-repo https://github.com/LiteLDev/xmake-repo.git")

-- 2. 添加依赖包
add_requires("levilamina")

-- 3. 定义构建目标
target("WebPanel")
    -- 设置为动态库 (.dll / .so)
    set_kind("shared")
    -- 添加源文件
    add_files("webpanel.cpp")
    -- 添加依赖包
    add_packages("levilamina")
    -- 设置语言标准 (LeviLamina 需要 C++20)
    set_languages("c++20")
    -- 设置输出文件名
    set_filename("WebPanel.dll")