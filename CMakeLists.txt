cmake_minimum_required (VERSION 3.2 FATAL_ERROR)
project ("LEAF" C CXX)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

include (MacroEnsureOutOfSourceBuild)
include (MacroAddUninstallTarget)
include (MacroGitVersionInfo)
include (MacroCreateConfigFiles)

macro_ensure_out_of_source_build ("Do not build LEAF in the source directory.")

# Used by most of the actual tools, so find them up-front.
find_package (Quick-DER 1.2 REQUIRED)
find_package (LillyDAP 0.9 REQUIRED)

add_subdirectory (src/lib)
add_subdirectory (src/messagelog)

