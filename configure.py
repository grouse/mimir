#!/usr/bin/env python3
import os
import sys
import argparse

sourcedir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.join(sourcedir, "tools/enki"))
from enki import *

parser = argparse.ArgumentParser("configure.py", description="generates build files")
parser.add_argument("-o", "--out", default="build", help="build generation output directory")
parser.add_argument("--debug", action="store_true", help="compile with debug info")
parser.add_argument("--optimize", action="store_true", help="turn on compiler optimization")
parser.add_argument("-r", "--render", choices=["opengl"], default="opengl", help="choose render backend")
args = parser.parse_args();

host_os   = sys.platform
target_os = host_os

build = Ninja("build", sourcedir, sys.argv[1:], target_os)

### toolchain
if host_os == "win32":
    build.rule("cc", "clang -D_DLL -MMD -MF $out.d $cflags $ccflags $flags -c $in -o $out",
               description = "CC $in",
               depfile = "$out.d")
    build.rule("cxx", "clang++ -D_DLL -MMD -MF $out.d $cflags $cxxflags $flags -c $in -o $out",
               description = "CXX $in",
               depfile = "$out.d")
    build.rule("link", "clang++ -Wl,-nodefaultlib:libcmt -D_DLL -lmsvcrt $linkflags -o$out $in $libs",
               description = "LINK $out")
    build.rule("ar", "lld-link.exe /lib \"/OUT:$out\" /nologo $arflags $in",
               description = "LIB $out")
    build.rule("touch", command = "cmd /c type nul > \"$out\"",
               description = "touch $out")

if host_os == "linux":
    build.rule("cc", "clang -MMD -MF $out.d $cflags $ccflags $flags -c $in -o $out",
               description = "CXX $in",
               depfile = "$out.d")
    build.rule("cxx", "clang++ -MMD -MF $out.d $cflags $cxxflags $flags -c $in -o $out",
               description = "CXX $in",
               depfile = "$out.d")
    build.rule("link", "clang++ $linkflags $libs -o$out $in",
               description = "LINK $out")
    build.rule("ar", "ar -rcs $arflags $out $in",
               description = "LIB $out")
    build.rule("touch", command = "touch $out",
               description = "touch $out")



### default flags
build.flags["c"].extend([ "-Wall" ])
build.flags["cxx"].extend([
    "-std=c++20",

    "-fno-rtti",
    "-fno-exceptions",

    "-Wno-missing-braces",
    "-Wno-char-subscripts",
    "-Wno-c99-designator",
    "-Wno-missing-field-initializers",
    "-Wno-non-c-typedef-for-linkage",
    "-Wno-unused-function",
    "-Wno-initializer-overrides",
])

if args.debug:
    build.flags["c"].append("-g")
    build.flags["link"].append("-g")

if args.optimize:
    build.flags["c"].append("-O2")

ext_optimised_flags = { "c" : [ "-O2", "-Wno-everything" ] }

### tree-sitter
tree_sitter_modules = []


tree_sitter = build.library("tree-sitter", "$root/external/tree-sitter-0.20.8", flags = ext_optimised_flags)
include_path(tree_sitter, "lib/include", public = True)
cc(tree_sitter, "lib/src/lib.c")
tree_sitter_modules.append(tree_sitter)

## tree-sitter-cpp
tree_sitter_cpp = build.library("tree-sitter-cpp", "$root/external/tree-sitter-cpp-master", ext_optimised_flags)
dep(tree_sitter_cpp, tree_sitter)
cc(tree_sitter_cpp, "src/parser.c")
cxx(tree_sitter_cpp, "src/scanner.cc")
tree_sitter_modules.append(tree_sitter_cpp)

## tree-sitter-rust
tree_sitter_rust = build.library("tree-sitter-rust", "$root/external/tree-sitter-rust-0.20.1", ext_optimised_flags)
dep(tree_sitter_rust, tree_sitter)
cc(tree_sitter_rust, ["src/parser.c", "src/scanner.c"])
tree_sitter_modules.append(tree_sitter_rust)

## tree-sitter-bash
tree_sitter_bash = build.library("tree-sitter-bash", "$root/external/tree-sitter-bash-master", ext_optimised_flags)
dep(tree_sitter_bash, tree_sitter)
cc(tree_sitter_bash, "src/parser.c")
cxx(tree_sitter_bash, "src/scanner.cc")
tree_sitter_modules.append(tree_sitter_bash)

## tree-sitter-csharp
tree_sitter_csharp = build.library("tree-sitter-csharp", "$root/external/tree-sitter-c-sharp-0.19.1", ext_optimised_flags)
dep(tree_sitter_csharp, tree_sitter)
cc(tree_sitter_csharp, ["src/parser.c", "src/scanner.c"])
tree_sitter_modules.append(tree_sitter_csharp)

## tree-sitter-lua
tree_sitter_lua = build.library("tree-sitter-lua", "$root/external/tree-sitter-lua-master", ext_optimised_flags)
dep(tree_sitter_lua, tree_sitter)
cc(tree_sitter_lua, ["src/parser.c", "src/scanner.c"])
tree_sitter_modules.append(tree_sitter_lua)

## tree-sitter-comment
tree_sitter_comment = build.library("tree-sitter-comment", "$root/external/tree-sitter-comment-master", ext_optimised_flags)
include_path(tree_sitter_comment, "src")
dep(tree_sitter_comment, tree_sitter)
cc(tree_sitter_comment, ["src/parser.c", "src/scanner.c"])
tree_sitter_modules.append(tree_sitter_comment)

### mjson
mjson = build.library("mjson", "$root/external/mjson", ext_optimised_flags)
include_path(mjson, "src", public=True)
cc(mjson, [ "src/mjson.c" ])

### core
core = build.library("core", "$root/src/core")
include_path(core, ["$root", "$root/external"])
if host_os == "win32": define(core, "_CRT_SECURE_NO_WARNINGS");

meta(core, "lexer.cpp")
meta(core, "ini.cpp")
meta(core, "maths.cpp")
meta(core, "string.cpp")
meta(core, "window.cpp")
meta(core, "thread.cpp")
meta(core, "process.cpp")
meta(core, "core.cpp")
meta(core, "file.cpp")
meta(core, "assets.cpp")

cxx(core, "core.cpp")
cxx(core, "memory.cpp")
cxx(core, "string.cpp")
cxx(core, "thread.cpp")
cxx(core, "file.cpp")
cxx(core, "ini.cpp")
cxx(core, "lexer.cpp")
cxx(core, "window.cpp")
cxx(core, "maths.cpp")
cxx(core, "process.cpp")
cxx(core, "assets.cpp")


### mimir
mimir = build.executable("mimir", "$root/src")
dep(mimir, tree_sitter_modules)
dep(mimir, [ core, mjson ])

define(mimir, "APP_INPUT_ID_START=0x100")
define(mimir, "GUI_INPUT_ID_START=0x200")
define(mimir, "EDITOR_INPUT_ID_START=0x300")
define(mimir, "ASSETS_DIR=\"$root/assets\"")
define(mimir, ["STBI_NO_LINEAR", "STBI_NO_HDR"])

if host_os == "win32": define(mimir, "_CRT_SECURE_NO_WARNINGS", public=True);

include_path(mimir, ["$root/src", "$root", "$root/external", "$builddir"], public = True)

cxx(mimir, "mimir.cpp")
cxx(mimir, "font.cpp")

meta(mimir, "mimir.cpp")
meta(mimir, "gui.cpp")
meta(mimir, "font.cpp")

if args.render == "opengl":
    gl_defines = define(mimir, "GFX_OPENGL")

    meta(mimir, "gfx_opengl.cpp")
    cxx(mimir, "gfx_opengl.cpp")

    gl_libs = []
    if target_os == "win32":
        gl_libs.extend(lib(mimir, "opengl32"))

    if target_os == "linux":
        gl_libs.extend(lib(mimir, "GLX"))

    define(core, gl_defines)
    lib(core, gl_libs)


if target_os == "win32":
    cxx(mimir, "win32_main.cpp")
    cxx(mimir, "core/win32_opengl.cpp")

    lib(mimir, "user32")
    lib(mimir, "shell32")
    lib(mimir, "gdi32")
    lib(mimir, "shlwapi")
    lib(mimir, "Xinput")

if target_os == "linux":
    cxx(mimir, "linux_main.cpp")
    cxx(mimir, "core/linux_opengl.cpp")

    lib(mimir, "X11")
    lib(mimir, "Xi")

test = build.test(mimir, "$root/src/core")
include_path(test, "$root/src")
cxx(test, "tests.cpp")
meta(test, "tests.cpp", flags = [ "--tests" ])

build.default = mimir;
build.generate()
