#################################################################
#								#
# Copyright (c) 2012-2020 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2017 Stephen L Johnson. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# Needed for CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT
cmake_minimum_required(VERSION 3.7.1)
if(POLICY CMP0087)
  # Policy needed since cmake 3.16
  cmake_policy(SET CMP0087 NEW)
endif()
project(YDB C ASM)

# Max optimization level is -O2
get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
foreach(lang ${languages})
  get_property(flags CACHE CMAKE_${lang}_FLAGS_RELEASE PROPERTY VALUE)
  if("${flags}" MATCHES "-O[3-9]")
    string(REGEX REPLACE "-O[3-9]" "-O2" flags "${flags}")
    set_property(CACHE CMAKE_${lang}_FLAGS_RELEASE PROPERTY VALUE "${flags}")
  endif()
endforeach()

# Defaults
set(version r201)
# If CMAKE_BUILD_TYPE is not defined make it a Release build
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

# Set the install prefix to the build directory and do not allow the default /usr/local, as that's invalid for YottaDB
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE ${CMAKE_CURRENT_BINARY_DIR})
endif()

if("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
  set(defined_types_tail "Debug")
else()
  set(defined_types_tail "Release")
endif()

# If CMAKE_BUILD_TYPE is set to Debug, then CMAKE_C_FLAGS_DEBUG variable contents will be added at the end of
# CMAKE_C_FLAGS during compilation (likewise with CMAKE_ASM_FLAGS_DEBUG). Keep these debug variables defined and ready.
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
if ( CMAKE_COMPILER_IS_GNUCC )
  set(CMAKE_ASM_FLAGS_DEBUG "${CMAKE_ASM_FLAGS_DEBUG} -Wa,--defsym,DEBUG=1")
else()
  set(CMAKE_ASM_FLAGS_DEBUG "${CMAKE_ASM_FLAGS_DEBUG} -Wa,-defsym,DEBUG=1")
endif()

if ( CMAKE_COMPILER_IS_GNUCC )
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wno-unused-result -Wno-parentheses -Wno-unused-value -Wno-unused-variable")
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wno-maybe-uninitialized -Wno-char-subscripts -Wno-unused-but-set-variable")
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wno-unused-function")
endif()

# The below option enables the address sanitizer in the build.
# It is recommended to enable this option only for non-production builds of YottaDB.
option(ENABLE_ASAN "Enable address sanitizer in build?" OFF)

# The below option enables the -ftrivial-auto-var-init=pattern flag in the compiler.
# It is recommended to enable this option only for non-production builds of YottaDB.
option(ENABLE_AUTO_VAR_INIT_PATTERN "Enable -ftrivial-auto-var-init=pattern in build?" OFF)

# The below option enables code profiling
option(ENABLE_PROFILING "Enable code profiling?" OFF)

set(install_permissions_script
  OWNER_READ OWNER_EXECUTE OWNER_WRITE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  )

set_property(DIRECTORY APPEND PROPERTY $<$<CONFIG:Debug>:DEBUG)

# Only IA64 and x86_64 architectures use this option
set(gen_xfer_desc 0)

# Platform specific libs
set(gtm_osarch_libs "")

# Define these ahead of establishing platforms
set(sources_used "")
set(extralibs "")
message("--> OS = ${CMAKE_SYSTEM_NAME} / ARCH = ${CMAKE_SYSTEM_PROCESSOR}")
# Establish platform
# Except for Solaris, CMAKE_COMPILER_IS_GNUCC is true
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  include(sr_linux/platform.cmake)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "AIX")
  include(sr_aix/platform.cmake)
else()
  message(FATAL_ERROR "--> OS = ${CMAKE_SYSTEM_NAME} / ARCH = ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Record what we plan to do (dump various cmake options)
message("*** ENABLE_ASAN:                  ${ENABLE_ASAN}")
message("*** ENABLE_AUTO_VAR_INIT_PATTERN: ${ENABLE_AUTO_VAR_INIT_PATTERN}")
message("*** CMAKE_C_FLAGS:                ${CMAKE_C_FLAGS}")
message("*** CMAKE_ASM_FLAGS:              ${CMAKE_ASM_FLAGS}")
message("*** CMAKE_ASM_FLAGS_DEBUG:        ${CMAKE_ASM_FLAGS_DEBUG}")
message("*** CMAKE_ASM_COMPILE_OBJECT:     ${CMAKE_ASM_COMPILE_OBJECT}")
# Choose where to get bootstrap sources.
set(YDB_DIST "" CACHE PATH "Existing YottaDB Distribution")
if(YDB_DIST)
  # Bootstrap using already-installed yottadb.
  message(STATUS "Using existing ydb_dist=${YDB_DIST} to generate sources.")
  set(gen_bootstrap 1)
  set(gen_bootstrap_dist "${YDB_DIST}")
  set(gen_bootstrap_yottadb "${YDB_DIST}/yottadb")
  set(gen_bootstrap_depend "${gen_bootstrap_yottadb}")
else()
  # Bootstrap using pre-generated bootstrap sources.
  # We generate them again only for comparison.
  message(STATUS "Bootstrapping from pre-generated sources.")
  set(gen_bootstrap 0)
  set(gen_bootstrap_dist "${YDB_BINARY_DIR}")
  set(gen_bootstrap_yottadb "$<TARGET_FILE:yottadb>")
  set(gen_bootstrap_depend yottadb)
endif()

set(YDB_INSTALL_DIR yottadb_${version} CACHE STRING "Install directory")
if(NOT YDB_INSTALL_DIR)
  set(YDB_INSTALL_DIR .)
endif()

# Save the contents of /etc/os-release so ydbinstall.sh can determine if
# a force-install is required
execute_process(
  COMMAND cp /etc/os-release build_os_release
  )

#-----------------------------------------------------------------------------

# Source files are organized into subdirectories named by platform.
# Store in 'gt_src_list' a list of directory names selected and
# ordered based on the target platform.  A given source file name may
# appear in multiple directories but only the instance found earliest
# in the search path will be used.  Later instances of the same source
# file name will be ignored.
list(APPEND gt_src_list
  sr_unix_gnp
  sr_unix_cm
  sr_unix
  sr_port_cm
  sr_port
  )

# Collect the source file names belonging to each directory.  Store
# the full path to a directory in 'source_dir_${d}' and the list of
# source file names in the directory in 'sources_${d}'.
foreach(d ${gt_src_list})
  set(source_dir_${d} ${YDB_SOURCE_DIR}/${d})
  file(GLOB sources_${d} RELATIVE ${source_dir_${d}}
    ${d}/*.c ${d}/*.s ${d}/*.si ${d}/*.txt)
endforeach()
# Prefer generated sources over all other locations.
file(MAKE_DIRECTORY ${YDB_BINARY_DIR}/genused)
file(MAKE_DIRECTORY ${YDB_BINARY_DIR}/gennotused)
set(source_dir_genused ${YDB_BINARY_DIR}/genused)
# Hard-code list since we cannot glob files that do not exist.
set(sources_gen gtm_threadgbl_deftypes.h gtm_threadgbl_deftypes_asm.si)
if(gen_bootstrap)
  set(sources_gen ${sources_gen} cmerrors_ctl.c cmierrors_ctl.c gdeerrors_ctl.c merrors_ctl.c ttt.c)
endif()

# The C preprocessor include search path consists of every source
# directory ordered by 'gt_src_list'.
set(includes ${YDB_BINARY_DIR}/genused)
foreach(d ${gt_src_list})
  file(RELATIVE_PATH d_REL ${CMAKE_CURRENT_SOURCE_DIR} ${source_dir_${d}}/.)
  list(APPEND includes ${d_REL})
endforeach()
include_directories(${includes})
# We generate some uniquely-named headers in the top of the build tree.
include_directories(${YDB_BINARY_DIR})
include_directories(/usr/include/openssl-1.0)

# Function to select the first instance of a given source file name
# found in the ordered 'gt_src_list' search path.
function(select_file src_var name)
  set(found "")
  foreach(d ${gt_src_list})
    set(src "${source_dir_${d}}/${name}")
    if(EXISTS "${src}")
      set(found "${src}")
      break()
    endif()
  endforeach()
  if(NOT found)
    message(FATAL_ERROR "Cannot find \"${name}\" as ${ARGN}")
  endif()
  set("${src_var}" "${found}" PARENT_SCOPE)
endfunction()

# Macro to search directories ordered by 'gt_src_list' for a given
# list of source file names, assign them to a target, and exclude the
# source file names from inclusion in libmumps.
macro(set_source_list target)
  foreach(name ${ARGN})
    set(src "")
    foreach(d ${gt_src_list})
      if(";${sources_${d}};" MATCHES ";(${name}\\.(c|s|si));")
        set(fname ${CMAKE_MATCH_1})
        set(src ${d}/${fname})
        get_filename_component(fnoext ${fname} NAME_WE)
        set("source_used_${fnoext}" 1)
        list(APPEND sources_used ${source_dir_${d}}/${fname})
        break()
      endif()
    endforeach()
    if(src)
      list(APPEND ${target}_SOURCES ${src})
    else()
      message(FATAL_ERROR "No source \"${name}\" available!")
    endif()
  endforeach()
endmacro()

# Macro like set_source_list but that loads the list of source file
# names from a file.
macro(load_source_list target listfile)
  file(STRINGS "${listfile}" names)
  set_source_list(${target} ${names})
endmacro()

#-----------------------------------------------------------------------------

# Assign sources to executables.
set_source_list(gtm_threadgbl_deftypes gtm_threadgbl_deftypes)
set_source_list(dse              dse dlopen_libyottadb)
set_source_list(gtcm_gnp_server  gtcm_gnp_server dlopen_libyottadb)
set_source_list(gtcm_pkdisp      gtcm_pkdisp omi_extstr)
set_source_list(gtcm_play        gtcm_play dlopen_libyottadb)
set_source_list(gtcm_server      gtcm_server dlopen_libyottadb)
set_source_list(gtcm_shmclean    gtcm_shmclean dlopen_libyottadb)
set_source_list(gtmsecshr        gtmsecshr_wrapper)
set_source_list(gtmsecshr_real   gtmsecshr)
set_source_list(libyottadb       gtm_main mupip_main mupip_cmd dse_main dse_cmd lke_main lke_cmd)
set_source_list(libyottadb       gtcm_play_main omi_srvc_xct omi_extstr)
set_source_list(libyottadb       gtcm_server_main gtcm_gnp_server_main gtcm_gnp_clitab gtcm_shmclean_main)
set_source_list(lke              lke dlopen_libyottadb)
set_source_list(yottadb          gtm dlopen_libyottadb)
set_source_list(mupip            mupip dlopen_libyottadb)

#-----------------------------------------------------------------------------
# libmumps gets leftover sources, so compute the remaining list.
set(source_used_dtgbldir 1) # exclude unused source
set_source_list(libmumps ydb_getenv minimal_gbldefs)
foreach(d ${gt_src_list})
  foreach(s ${sources_${d}})
    get_filename_component(snoext ${s} NAME_WE)
    if(NOT source_used_${snoext} AND "${s}" MATCHES "\\.(c|s|si)$")
      list(APPEND libmumps_SOURCES ${d}/${s})
      set(source_used_${snoext} 1)
      list(APPEND sources_used ${source_dir_${d}}/${s})
    endif()
  endforeach()
endforeach()

#-----------------------------------------------------------------------------
# Generate files depending on gtm

# Copy generation routines to a working directory.
foreach(m chk2lev.m chkop.m gendash.m genout.m loadop.m loadvx.m msg.m tttgen.m tttscan.m)
  add_custom_command(
    OUTPUT gennotused/${m}
    DEPENDS ${YDB_SOURCE_DIR}/sr_port/${m}
    COMMAND ${CMAKE_COMMAND} -E copy ${YDB_SOURCE_DIR}/sr_port/${m} ${YDB_BINARY_DIR}/gennotused/${m}
    )
endforeach()

foreach(f ttt.txt opcode_def.h vxi.h)
  select_file(src ${f})
  list(APPEND tttfiles ${src})
endforeach()
set(mumps_ttt_args -run tttgen ${tttfiles})
add_custom_command(
  OUTPUT gennotused/ttt.c
  DEPENDS ${tttfiles}
    gennotused/chk2lev.m gennotused/chkop.m gennotused/gendash.m gennotused/genout.m gennotused/loadop.m
    gennotused/loadvx.m gennotused/tttgen.m gennotused/tttscan.m
    ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake ${gen_bootstrap_depend}
  WORKING_DIRECTORY ${YDB_BINARY_DIR}/gennotused
  COMMAND ${CMAKE_COMMAND}
    -D ydb_dist=${gen_bootstrap_dist}
    -D gtm_tools=${YDB_SOURCE_DIR}/sr_port
    -D ydb_routines=.
    -D ydb_chset=M
    -D mumps=${gen_bootstrap_yottadb}
    -D "args=${mumps_ttt_args}"
    -D output_file=ttt.log
    -P ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake
  VERBATIM
  )
list(APPEND gen_bootstrap_files gennotused/ttt.c)

set(gen_merrors_extra gennotused/merrors_ansi.h)
foreach(msg
    sr_port/cmerrors.msg
    sr_port/gdeerrors.msg
    sr_port/merrors.msg
    sr_port/cmierrors.msg
    )
  get_filename_component(name ${msg} NAME_WE)
  set(mumps_msg_args -run msg ${YDB_SOURCE_DIR}/${msg} unix)
  set(outputs gennotused/${name}_ctl.c ${gen_${name}_extra})
  add_custom_command(
    OUTPUT ${outputs}
    DEPENDS gennotused/msg.m ${YDB_SOURCE_DIR}/${msg}
            ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake ${gen_bootstrap_depend}
    WORKING_DIRECTORY ${YDB_BINARY_DIR}/gennotused
    COMMAND ${CMAKE_COMMAND}
      -D ydb_dist=${gen_bootstrap_dist}
      -D gtm_tools=${YDB_SOURCE_DIR}/sr_port
      -D ydb_routines=.
      -D ydb_chset=M
      -D mumps=${gen_bootstrap_yottadb}
      -D "args=${mumps_msg_args}"
      -P ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
    )
  list(APPEND gen_bootstrap_files ${outputs})
endforeach()

add_custom_target(gen_bootstrap ALL DEPENDS ${gen_bootstrap_files})

#-----------------------------------------------------------------------------

if(gen_xfer_desc)
  list(SORT sources_used)
  set(CMAKE_CONFIGURABLE_FILE_CONTENT "")
  foreach(src ${sources_used})
    set(CMAKE_CONFIGURABLE_FILE_CONTENT "${CMAKE_CONFIGURABLE_FILE_CONTENT}${src}\n")
  endforeach()
  foreach(inc ${includes})
    get_filename_component(absinc ${inc} REALPATH)
    list(APPEND abs_includes ${absinc})
  endforeach()
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in ${YDB_BINARY_DIR}/sources.list)
  add_custom_command(
    OUTPUT xfer_desc.i
    WORKING_DIRECTORY ${YDB_BINARY_DIR}
    DEPENDS ${YDB_SOURCE_DIR}/sr_unix/gen_xfer_desc.cmake
            ${YDB_BINARY_DIR}/sources.list
    COMMAND ${CMAKE_COMMAND}
      -D CMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -D "arch=${arch}"
      -D "includes=${abs_includes}"
      -D CMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
      -P ${YDB_SOURCE_DIR}/sr_unix/gen_xfer_desc.cmake
    VERBATIM
    )
  add_custom_target(gen_xfer_desc DEPENDS xfer_desc.i)
  if(gen_bootstrap)
    add_dependencies(gen_xfer_desc gen_bootstrap)
  endif()
endif()

#-----------------------------------------------------------------------------

add_executable(gtm_threadgbl_deftypes ${gtm_threadgbl_deftypes_SOURCES})
if(gen_xfer_desc)
  add_dependencies(gtm_threadgbl_deftypes gen_xfer_desc)
endif()

# gtm_threadgbl_deftypes
set(gtmthreadgblasmhdr   "genused/gtm_threadgbl_deftypes.h")
add_custom_command(
  OUTPUT ${gtmthreadgblasmhdr}
  DEPENDS gtm_threadgbl_deftypes
  COMMAND gtm_threadgbl_deftypes > ${gtmthreadgblasmhdr}.new
  COMMAND cmp -s ${gtmthreadgblasmhdr} ${gtmthreadgblasmhdr}.new && rm -f ${gtmthreadgblasmhdr}.new || mv ${gtmthreadgblasmhdr}.new ${gtmthreadgblasmhdr}
  )
# gtm_threadgbl_deftypes_asm.si
set(gtmthreadgblasmfile   "genused/gtm_threadgbl_deftypes_asm.si")
select_file(gtmthreadgblasmaccess "gtm_threadgbl_asm_access.txt")
add_custom_command(
  OUTPUT ${gtmthreadgblasmfile}
  DEPENDS ${gtmthreadgblasmhdr}
  COMMAND ${CMAKE_COMMAND}
    -D "gtmthreadgblasmfile=${gtmthreadgblasmfile}"
    -D "gtmthreadgblasmhdr=${gtmthreadgblasmhdr}"
    -D "gtmthreadgblasmaccess=${gtmthreadgblasmaccess}"
    -P ${YDB_SOURCE_DIR}/${srdir}/gen_threadgbl_asm.cmake
  VERBATIM
  )

add_custom_target(gen_gtm_threadgbl_deftypes
  DEPENDS ${YDB_BINARY_DIR}/${gtmthreadgblasmhdr} ${YDB_BINARY_DIR}/${gtmthreadgblasmfile})

foreach(exp ydbexe yottadb)
  set(out ${exp}_symbols.export)
  set(in ${YDB_SOURCE_DIR}/sr_unix/${exp}_symbols.exp)
  add_custom_command(
    OUTPUT ${out}
    DEPENDS ${in} gen_gtm_threadgbl_deftypes
    COMMAND tcsh -f ${YDB_SOURCE_DIR}/sr_linux/genexport.csh ${in} ${out}
    )
endforeach()

add_custom_target(gen_export DEPENDS ydbexe_symbols.export yottadb_symbols.export)

foreach(lib
  mumps
  ${extralibs}
  )
  add_library(lib${lib} STATIC ${lib${lib}_SOURCES})
  set_property(TARGET lib${lib} PROPERTY OUTPUT_NAME ${lib})
  add_dependencies(lib${lib} gen_gtm_threadgbl_deftypes)
endforeach()

target_link_libraries(libmumps ${libsyslibs})
if(gen_xfer_desc)
  add_dependencies(libmumps gen_xfer_desc)
endif()

function(add_executable target)
  _add_executable(${target} ${ARGN})
  if (ENABLE_ASAN)
  	set_target_properties(${target} PROPERTIES LINK_FLAGS "-Wl,-Map=${target}.map -fsanitize=address")
  else()
  	set_target_properties(${target} PROPERTIES LINK_FLAGS "-Wl,-Map=${target}.map")
  endif()
  # Since all utilities now dlopen() libyottadb.so at startup, add a dependency to not build the utilities
  # until libyottadb.so was built. And that in turn relies on threadgbl_deftypes being built so add that too.
  add_dependencies(${target} libyottadb gen_gtm_threadgbl_deftypes)
  # And add system libraries as a target library for all utilities (e.g. dlopen() etc.).
  target_link_libraries(${target} ${libsyslibs})
endfunction()

add_executable(yottadb ${yottadb_SOURCES})

add_executable(dse ${dse_SOURCES})
list(APPEND with_export dse)

add_executable(gtmsecshr ${gtmsecshr_SOURCES})
target_link_libraries(gtmsecshr libmumps) # needed for static linking since this is a setuid executable
                                          # and does not do a dlopen of libyottadb.so

add_executable(gtmsecshr_real ${gtmsecshr_real_SOURCES})
set_target_properties(gtmsecshr_real PROPERTIES
  OUTPUT_NAME gtmsecshr
  RUNTIME_OUTPUT_DIRECTORY ${YDB_BINARY_DIR}/gtmsecshrdir
  )
target_link_libraries(gtmsecshr_real libmumps) # needed for static linking since this is a setuid executable
                                               # and does not do a dlopen of libyottadb.so

add_executable(mupip ${mupip_SOURCES})
list(APPEND with_export mupip)

add_executable(lke ${lke_SOURCES})
list(APPEND with_export lke)

add_executable(gtcm_server ${gtcm_server_SOURCES})
list(APPEND with_export gtcm_server)

add_executable(gtcm_gnp_server ${gtcm_gnp_server_SOURCES})
list(APPEND with_export gtcm_gnp_server)

add_executable(gtcm_play ${gtcm_play_SOURCES})
list(APPEND with_export gtcm_play)

add_executable(gtcm_pkdisp ${gtcm_pkdisp_SOURCES})

add_executable(gtcm_shmclean ${gtcm_shmclean_SOURCES})

add_library(libyottadb MODULE ${libyottadb_SOURCES})
set_property(TARGET libyottadb PROPERTY OUTPUT_NAME yottadb)
target_link_libraries(libyottadb libmumps)
set_target_properties(libyottadb PROPERTIES
  LINK_FLAGS "${libyottadb_link} -Wl,-Map=libyottadb.map"
  LINK_DEPENDS "${libyottadb_dep}"
  )
add_dependencies(libyottadb gen_export)
add_dependencies(yottadb libyottadb)

#-----------------------------------------------------------------------------
## Encryption Plugin Library compilation ##

# Generate the list of OpenSSL options from the target version's ssl.h. This avoids hardcoding these options
# 1. Find the header file
find_file(SSLH openssl/ssl.h CMAKE_INCLUDE_PATH)
if(SSLH MATCHES ".+-NOTFOUND")
	message(FATAL_ERROR "FATAL: ssl.h not found (${SSLH})")
else()
	message("INFO: Using ssl.h ${SSLH}")
endif()
# 2. Find awk
find_program(AWK gawk mawk awk)
if(AWK MATCHES ".+-NOTFOUND")
	message(FATAL_ERROR "FATAL: AWK not be found (${AWK}).")
endif()
# 3. Create the awk scripts to avoid spurious CMake parsing errors and warnings
# NOTE: select between field 2 vs 3 because OpenSSL headers after 1.0.2 do "#<space>define" instead of "#define"
set(CMAKE_CONFIGURABLE_FILE_CONTENT
	"/define[ \\t]+SSL_OP_[A-Z_v0-9]+[ \\t]+/{fld=2;if($2 ~ /define/)fld=3;print \"DEFINE_SSL_OP(\" $(fld) \"),\"}")
configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in ${YDB_BINARY_DIR}/gen_tls_options.awk)
set(CMAKE_CONFIGURABLE_FILE_CONTENT
	"/define[ \\t]+SSL_VERIFY_[A-Z_v0-9]+[ \\t]+/{fld=2;if($2 ~ /define/)fld=3;print \"DEFINE_SSL_OP(\" $(fld) \"),\"}")
configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in ${YDB_BINARY_DIR}/gen_tls_verify_options.awk)
# 4. Generate the files
add_custom_command(
  OUTPUT ${YDB_BINARY_DIR}/gen_tls_verify_options.h
  DEPENDS gen_gtm_threadgbl_deftypes
  COMMENT "Generating SSL/TLS verification options header file"
  COMMAND ${AWK} -f ${YDB_BINARY_DIR}/gen_tls_verify_options.awk ${SSLH} > ${YDB_BINARY_DIR}/gen_tls_verify_options.h
  VERBATIM
  )
add_custom_command(
  OUTPUT ${YDB_BINARY_DIR}/gen_tls_options.h
  DEPENDS gen_gtm_threadgbl_deftypes
  COMMENT "Generating SSL/TLS options header file"
  COMMAND ${AWK} -f ${YDB_BINARY_DIR}/gen_tls_options.awk ${SSLH} > ${YDB_BINARY_DIR}/gen_tls_options.h
  VERBATIM
  )
# 5. Setup the dependency for downstream consumers
add_custom_target(gen_tls_options
  DEPENDS ${YDB_BINARY_DIR}/gen_tls_options.h ${YDB_BINARY_DIR}/gen_tls_verify_options.h)

#-----------------------------------------------------------------------------
install(TARGETS
  yottadb
  dse
  lke
  gtcm_server
  gtcm_gnp_server
  gtcm_pkdisp
  gtcm_play
  gtcm_shmclean
  gtmsecshr
  mupip
  libyottadb
  DESTINATION ${YDB_INSTALL_DIR}
  )
install(TARGETS gtmsecshr_real DESTINATION ${YDB_INSTALL_DIR}/gtmsecshrdir)

# <r>.m -> <R>.m
#file(GLOB m_files_sr_port RELATIVE ${YDB_SOURCE_DIR}/sr_port ${YDB_SOURCE_DIR}/sr_port/*.m)
set(m_files_sr_port
  gde.m
  gdeadd.m
  gdechang.m
  gdedelet.m
  gdeexit.m
  gdehelp.m
  gdeinit.m
  gdelocks.m
  gdelog.m
  gdemap.m
  gdemsgin.m
  gdeparse.m
  gdequit.m
  gderenam.m
  gdescan.m
  gdesetgd.m
  gdeshow.m
  gdespawn.m
  gdetempl.m
  )
file(GLOB m_files_sr_unix RELATIVE ${YDB_SOURCE_DIR}/sr_unix ${YDB_SOURCE_DIR}/sr_unix/*.m)
file(GLOB m_files_sr_x86_64 RELATIVE ${YDB_SOURCE_DIR}/sr_x86_64 ${YDB_SOURCE_DIR}/sr_x86_64/*.m)
file(GLOB m_files_sr_armv7l RELATIVE ${YDB_SOURCE_DIR}/sr_armv7l ${YDB_SOURCE_DIR}/sr_armv7l/*.m)
file(GLOB m_files_sr_aarch64 RELATIVE ${YDB_SOURCE_DIR}/sr_aarch64 ${YDB_SOURCE_DIR}/sr_aarch64/*.m)
file(GLOB m_files_sr_i386 RELATIVE ${YDB_SOURCE_DIR}/sr_i386 ${YDB_SOURCE_DIR}/sr_i386/*.m)
file(GLOB mpt_files_sr_port RELATIVE ${YDB_SOURCE_DIR}/sr_port ${YDB_SOURCE_DIR}/sr_port/*.mpt)
file(GLOB mpt_files_sr_unix RELATIVE ${YDB_SOURCE_DIR}/sr_unix ${YDB_SOURCE_DIR}/sr_unix/*.mpt)

set(ydb_chset_dir_ "")
set(ydb_chset_dir_UTF-8 "/utf8")

foreach(d ${gt_src_list})
  foreach(m ${m_files_${d}})
    get_filename_component(m_name "${m}" NAME_WE)
    string(TOUPPER "${m_name}" m_upper)
    if("${m}" MATCHES "^gde")
        foreach(ydb_chset "M" "UTF-8")
        set(m_out "${YDB_BINARY_DIR}${ydb_chset_dir_${ydb_chset}}/${m_upper}.m")
        add_custom_command(
          OUTPUT "${m_out}"
          DEPENDS ${YDB_SOURCE_DIR}/${d}/${m}
          COMMAND ${CMAKE_COMMAND} -E copy ${YDB_SOURCE_DIR}/${d}/${m} "${m_out}"
          )
        list(APPEND files_to_place "${m_out}")
        list(APPEND m_routines "${m_out}")
        list(APPEND ydb_chset_routines_${ydb_chset} "${m_out}")
      endforeach()
    else()
      if(";${m_name};" MATCHES ";GTMDefinedTypesInit[a-zA-Z]*;")
	# If Release build, copy/install GTMDefinedTypesInitRelease.m as GTMDefinedTypesInit.m and ignore GTMDefinedTypesDebug.m
	# If Debug   build, copy/install GTMDefinedTypesInitDebug.m   as GTMDefinedTypesInit.m and ignore GTMDefinedTypesRelease.m
        if(m_name STREQUAL GTMDefinedTypesInit${defined_types_tail})
          set(m_out "${YDB_BINARY_DIR}/GTMDefinedTypesInit.m")
	else()
          set(m_out "")
	endif()
      else()
        set(m_out "${YDB_BINARY_DIR}/${m_upper}.m")
      endif()
      if (NOT m_out STREQUAL "")
        add_custom_command(
          OUTPUT "${m_out}"
          DEPENDS ${YDB_SOURCE_DIR}/${d}/${m}
          COMMAND ${CMAKE_COMMAND} -E copy ${YDB_SOURCE_DIR}/${d}/${m} "${m_out}"
          )
        install(FILES "${m_out}" DESTINATION ${YDB_INSTALL_DIR})
        list(APPEND files_to_place "${m_out}")
        list(APPEND m_routines "${m_out}")
      endif()
    endif()
  endforeach()
  foreach(m ${mpt_files_${d}})
    string(TOUPPER "_${m}" m_upper)
    string(REGEX REPLACE "MPT$" "m" m_out "${YDB_BINARY_DIR}/${m_upper}")
    add_custom_command(
      OUTPUT "${m_out}"
      DEPENDS ${YDB_SOURCE_DIR}/${d}/${m}
      COMMAND ${CMAKE_COMMAND} -E copy ${YDB_SOURCE_DIR}/${d}/${m} "${m_out}"
      )
    install(FILES "${m_out}" DESTINATION ${YDB_INSTALL_DIR})
    list(APPEND files_to_place "${m_out}")
    list(APPEND m_routines "${m_out}")
  endforeach()
endforeach()

# Below is the list of header files that are placed in $ydb_dist for C applications that use YottaDB
# The purpose of some of the files is listed below.
#	ydb_tls_interface.h  : is needed for users of the TLS plugin
#	ydbcrypt_interface.h : is needed for users of the Encryption plugin
set(files)
foreach(f
  gtm_common_defs.h
  gtm_descript.h
  gtm_limits.h
  gtm_sizeof.h
  gtm_stdio.h
  gtm_stdlib.h
  gtm_string.h
  gtm_strings.h
  gtmxc_types.h
  libyottadb.h
  gparam_list.h
  libydberrors.h
  libydberrors2.h
  mmrhash.h
  ydb_tls_interface.h
  ydbcrypt_interface.h
  gtmgblstat.xc
  arch.gtc
  gtcm_run.gtc
  gtcm_slist.gtc
  gdedefaults.gtc
  ydb_env_set.gtc
  ydb_env_unset.gtc
  dse.hlp
  gde.hlp
  lke.hlp
  mumps.hlp
  mupip.hlp
  custom_errors_sample.txt
  )
  select_file(src ${f})
  get_filename_component(srcfname "${src}" NAME)
  add_custom_command(
    OUTPUT "${YDB_BINARY_DIR}/${srcfname}"
    DEPENDS "${src}"
    COMMAND ${CMAKE_COMMAND} -E copy "${src}" "${YDB_BINARY_DIR}/${srcfname}"
    )
  list(APPEND files_to_place "${YDB_BINARY_DIR}/${srcfname}")
  list(APPEND files ${src})
endforeach()
install(FILES ${files} DESTINATION ${YDB_INSTALL_DIR})

set(scripts)
foreach(f
  ydb.gtc
  )
  select_file(src ${f})
  list(APPEND scripts ${src})
endforeach()
install(FILES ${scripts}
  DESTINATION ${YDB_INSTALL_DIR}
  PERMISSIONS ${install_permissions_script}
  )

if (NOT DEFINED ENV{ydb_icu_version})
  find_program(PKGCONFIG NAMES pkg-config)
  if(PKGCONFIG)
    execute_process(
      COMMAND ${PKGCONFIG} --modversion icu-io
      OUTPUT_VARIABLE icu_version
      RESULT_VARIABLE icu_failed
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    if(icu_failed)
      message(FATAL_ERROR "Command\n ${PKGCONFIG} --modversion icu-io\nfailed (${icu_failed}).")
    elseif("x${icu_version}" MATCHES "^x([0-9]+\\.[0-9]+)")
      set(ydb_icu_version "${CMAKE_MATCH_1}")
    else()
      message(FATAL_ERROR "Command\n ${PKGCONFIG} --modversion icu-io\nproduced unrecognized output:\n ${icu_version}")
    endif()
  else()
    message(FATAL_ERROR "Unable to find 'pkg-config'.  Set PKGCONFIG in CMake cache.")
  endif()
else()
  set(ydb_icu_version "$ENV{ydb_icu_version}")
endif()

find_program(LOCALECFG NAMES locale)
if(LOCALECFG)
  execute_process(
  COMMAND ${LOCALECFG} -a
    OUTPUT_VARIABLE locale_list
    RESULT_VARIABLE locale_failed
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(locale_failed)
    message(FATAL_ERROR "Command\n ${LOCALECFG} -a\nfailed (${locale_failed}).")
  endif()
  STRING(REGEX REPLACE "\n" ";" locale_list "${locale_list}")
  foreach(lc ${locale_list})
    string(TOLOWER "${lc}" lc_lower)
    if("x${lc_lower}" MATCHES "^x[a-zA-Z_]+\\.?utf-?8")
      set(LC_ALL ${lc})
      message("-- Setting locale to ${LC_ALL}")
      break()
    endif()
  endforeach(lc)
  if("${LC_ALL}" STREQUAL "")
    message("Locale undefined. Expect to see NONUTF8LOCALE during MUMPS routine compilation: ${locale_list}\n")
  endif()
else()
  message(FATAL_ERROR "Unable to find 'locale'.  Set LOCALECFG in CMake cache.")
endif()

foreach(ydb_chset "M" "UTF-8")
  foreach(in ${ydb_chset_routines_${ydb_chset}})
    # ${in} is a ".m" file name. For example, "<path>/GDEGET.m". Derive the corresponding ".o" for it i.e. "<path>/GDEGET.o"
    # in the example case.
    get_filename_component(baseName "${in}" NAME_WE)
    get_filename_component(out_dir "${in}" PATH)
    set(out ${out_dir}/${baseName}.o)
    add_custom_command(
      OUTPUT ${out}
      DEPENDS ${in} ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake yottadb
      COMMAND ${CMAKE_COMMAND}
        -D ydb_dist=${YDB_BINARY_DIR}
        -D ydb_routines=.
        -D ydb_chset=${ydb_chset}
        -D ydb_icu_version=${ydb_icu_version}
        -D LC_ALL=${LC_ALL}
        -D mumps=$<TARGET_FILE:yottadb>
        -D "args=${in}"
        -P ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake
      WORKING_DIRECTORY ${out_dir}
      VERBATIM
      )
    list(APPEND files_to_place ${out})
    list(APPEND mo_files ${out})
    install(FILES "${out}" DESTINATION ${YDB_INSTALL_DIR}${ydb_chset_dir_${ydb_chset}})
  endforeach()
endforeach()

#-----------------------------------------------------------------------------
set(gtm_hlp mumps.hlp)
set(gde_hlp gde.hlp)
set(mupip_hlp mupip.hlp)
set(dse_hlp dse.hlp)
set(lke_hlp lke.hlp)
foreach(help gtm gde mupip dse lke)
  set(CMAKE_CONFIGURABLE_FILE_CONTENT
    "Change -segment DEFAULT -block=2048 -file=\$ydb_dist/${help}help.dat
Change -region DEFAULT -record=1020 -key=255 -qdbrundown -nostats
exit")
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in
                 ${YDB_BINARY_DIR}/${help}help.in1)
  set(CMAKE_CONFIGURABLE_FILE_CONTENT
    "Do ^GTMHLPLD
${YDB_SOURCE_DIR}/sr_port/${${help}_hlp}
Halt")
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in
                 ${YDB_BINARY_DIR}/${help}help.in2)
  set(env
    -D ydb_dist=${YDB_BINARY_DIR}
    -D ydb_routines=.
    -D ydb_gbldir=${YDB_BINARY_DIR}/${help}help
    -D ydb_chset=M
    )
  add_custom_command(
    OUTPUT ${YDB_BINARY_DIR}/${help}help.gld
    DEPENDS ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake $<TARGET_FILE:yottadb>
            ${YDB_BINARY_DIR}/${help}help.in1
            ${mo_files}
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:yottadb> -D "args=-run;GDE"
      -D input_file=${YDB_BINARY_DIR}/${help}help.in1
      -P ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
    )
  add_custom_command(
    OUTPUT ${YDB_BINARY_DIR}/${help}help.dat
    DEPENDS ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake $<TARGET_FILE:yottadb> $<TARGET_FILE:mupip>
            ${YDB_BINARY_DIR}/${help}help.in2
            ${YDB_BINARY_DIR}/${help}help.gld
    WORKING_DIRECTORY ${YDB_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove -f ${help}help.dat
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:mupip> -D "args=create"
      -D gtmgbldir=${help}help
      -P ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:yottadb> -D "args=-direct"
      -D input_file=${YDB_BINARY_DIR}/${help}help.in2
      -P ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
    )
  list(APPEND files_to_place ${help}help.dat)
  install(FILES ${YDB_BINARY_DIR}/${help}help.dat DESTINATION ${YDB_INSTALL_DIR})
  install(FILES ${YDB_BINARY_DIR}/${help}help.gld DESTINATION ${YDB_INSTALL_DIR})

  # Set help files to read-only after installing.
  # By default variables are not inherited. Pass the ones that are needed for installing.
  install(CODE "set(YDB_BUILD_DIR \"${YDB_BUILD_DIR}\")")
  install(CODE "set(YDB_INSTALL_DIR \"${CMAKE_INSTALL_PREFIX}/${YDB_INSTALL_DIR}\")")
  install(CODE "set(help \"${help}\")")
  install(SCRIPT ${YDB_SOURCE_DIR}/cmake/set-help-read-only.cmake)
endforeach()
add_custom_command(
  OUTPUT GTMDEFINEDTYPESTODB.o
  DEPENDS GTMDefinedTypesInit.m ${YDB_BINARY_DIR}/gtmhelp.dat
  COMMAND ${CMAKE_COMMAND}
    -D ydb_dist=${YDB_BINARY_DIR}
    -D ydb_routines=.
    -D ydb_gbldir=${YDB_BINARY_DIR}/gtmhelp
    -D ydb_chset=M
    -D mumps=$<TARGET_FILE:yottadb>
    -D "args=-run;GTMDEFINEDTYPESTODB"
    -P ${YDB_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
  )
list(APPEND files_to_place GTMDEFINEDTYPESTODB.o)

configure_file(sr_unix/gpgagent.tab.in plugin/gpgagent.tab @ONLY)
set(YDB_TOP "$ENV{PWD}/${YDB_INSTALL_DIR}")
configure_file(sr_unix/gpgagent.tab.in CMakeFiles/plugin/gpgagent.tab @ONLY)
install(FILES ${YDB_BINARY_DIR}/CMakeFiles/plugin/gpgagent.tab DESTINATION ${YDB_INSTALL_DIR}/plugin)

install(FILES sr_unix/configure.gtc DESTINATION ${YDB_INSTALL_DIR}
  PERMISSIONS ${install_permissions_script}
  RENAME configure
  )

install(FILES sr_unix/ydbinstall.sh DESTINATION ${YDB_INSTALL_DIR}
  PERMISSIONS ${install_permissions_script}
  RENAME ydbinstall
  )

install(FILES sr_unix/lowerc_cp.sh DESTINATION ${YDB_INSTALL_DIR}
  RENAME lowerc_cp
  )

foreach(d COPYING README.md)
  if (EXISTS ${YDB_SOURCE_DIR}/${d})
    install(FILES ${d} DESTINATION ${YDB_INSTALL_DIR})
  endif()
endforeach()

#-----------------------------------------------------------------------------
# Strip debug symbols from files
if("${CMAKE_BUILD_TYPE}" MATCHES "RelWithDebInfo")
  list(APPEND executable_target_list
    dse
    gtcm_gnp_server
    gtcm_pkdisp
    gtcm_play
    gtcm_server
    gtcm_shmclean
    gtmsecshr
    gtmsecshr_real
    libyottadb
    lke
    yottadb
    mupip
    )

  foreach(e ${executable_target_list})
    add_custom_command(TARGET ${e} POST_BUILD
      COMMAND objcopy --only-keep-debug $<TARGET_FILE:${e}> ${e}.dbg
      COMMAND objcopy --strip-debug $<TARGET_FILE:${e}>
      COMMAND objcopy --add-gnu-debuglink=${e}.dbg $<TARGET_FILE:${e}>
      WORKING_DIRECTORY "${YDB_BINARY_DIR}"
      )
    install(FILES "${YDB_BINARY_DIR}/${e}.dbg" DESTINATION ${YDB_INSTALL_DIR})
  endforeach()
endif()

add_custom_target(place_files DEPENDS ${files_to_place})
add_custom_target(make_read_only ALL DEPENDS ${targets_to_run} place_files)

#-----------------------------------------------------------------------------
# Add git hash to $ZYRELEASE
# This preprocesses `sr_linux/release_commit.h.in` and outputs to `release_commit.h` in the build directory.
# The output file will be automatically rebuilt whenever the git commit changes,
# even if `cmake` is not rerun before running `make`.
set(PRE_CONFIGURE_FILE "${PROJECT_SOURCE_DIR}/sr_linux/release_commit.h.in")
set(POST_CONFIGURE_FILE "${PROJECT_BINARY_DIR}/release_commit.h")
include(${PROJECT_SOURCE_DIR}/cmake/git-watcher.cmake)
set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${PROJECT_BINARY_DIR}/git-state")
add_dependencies(libmumps check_git_repository)
add_dependencies(libyottadb check_git_repository)
