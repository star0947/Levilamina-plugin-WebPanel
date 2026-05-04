add_rules("mode.debug", "mode.release")
add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")
option("target_type") set_default("server") set_showmenu(true) set_values("server", "client")
option_end()

add_requires("levilamina", {configs = {target_type = get_config("target_type")}})
add_requires("levibuildscript")
add_requires("cpp-httplib")
add_requires("nlohmann_json")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("WebPanel")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags("/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_defines("NOMINMAX", "UNICODE")
    add_packages("levilamina", "cpp-httplib", "nlohmann_json")
    set_exceptions("none")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")

    -- 构建后自动复制前端文件到输出目录（可选，方便本地直接得到完整插件结构）
    after_build(function (target)
        local assetsDir = path.join(os.projectdir(), "assets", "web")
        local dstDir = path.join(target:targetdir(), "..", "data", "web")
        if os.isdir(assetsDir) then
            os.mkdir(dstDir)
            os.cp(assetsDir, dstDir, {root = true})
            print("Copied web assets to " .. dstDir)
        end
    end)