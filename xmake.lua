
set_languages("c++latest")

add_cxflags("/utf-8")

target("dfsearch")
    set_kind("binary")
    set_optimize("fastest")
    set_warnings("more")
    add_files("src/main.cpp")

target("fastdfs")
    set_kind("binary")
    set_optimize("fastest")
    set_warnings("more")
    add_files("src/fast.cpp")
