-- 全局构建规则和模式
add_rules("mode.debug", "mode.release")

-- 全局第三方仓库（必须放在全局作用域）
add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- 全局配置选项（与 build.yml 中 --target_type=server 联动）
option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- 全局依赖（根据选项自动拉取）
add_requires("levilamina", {configs = {target_type = get_config("target_type")}})
add_requires("levibuildscript")

-- 运行时设置
if not has_config("vs_runtime") then
    set_runtimes("MD")
end

-- 目标模块（只放目标属性）
target("WebPanel")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags("/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_defines("NOMINMAX", "UNICODE")
    add_packages("levilamina")
    set_exceptions("none")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")