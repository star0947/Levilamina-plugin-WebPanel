-- 设置构建模式（debug/release）
add_rules("mode.debug", "mode.release")

-- 添加 LeviLamina 官方仓库
add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- 定义命令行选项（支持 workflow 中的 --target_type=server）
option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- 声明依赖项
add_requires("levilamina", {configs = {target_type = get_config("target_type")}})
add_requires("levibuildscript")
add_requires("nlohmann_json")
add_requires("cpp-httplib")

-- 设置运行时库（若未指定则默认为 /MD）
if not has_config("vs_runtime") then
    set_runtimes("MD")
end

-- 定义构建目标
target("WebPanel")
    -- 添加 LeviLamina 构建规则（用于自动打包 mod）
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")

    -- 设置为动态库 (.dll)
    set_kind("shared")
    -- 设置语言标准
    set_languages("c++20")
    -- 设置异常处理模式（与 /EHa 配合）
    set_exceptions("none")

    -- 添加源文件和头文件
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    add_includedirs("include")  -- 让编译器能找到 httplib.h

    -- 添加依赖包
    add_packages("levilamina", "nlohmann_json", "cpp-httplib")

    -- 编译选项（官方推荐）
    add_cxflags("/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    -- 预定义宏
    add_defines("NOMINMAX", "UNICODE")
    -- 调试符号
    set_symbols("debug")