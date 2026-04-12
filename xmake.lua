add_rules("mode.debug", "mode.release")

local sdkdir = get_config("sdk") or "./sdk"

target("WebPanel")
    set_kind("shared")
    add_files("webpanel.cpp")
    set_languages("c++20")
    add_includedirs(sdkdir .. "/include")
    add_linkdirs(sdkdir .. "/lib")
    add_links("LLAPI")