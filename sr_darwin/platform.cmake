#################################################################
#								#
#	Copyright 2013 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

set(arch "x86_64")
set(bits 64)
set(srdir "sr_darwin")

# Platform directories
list(APPEND gt_src_list sr_darwin sr_x86_64 sr_x86_regs)
set(gen_xfer_desc 1)

# Assembler
#enable_language(ASM_NASM)
set(CMAKE_INCLUDE_FLAG_ASM "-Wa,-I") # gcc -I does not make it to "as"
#set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -flat_namespace -undefined suppress")
#list(APPEND CMAKE_ASM_COMPILE_OBJECT "objcopy --prefix-symbols=_ <OBJECT>")

# Compiler
set(CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} -fsigned-char -fPIC -Wmissing-prototypes -fno-omit-frame-pointer -mmacosx-version-min=10.11")

set(CMAKE_C_FLAGS_RELEASE  "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing")

# https://cmake.org/pipermail/cmake/2009-June/029929.html
SET(CMAKE_C_CREATE_STATIC_LIBRARY
  "<CMAKE_AR> cr <TARGET> <LINK_FLAGS> <OBJECTS> "
	"<CMAKE_RANLIB> -c <TARGET> "
  )

add_definitions(
  #-DNOLIBGTMSHR #gt_cc_option_DBTABLD=-DNOLIBGTMSHR
  -D_GNU_SOURCE
  -D_FILE_OFFSET_BITS=64
  -D_XOPEN_SOURCE=600L
  -D_LARGEFILE64_SOURCE
  -D_DARWIN_C_SOURCE
  )

# Locate external packages
find_package(Curses REQUIRED) # FindCurses.cmake
include_directories(${CURSES_INCLUDE_PATH})

find_package(Zlib REQUIRED)   # FindZLIB.cmake
include_directories(${ZLIB_INCLUDE_DIRS})

find_library(LIBELF_LIBRARY_PATH NAMES elf)
if(LIBELF_LIBRARY_PATH)
  message("-- Found libelf: ${LIBELF_LIBRARY_PATH}")
endif()
get_filename_component(_libelfLibDir "${LIBELF_LIBRARY_PATH}" PATH)
get_filename_component(_libelfParentDir "${_libelfLibDir}" PATH)
find_path(LIBELF_INCLUDE_PATH NAMES libelf.h libelf/libelf.h HINTS "${_libelfParentDir}/include" )
include_directories(${LIBELF_INCLUDE_PATH})

set(GTM_SET_ICU_VERSION 0 CACHE BOOL "Unless you want ICU from MacPorts/other avoid setting gtm_icu_version to get Apple's undocumented ICU library")

# Set some MOSX specific stuff 
set(CMAKE_MACOSX_RPATH 1)
# Found these on the web: CMAKE_OSX_ARCHITECTURES, CMAKE_OSX_DEPLOYMENT_TARGET, CMAKE_OSX_SYSROOT

# Linker
set(gtm_link  "-Wl,-U,gtm_filename_to_id -Wl,-U,gtm_zstatus -Wl,-v -Wl,-exported_symbols_list \"${GTM_BINARY_DIR}/gtmexe_symbols.export\"")
set(libgtmshr_link "-Wl,-U,gtm_ci -Wl,-U,gtm_filename_to_id -Wl,-exported_symbols_list \"${GTM_BINARY_DIR}/gtmshr_symbols.export\"")
set(libgtmshr_dep  "${GTM_BINARY_DIR}/gtmexe_symbols.export")

set(libmumpslibs "-lm -ldl -lc -lpthread ${LIBELF_LIBRARY_PATH} ${CURSES_NCURSES_LIBRARY}")

