set_project("Arke-Android")
set_version("0.1.1")
set_languages("cxx20", "c11")

add_rules("mode.debug", "mode.release")

add_includedirs("include", "third_party")

target("arke_android")
    set_kind("shared")
    add_files("src/core/*.cpp")
    add_files("src/shader/*.cpp")
    add_files("src/platform/android/*.cpp")

    if is_plat("android") then
        add_syslinks("log", "android", "EGL", "GLESv3")
    else
        add_syslinks("dl", "pthread")
    end

target("test_shader_transpile")
    set_kind("binary")
    add_files("tests/test_shader_transpile.cpp")
    add_files("src/core/*.cpp")
    add_files("src/shader/*.cpp")
    add_syslinks("dl", "pthread")
