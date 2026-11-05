set_xmakever("3.0.0")
set_project("HorseReins")
set_version("0.1.0")
set_arch("x64")
set_languages("c++23")
set_encodings("utf-8")
set_warnings("allextra")
add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")

set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", false)

includes("lib/commonlibsse-ng")

target("HorseReins", function()
    add_deps("commonlibsse-ng")
    add_rules("commonlibsse-ng.plugin", {
        name = "HorseReins",
        author = "HorseReins",
        description = "Horse ropes for Skyrim SE and AE"
    })
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "lib/menu-framework")
    set_pcxxheader("src/PCH.h")
end)

