-- Copyright (c) 2023 - 2024 Thakee Nathees

-- ---------------------------------------------------------------------------
-- Variables
-- ---------------------------------------------------------------------------

local root_dir_rel      = ".."
local project_name      = path.getbasename(path.getdirectory(os.getcwd()))
local build_dir         = root_dir_rel .. "/build"

local raylib_dir        = root_dir_rel .. "/thirdparty/raylib-5.0"

local raylib_bin_dir    = build_dir .. "/bin/%{cfg.buildcfg}/raylib"
local project_bin_dir   = build_dir .. "/bin/%{cfg.buildcfg}/" .. project_name

-- ---------------------------------------------------------------------------
-- Workspace
-- ---------------------------------------------------------------------------

workspace (project_name)
  location (build_dir)
  configurations { "Debug", "Release"}
  architecture "x64"
  cppdialect "C++17"
  startproject (project_name)


-- ---------------------------------------------------------------------------
-- Raylib
-- ---------------------------------------------------------------------------

-- Copied from and modified:
--   https://github.com/raylib-extras/game-premake/blob/main/raylib_premake5.lua

function platform_defines()
  defines{"PLATFORM_DESKTOP"}

  filter {"options:graphics=opengl43"}
    defines{"GRAPHICS_API_OPENGL_43"}

  filter {"options:graphics=opengl33"}
    defines{"GRAPHICS_API_OPENGL_33"}

  filter {"options:graphics=opengl21"}
    defines{"GRAPHICS_API_OPENGL_21"}

  filter {"options:graphics=opengl11"}
    defines{"GRAPHICS_API_OPENGL_11"}

  filter {"system:macosx"}
    disablewarnings {"deprecated-declarations"}

  filter {"system:linux"}
    defines {"_GNU_SOURCE"}
-- This is necessary, otherwise compilation will fail since
-- there is no CLOCK_MONOTOMIC. raylib claims to have a workaround
-- to compile under c99 without -D_GNU_SOURCE, but it didn't seem
-- to work. raylib's Makefile also adds this flag, probably why it went
-- unnoticed for so long.
-- It compiles under c11 without -D_GNU_SOURCE, because c11 requires
-- to have CLOCK_MONOTOMIC
-- See: https://github.com/raysan5/raylib/issues/2729

  filter{}
end


function link_raylib()

  links {"raylib"}

  includedirs { raylib_dir .. "/src" }
  includedirs { raylib_dir .. "/src/external" }
  includedirs { raylib_dir .. "/src/external/glfw/include" }
  platform_defines()

  filter "action:vs*"
    defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}
    dependson {"raylib"}
    links {"raylib.lib"}
    libdirs { raylib_bin_dir }
    characterset ("MBCS")

  filter "system:windows"
    defines{"_WIN32"}
    links {"winmm", "kernel32", "opengl32", "gdi32"}
    libdirs { raylib_bin_dir }

  filter "system:linux"
    links {"pthread", "GL", "m", "dl", "rt", "X11"}

  filter "system:macosx"
    links {
      "OpenGL.framework",
      "Cocoa.framework",
      "IOKit.framework",
      "CoreFoundation.framework",
      "CoreAudio.framework",
      "CoreVideo.framework"
    }

  filter{}
end


project "raylib"
  kind "StaticLib"
  platform_defines()
  location (build_dir)
  language "C"
  targetdir (raylib_bin_dir)

  filter "action:vs*"
    defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}
    characterset ("MBCS")

  filter{}

  print ("Using raylib dir " .. raylib_dir);
  
  includedirs {
    raylib_dir .. "/src",
    raylib_dir .. "/src/external/glfw/include"
  }

  vpaths {
    ["Header Files"] = { raylib_dir .. "/src/**.h"},
    ["Source Files/*"] = { raylib_dir .. "/src/**.c"},
  }

  files {
    raylib_dir .. "/src/*.h",
    raylib_dir .. "/src/*.c"
  }

  filter { "system:macosx", "files:" .. raylib_dir .. "/src/rglfw.c" }
    compileas "Objective-C"

  filter{}


-- ---------------------------------------------------------------------------
-- Main Project
-- ---------------------------------------------------------------------------

project (project_name)
  kind "ConsoleApp"
  language "C++"
  location (build_dir)
  targetdir (project_bin_dir)
  vpaths {
    ["Header Files"] = { "/src/**.h", "/src/**.hpp" },
    ["Source Files/*"] = {  "/src/**.cpp", "/src/**.cpp" },
  }

  -- Optional: Set specific configurations for Debug and Release builds
  filter "configurations:Debug"
    defines { "DEBUG" }
    symbols "On"

  filter "configurations:Release"
    defines { "NDEBUG" }
    optimize "On"

  filter {}

  -- Termbox option to support true color.
  defines { "TB_OPT_ATTR_W=32" }

  files {
    root_dir_rel .. "/src/**.c",
    root_dir_rel .. "/src/**.h",
    root_dir_rel .. "/src/**.cpp",
    root_dir_rel .. "/src/**.hpp",

    -- Thirdparty source files.
    root_dir_rel .. "/thirdparty/tomlcpp/**.c",
    root_dir_rel .. "/thirdparty/tomlcpp/**.cpp",
    root_dir_rel .. "/thirdparty/tomlcpp/**.h",
    root_dir_rel .. "/thirdparty/tomlcpp/**.hpp",
    root_dir_rel .. "/thirdparty/tree-sitter/include/**.h",
    root_dir_rel .. "/thirdparty/tree-sitter/src/lib.c",
  }

  includedirs {
    root_dir_rel .. "/src/",
    root_dir_rel .. "/include/",

    -- Thirdparty includes.
    root_dir_rel .. "/thirdparty/",
    root_dir_rel .. "/thirdparty/tree-sitter/include/",
    root_dir_rel .. "/thirdparty/tree-sitter/src/",
    root_dir_rel .. "/thirdparty/tomlcpp/",
  }

  -- Enable if needed.
  -- buildoptions { "-Wall" }

  link_raylib()


-- Copy files files after build.
postbuildcommands {
  -- "cp " .. root_dir_abs .. "/src/res/config.toml " .. project_bin_dir
}
