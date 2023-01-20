
set_languages("c++latest")

set_policy("build.c++.modules", true)

add_requires("xxhash")
add_packages("xxhash")

if is_os("windows") then
    add_defines("__WINDOWS__")
end

add_cxflags("/utf-8")

target("check")
    set_kind("binary")
    set_optimize("fastest")
    set_warnings("more")
    add_files("src/main.cpp")
