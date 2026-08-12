# SPDX-License-Identifier: GPL-2.0
#
# suunto2subsurface links statically against a prebuilt subsurface_corelib
# (built from vendor/subsurface, see vendor/README.md), the same way
# subsurface's own smtk-import/CMakeLists.txt consumes it.

cmake_minimum_required(VERSION 3.16)
project(suunto2subsurface CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Must match vendor/subsurface's own CMAKE_CXX_STANDARD (20): our sources
# directly include its headers (core/units.h, core/gas.h, ...), which use
# C++20 designated initializers. GCC/Clang accept those under -std=c++17 too
# (as an extension, with at most a warning), so this went unnoticed on
# Linux/macOS -- MSVC enforces the standard strictly and hard-errors
# ("use of designated initializers requires at least '/std:c++20'") without
# this, confirmed by a real Windows CI failure.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(NOT DEFINED SUBSURFACE_SRC)
	set(SUBSURFACE_SRC ${CMAKE_SOURCE_DIR}/vendor/subsurface)
endif()
# build-downloader (SUBSURFACE_TARGET_EXECUTABLE=DownloaderExecutable), not
# build-cli: unlike the CLI target, it includes commands/ (subsurface_commands),
# which core/save-git.cpp needs (Command::changesMade()) for saving into a
# git-backed dive log -- see vendor/README.md.
if(NOT DEFINED SUBSURFACE_BUILD)
	set(SUBSURFACE_BUILD ${SUBSURFACE_SRC}/build-downloader)
endif()
if(NOT DEFINED SUBSURFACE_INSTALL_ROOT)
	set(SUBSURFACE_INSTALL_ROOT ${CMAKE_SOURCE_DIR}/vendor/install-root)
endif()

# pkg-config is the normal way to find these on Linux/macOS. It isn't
# reliably wired up for vcpkg on Windows, so fall back to a plain
# find_path()/find_library() pair there (or anywhere pkg-config can't find a
# given module) -- either path fills in the same <PREFIX>_INCLUDE_DIRS/
# <PREFIX>_LIBRARIES variable names used below.
find_package(PkgConfig)

function(find_dependency_lib prefix pkgconfig_name header)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(${prefix} QUIET ${pkgconfig_name})
	endif()
	if(${prefix}_FOUND)
		# pkg_check_modules()'s plain _LIBRARIES only gives short names (e.g.
		# "git2"), relying on the linker's default search paths to resolve
		# them -- true of Linux's system libdirs but not of macOS's Homebrew
		# prefix (/opt/homebrew/lib), confirmed by a real CI failure ("ld:
		# library 'git2' not found"). _LINK_LIBRARIES gives full absolute
		# paths instead, which need no default search path at all.
		set(${prefix}_LIBRARIES ${${prefix}_LINK_LIBRARIES} PARENT_SCOPE)
		return()
	endif()
	find_path(${prefix}_INCLUDE_DIRS NAMES ${header})
	find_library(${prefix}_LIBRARIES NAMES ${ARGN})
	if(NOT ${prefix}_INCLUDE_DIRS OR NOT ${prefix}_LIBRARIES)
		message(FATAL_ERROR "Could not find ${pkgconfig_name} (looked for header ${header} and library ${ARGN})")
	endif()
	set(${prefix}_INCLUDE_DIRS ${${prefix}_INCLUDE_DIRS} PARENT_SCOPE)
	set(${prefix}_LIBRARIES ${${prefix}_LIBRARIES} PARENT_SCOPE)
endfunction()

find_dependency_lib(LIBGIT2 libgit2 git2.h git2)
find_dependency_lib(LIBXML2 libxml-2.0 libxml/xmlversion.h xml2 libxml2)
find_dependency_lib(LIBXSLT libxslt libxslt/xslt.h xslt libxslt)
find_dependency_lib(SQLITE3 sqlite3 sqlite3.h sqlite3)
find_dependency_lib(LIBZIP libzip zip.h zip libzip)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Concurrent Bluetooth Svg)

find_library(SSRF_CORELIB
	NAMES subsurface_corelib
	PATHS ${SUBSURFACE_BUILD}/core
	NO_DEFAULT_PATH
)
if(NOT SSRF_CORELIB)
	message(FATAL_ERROR
		"libsubsurface_corelib.a not found under ${SUBSURFACE_BUILD}/core.\n"
		"Build it first: see vendor/README.md")
endif()
message(STATUS "Using subsurface_corelib: ${SSRF_CORELIB}")

find_library(SSRF_COMMANDS
	NAMES subsurface_commands
	PATHS ${SUBSURFACE_BUILD}/commands
	NO_DEFAULT_PATH
)
if(NOT SSRF_COMMANDS)
	message(FATAL_ERROR
		"libsubsurface_commands.a not found under ${SUBSURFACE_BUILD}/commands.\n"
		"Build it first: see vendor/README.md")
endif()

find_library(LIBDIVECOMPUTER_LIBRARIES
	NAMES divecomputer
	PATHS ${SUBSURFACE_INSTALL_ROOT}/lib ${SUBSURFACE_INSTALL_ROOT}/lib64
	NO_DEFAULT_PATH
)
if(NOT LIBDIVECOMPUTER_LIBRARIES)
	message(FATAL_ERROR
		"libdivecomputer.a not found under ${SUBSURFACE_INSTALL_ROOT}.\n"
		"Build it first: see vendor/README.md")
endif()

# suuntocore: talking to the Suunto cloud API and converting its JSON into
# the shape core/import-suunto-json.cpp parses. No dependency on
# subsurface_corelib -- pure Qt Network/Core -- so it can be unit-tested
# without the (slow) corelib build.
add_library(suuntocore STATIC
	src/suuntocrypto.cpp
	src/suuntoclient.cpp
	src/suuntoconverter.cpp
)
target_link_libraries(suuntocore PUBLIC Qt6::Core Qt6::Network)
target_include_directories(suuntocore PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(suunto2subsurface
	MACOSX_BUNDLE
	src/main.cpp
	src/diveimporter.cpp
	src/mainwindow.cpp
	src/suuntoworker.cpp
)
if(APPLE)
	set_target_properties(suunto2subsurface PROPERTIES
		MACOSX_BUNDLE_BUNDLE_NAME "suunto2subsurface"
		MACOSX_BUNDLE_GUI_IDENTIFIER "org.suunto2subsurface.suunto2subsurface"
	)
endif()

target_include_directories(suunto2subsurface PRIVATE
	${SUBSURFACE_SRC}
	${SUBSURFACE_INSTALL_ROOT}/include
	${LIBGIT2_INCLUDE_DIRS}
	${LIBXML2_INCLUDE_DIRS}
	${LIBXSLT_INCLUDE_DIRS}
)

# Qt6::Bluetooth/Qt6::Svg must come *after* SSRF_CORELIB here: corelib's
# btdiscovery.cpp/imagedownloader.cpp have undefined references to them, and
# a single-pass linker (Ubuntu 22.04's default ld.bfd, hit by the Linux CI
# job) only resolves undefined symbols from libraries listed later on the
# command line -- listing them first (as this used to) links fine on some
# toolchains but fails with "undefined reference to QBluetoothDeviceInfo::..."
# on others.
target_link_libraries(suunto2subsurface PRIVATE
	suuntocore
	Qt6::Core
	Qt6::Widgets
	Qt6::Network
	Qt6::Concurrent
	${SSRF_CORELIB}
	${SSRF_COMMANDS}
	${SSRF_CORELIB}
	Qt6::Bluetooth
	Qt6::Svg
	${LIBDIVECOMPUTER_LIBRARIES}
	${LIBGIT2_LIBRARIES}
	${LIBXML2_LIBRARIES}
	${LIBXSLT_LIBRARIES}
	${SQLITE3_LIBRARIES}
	${LIBZIP_LIBRARIES}
)

# Throwaway dev tool (no GUI) for validating suuntocore against a real
# Suunto account -- see tools/README.md. Not part of the shipped product.
add_executable(suunto_probe
	tools/suunto_probe.cpp
)
target_link_libraries(suunto_probe PRIVATE suuntocore Qt6::Core Qt6::Network)
