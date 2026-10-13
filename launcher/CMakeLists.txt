# RDoom launcher.
#   rdoom_launcher_core: portable logic (profiles, library, launch plan). Built
#                        for the Switch and for host tests.
#   rdoom_launcher_ui:   Switch-only front end (libnx framebuffer + HID).
cmake_minimum_required(VERSION 3.16)
project(rdoom_launcher CXX C)

set(RDOOM_LAUNCHER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(rdoom_launcher_core STATIC
	src/core/log.cpp
	src/core/fsutil.cpp
	src/core/paths.cpp
	src/core/archive.cpp
	src/core/content.cpp
	src/core/profile.cpp
	src/core/library.cpp
	src/core/launch.cpp
	src/core/stb_impl.cpp)
target_include_directories(rdoom_launcher_core PUBLIC src/core third_party)
# third-party single-header code: keep its warnings out of the build log
set_source_files_properties(src/core/stb_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_compile_features(rdoom_launcher_core PUBLIC cxx_std_17)

add_library(rdoom_launcher_ui STATIC
	src/ui/canvas.cpp
	src/ui/text.cpp
	src/ui/input.cpp
	src/ui/thumbs.cpp
	src/ui/app.cpp
	src/ui/app_screens.cpp)
target_include_directories(rdoom_launcher_ui PUBLIC src/ui)
target_link_libraries(rdoom_launcher_ui PUBLIC rdoom_launcher_core)
set_source_files_properties(src/ui/text.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-function")

if(NINTENDO_SWITCH)
	# the libnx front end (framebuffer, HID, swkbd) lives with the engine's
	# Switch platform code, so it is built there.
	target_include_directories(rdoom_launcher_core PRIVATE $ENV{PORTLIBS_PREFIX}/include)
else()
	find_package(ZLIB REQUIRED)
	target_link_libraries(rdoom_launcher_core PUBLIC ZLIB::ZLIB)
endif()
