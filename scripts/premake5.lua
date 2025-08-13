-- Copyright (c) 2023 - 2024 Thakee Nathees

-- ---------------------------------------------------------------------------
-- Variables
-- ---------------------------------------------------------------------------

local root_dir_rel = ".."
local project_name = path.getbasename(path.getdirectory(os.getcwd()))
local build_dir    = root_dir_rel .. "/build"
local bin_dir      = build_dir .. "/bin/%{cfg.buildcfg}/" .. project_name

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
-- Project
-- ---------------------------------------------------------------------------

project (project_name)
  kind "ConsoleApp"
  language "C++"
  location (build_dir)
  targetdir (bin_dir)
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
  characterset ("ASCII")

  files {
    root_dir_rel .. "/src/**.c",
    root_dir_rel .. "/src/**.h",
    root_dir_rel .. "/src/**.cpp",
    root_dir_rel .. "/src/**.hpp",

    -- Thirdparty source files.
    root_dir_rel .. "/thirdparty/termbox/*.c",
    root_dir_rel .. "/thirdparty/termbox/*.h",
    root_dir_rel .. "/thirdparty/tree-sitter-0.22.6/include/**.h",
    root_dir_rel .. "/thirdparty/tree-sitter-0.22.6/src/lib.c",
  }

  includedirs {
    root_dir_rel .. "/src/",

    -- Thirdparty includes.
    root_dir_rel .. "/thirdparty/",
    root_dir_rel .. "/thirdparty/termbox/",
    root_dir_rel .. "/thirdparty/tree-sitter-0.22.6/include/",
    root_dir_rel .. "/thirdparty/tree-sitter-0.22.6/src/",
  }

  -- Enable if needed.
  -- buildoptions { "-Wall" }


-- Copy files files after build.
postbuildcommands {
  -- "cp " .. root_dir_abs .. "/src/res/config.toml " .. bin_dir
}
