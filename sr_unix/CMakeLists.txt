#################################################################
#								#
#	Copyright 2012, 2013 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# CMake 2.8.4 introduced LINK_DEPENDS target property
# CMake 2.8.4 introduced generator expressions in custom commands
# CMake 2.8.5 introduced use of C compiler as the assembler
cmake_minimum_required(VERSION 2.8.5)
project(GTM C ASM)

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
set(version GTM_RELEASE_VERSION)
if("${version}" STREQUAL "")
        set(version V9.9-0)
endif()

# If CMAKE_BUILD_TYPE is not defined make it a Release build
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# If it's a debug build make sure GT.M uses all of its debug options
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

set(install_permissions_script
  OWNER_READ OWNER_EXECUTE OWNER_WRITE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  )

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG DEBUG)

# Only IA64 and x86_64 architectures use this option
set(gen_xfer_desc 0)

# Platform specific libs
set(gtm_osarch_libs "")

# Define these ahead of establishing platforms
set(gt_src_list)
set(sources_used "")
set(extralibs "")
set(is_encryption_supported 1)
set(libmumpsrestoreregex "")
message("--> OS = ${CMAKE_SYSTEM_NAME} / ARCH = ${CMAKE_SYSTEM_PROCESSOR}")
# Establish platform
# Except for Solaris, CMAKE_COMPILER_IS_GNUCC is true
if("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
  include(sr_linux/platform.cmake)
elseif("${CMAKE_SYSTEM_NAME}" MATCHES "HP-UX")
  include(sr_hpux/platform.cmake)
elseif("${CMAKE_SYSTEM_NAME}" MATCHES "AIX")
  include(sr_aix/platform.cmake)
elseif("${CMAKE_SYSTEM_NAME}" MATCHES "SunOS")
  include(sr_sun/platform.cmake)
else()
  message(FATAL_ERROR "--> OS = ${CMAKE_SYSTEM_NAME} / ARCH = ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Choose where to get bootstrap sources.
set(GTM_DIST "" CACHE PATH "Existing GT.M Distribution")
if(GTM_DIST)
  # Bootstrap using already-installed mumps.
  message(STATUS "Using existing gtm_dist=${GTM_DIST} to generate sources.")
  set(gen_bootstrap 1)
  set(gen_bootstrap_dist "${GTM_DIST}")
  set(gen_bootstrap_mumps "${GTM_DIST}/mumps")
  set(gen_bootstrap_depend "${gen_bootstrap_mumps}")
else()
  # Bootstrap using pre-generated bootstrap sources.
  # We generate them again only for comparison.
  set(gen_bootstrap 0)
  set(gen_bootstrap_dist "${GTM_BINARY_DIR}")
  set(gen_bootstrap_mumps "$<TARGET_FILE:mumps>")
  set(gen_bootstrap_depend mumps)
endif()

set(GTM_INSTALL_DIR lib/fis-gtm/${version}_${arch} CACHE STRING "Install directory (under CMAKE_PREFIX_PATH if relative)")
if(NOT GTM_INSTALL_DIR)
  set(GTM_INSTALL_DIR .)
endif()

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
  set(source_dir_${d} ${GTM_SOURCE_DIR}/${d})
  file(GLOB sources_${d} RELATIVE ${source_dir_${d}}
	  ${d}/*.c ${d}/*.s ${d}/*.si)
endforeach()
if(gen_bootstrap)
  # Prefer generated sources over all other locations.
  set(gt_src_list gen ${gt_src_list})
  set(source_dir_gen ${GTM_BINARY_DIR}/gen)
  # Hard-code list since we cannot glob files that do not exist.
  set(sources_gen cmerrors_ctl.c cmierrors_ctl.c gdeerrors_ctl.c merrors_ctl.c ttt.c)
endif()

# The C preprocessor include search path consists of every source
# directory ordered by 'gt_src_list'.
set(includes)
foreach(d ${gt_src_list})
  list(APPEND includes ${source_dir_${d}})
endforeach()
include_directories(${includes})
# We generate some uniquely-named headers in the top of the build tree.
include_directories(${GTM_BINARY_DIR})

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
	set("source_used_${fname}" 1)
	list(APPEND sources_used ${source_dir_${d}}/${fname})
	if(NOT "${libmumpsrestoreregex}" STREQUAL "")
	  if(";${name};" MATCHES ";(${libmumpsrestoreregex});")
	   set("source_used_${fname}" 0)
          endif()
        endif()
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

# Assign sources to libraries.
load_source_list(libcmisockettcp sr_unix_gnp/libcmisockettcp.list)
load_source_list(libdbcertify    sr_unix/libdbcertify.list)
load_source_list(libdse          sr_unix/libdse.list)
load_source_list(libgnpclient    sr_unix_gnp/libgnpclient.list)
load_source_list(libgnpserver    sr_unix_gnp/libgnpserver.list)
load_source_list(libgtcm         sr_unix_cm/libgtcm.list)
load_source_list(liblke          sr_unix/liblke.list)
load_source_list(libmupip        sr_unix/libmupip.list)
load_source_list(libstub         sr_unix/libstub.list)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "SunOS")
  load_source_list(libgtmrpc	sr_sun/libgtmrpc.list)
endif()

# Assign sources to executables.
set_source_list(gtm_threadgbl_deftypes gtm_threadgbl_deftypes)
set_source_list(dbcertify        dbcertify dbcertify_cmd)
set_source_list(dse              dse dse_cmd)
set_source_list(ftok             ftok)
set_source_list(geteuid          geteuid)
set_source_list(gtcm_gnp_server  gtcm_gnp_server)
set_source_list(gtcm_pkdisp      gtcm_pkdisp)
set_source_list(gtcm_play        gtcm_play omi_srvc_xct)
set_source_list(gtcm_server      gtcm_main omi_srvc_xct)
set_source_list(gtcm_shmclean    gtcm_shmclean)
set_source_list(gtmsecshr        gtmsecshr_wrapper)
set_source_list(gtmsecshr_real   gtmsecshr)
set_source_list(libgtmcrypt      gtmcrypt_ref gtmcrypt_pk_ref gtmcrypt_dbk_ref)
set_source_list(libgtmshr        gtm_main)
set_source_list(lke              lke lke_cmd)
set_source_list(maskpass         maskpass)
set_source_list(mumps            gtm)
set_source_list(mupip            mupip mupip_cmd)
set_source_list(semstat2         semstat2)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "SunOS")
  set_source_list(gtm_svc  	gtm_svc gtm_rpc_init gtm_dal_svc)
endif()

#-----------------------------------------------------------------------------
# libmumps gets leftover sources, so compute the remaining list.
set(source_used_dtgbldir.c 1) # exclude unused source
set(libmumps_SOURCES "")
foreach(d ${gt_src_list})
  foreach(s ${sources_${d}})
    if(NOT source_used_${s})
      list(APPEND libmumps_SOURCES ${d}/${s})
      set(source_used_${s} 1)
      list(APPEND sources_used ${source_dir_${d}}/${s})
    endif()
  endforeach()
endforeach()

#-----------------------------------------------------------------------------
# Generate files depending on gtm

# Copy generation routines to a working directory.
foreach(m chk2lev.m chkop.m gendash.m genout.m loadop.m loadvx.m msg.m tttgen.m tttscan.m)
  add_custom_command(
    OUTPUT gen/${m}
    DEPENDS ${GTM_SOURCE_DIR}/sr_port/${m}
    COMMAND ${CMAKE_COMMAND} -E copy ${GTM_SOURCE_DIR}/sr_port/${m} ${GTM_BINARY_DIR}/gen/${m}
    )
endforeach()

foreach(f ttt.txt opcode_def.h vxi.h)
  select_file(src ${f})
  list(APPEND tttfiles ${src})
endforeach()
set(mumps_ttt_args -run tttgen ${tttfiles})
add_custom_command(
  OUTPUT gen/ttt.c
  DEPENDS ${tttfiles}
    gen/chk2lev.m gen/chkop.m gen/gendash.m gen/genout.m gen/loadop.m
    gen/loadvx.m gen/tttgen.m gen/tttscan.m
    ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake ${gen_bootstrap_depend}
  WORKING_DIRECTORY ${GTM_BINARY_DIR}/gen
  COMMAND ${CMAKE_COMMAND}
    -D gtm_dist=${gen_bootstrap_dist}
    -D gtmroutines=.
    -D mumps=${gen_bootstrap_mumps}
    -D "args=${mumps_ttt_args}"
    -D output_file=ttt.log
    -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
  VERBATIM
  )
list(APPEND gen_bootstrap_files gen/ttt.c)

set(gen_merrors_extra gen/merrors_ansi.h)
foreach(msg
    sr_port/cmerrors.msg
    sr_port/gdeerrors.msg
    sr_port/merrors.msg
    sr_unix_gnp/cmierrors.msg
    )
  get_filename_component(name ${msg} NAME_WE)
  set(mumps_msg_args -run msg ${GTM_SOURCE_DIR}/${msg} unix)
  set(outputs gen/${name}_ctl.c ${gen_${name}_extra})
  add_custom_command(
    OUTPUT ${outputs}
    DEPENDS gen/msg.m ${GTM_SOURCE_DIR}/${msg}
            ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake ${gen_bootstrap_depend}
    WORKING_DIRECTORY ${GTM_BINARY_DIR}/gen
    COMMAND ${CMAKE_COMMAND}
      -D gtm_dist=${gen_bootstrap_dist}
      -D gtmroutines=.
      -D mumps=${gen_bootstrap_mumps}
      -D "args=${mumps_msg_args}"
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
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
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in ${GTM_BINARY_DIR}/sources.list)
  add_custom_command(
    OUTPUT xfer_desc.i
    WORKING_DIRECTORY ${GTM_BINARY_DIR}
    DEPENDS ${GTM_SOURCE_DIR}/sr_unix/gen_xfer_desc.cmake
            ${GTM_BINARY_DIR}/sources.list
    COMMAND ${CMAKE_COMMAND}
      -D CMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -D "arch=${arch}"
      -D "includes=${includes}"
      -P ${GTM_SOURCE_DIR}/sr_unix/gen_xfer_desc.cmake
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
elseif(gen_bootstrap)
  add_dependencies(gtm_threadgbl_deftypes gen_bootstrap)
endif()

add_custom_command(
  OUTPUT gtm_threadgbl_deftypes.h
  DEPENDS gtm_threadgbl_deftypes
  COMMAND gtm_threadgbl_deftypes > gtm_threadgbl_deftypes.h.tmp
  COMMAND ${CMAKE_COMMAND} -E rename gtm_threadgbl_deftypes.h.tmp gtm_threadgbl_deftypes.h
  )
add_custom_target(gen_gtm_threadgbl_deftypes DEPENDS gtm_threadgbl_deftypes.h)

foreach(exp exe shr)
  set(out gtm${exp}_symbols.export)
  set(in ${GTM_SOURCE_DIR}/sr_unix/gtm${exp}_symbols.exp)
  add_custom_command(
    OUTPUT ${out}
    DEPENDS ${in}
    COMMAND tcsh -f ${GTM_SOURCE_DIR}/sr_linux/genexport.csh ${in} ${out}
    )
endforeach()

add_custom_target(gen_export DEPENDS gtmexe_symbols.export gtmshr_symbols.export)

foreach(lib
  gtcm
  cmisockettcp
  gnpclient
  gnpserver
  dbcertify
  dse
  lke
  mupip
  stub
  mumps
  ${extralibs}
  )
  add_library(lib${lib} STATIC ${lib${lib}_SOURCES})
  set_property(TARGET lib${lib} PROPERTY OUTPUT_NAME ${lib})
  add_dependencies(lib${lib} gen_gtm_threadgbl_deftypes)
endforeach()

# TODO: find_package or find_library for system libs?
include_directories ("/usr/local/include")
target_link_libraries(libmumps		${libmumpslibs})

add_executable(mumps			${mumps_SOURCES})
target_link_libraries(mumps		libmumps)

add_executable(dse			${dse_SOURCES})
target_link_libraries(dse		libdse libmumps libstub)
list(APPEND with_export dse)

add_executable(dbcertify		${dbcertify_SOURCES})
target_link_libraries(dbcertify		libdbcertify libmupip libmumps libstub)

add_executable(geteuid			${geteuid_SOURCES})

add_executable(gtmsecshr		${gtmsecshr_SOURCES})
add_dependencies(gtmsecshr		gen_gtm_threadgbl_deftypes)

add_executable(gtmsecshr_real		${gtmsecshr_real_SOURCES})
target_link_libraries(gtmsecshr_real libmumps)
set_target_properties(gtmsecshr_real PROPERTIES
  OUTPUT_NAME gtmsecshr
  RUNTIME_OUTPUT_DIRECTORY ${GTM_BINARY_DIR}/gtmsecshrdir
  )
add_dependencies(gtmsecshr_real		gen_gtm_threadgbl_deftypes)

add_executable(mupip			${mupip_SOURCES})
target_link_libraries(mupip		libmupip libmumps libstub)
list(APPEND with_export mupip)

add_executable(lke			${lke_SOURCES})
target_link_libraries(lke		liblke libmumps libgnpclient libmumps libgnpclient libcmisockettcp)
list(APPEND with_export lke)


add_executable(gtcm_server		${gtcm_server_SOURCES})
target_link_libraries(gtcm_server	libgtcm libmumps libstub)
list(APPEND with_export gtcm_server)

add_executable(gtcm_gnp_server		${gtcm_gnp_server_SOURCES})
target_link_libraries(gtcm_gnp_server	libgnpserver liblke libmumps libcmisockettcp libstub)
list(APPEND with_export gtcm_gnp_server)

add_executable(gtcm_play		${gtcm_play_SOURCES})
target_link_libraries(gtcm_play		libgtcm libmumps libstub)
list(APPEND with_export gtcm_play)

add_executable(gtcm_pkdisp		${gtcm_pkdisp_SOURCES})
target_link_libraries(gtcm_pkdisp	libgtcm libmumps libstub)

add_executable(gtcm_shmclean 		${gtcm_shmclean_SOURCES})
target_link_libraries(gtcm_shmclean	libgtcm libmumps libstub)

add_executable(semstat2		 	${semstat2_SOURCES})

add_executable(ftok 			${ftok_SOURCES})
target_link_libraries(ftok 		libmumps libstub)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "SunOS")
  add_executable(gtm_svc		${gtm_svc_SOURCES})
  target_link_libraries(gtm_svc		libmumps libgnpclient libcmisockettcp libgtmrpc)
endif()
foreach(t ${with_export})
  set_target_properties(${t} PROPERTIES
    LINK_FLAGS "${gtm_link}"
    LINK_DEPENDS "${gtm_dep}"
    )
  add_dependencies(${t} gen_export)
endforeach()

add_library(libgtmshr MODULE ${libgtmshr_SOURCES})
set_property(TARGET libgtmshr PROPERTY OUTPUT_NAME gtmshr)
target_link_libraries(libgtmshr libmumps libgnpclient libcmisockettcp)
set_target_properties(libgtmshr PROPERTIES
  LINK_FLAGS "${libgtmshr_link}"
  LINK_DEPENDS "${libgtmshr_dep}"
  )
add_dependencies(libgtmshr gen_export)
add_dependencies(mumps libgtmshr)

if(is_encryption_supported)
  # Iterate over the list of GPG related libraries
  foreach(gpglib gpg-error gpgme gcrypt)
    # For each library, we need a new CMake variable, hence GPGLIB_${gpglib}
    find_library(GPGLIB_${gpglib} NAME ${gpglib} PATHS ${CMAKE_LIBRARY_PATH})
    # Append the found library to the list
    set(GPG_LIBRARIES ${GPG_LIBRARIES} ${GPGLIB_${gpglib}})
  endforeach()

  add_library(libgtmcrypt MODULE ${libgtmcrypt_SOURCES})
  set_target_properties(libgtmcrypt PROPERTIES
    OUTPUT_NAME gtmcrypt
    COMPILE_DEFINITIONS "USE_GCRYPT -DUSE_AES256CFB"
    LIBRARY_OUTPUT_DIRECTORY ${GTM_BINARY_DIR}/plugin
    )
  target_link_libraries(libgtmcrypt ${GPG_LIBRARIES})
  install(TARGETS libgtmcrypt DESTINATION ${GTM_INSTALL_DIR}/plugin)

  add_executable(maskpass ${maskpass_SOURCES})
  target_link_libraries(maskpass ${GPG_LIBRARIES})
  set_target_properties(maskpass PROPERTIES
    COMPILE_DEFINITIONS USE_GCRYPT
    RUNTIME_OUTPUT_DIRECTORY ${GTM_BINARY_DIR}/plugin/gtmcrypt
    )
  install(TARGETS maskpass DESTINATION ${GTM_INSTALL_DIR}/plugin/gtmcrypt)

  foreach(f
      add_db_key.sh
      build.sh
      encrypt_sign_db_key.sh
      gen_keypair.sh
      gen_sym_hash.sh
      gen_sym_key.sh
      gtmcrypt.tab
      gtmcrypt_dbk_ref.c
      gtmcrypt_dbk_ref.h
      gtmcrypt_interface.h
      gtmcrypt_pk_ref.c
      gtmcrypt_pk_ref.h
      gtmcrypt_dbk_ref.c
      gtmcrypt_dbk_ref.h
      gtmcrypt_ref.c
      gtmcrypt_ref.h
      gtmcrypt_sym_ref.h
      gtmxc_types.h
      import_and_sign_key.sh
      install.sh
      maskpass.c
      pinentry-gtm.sh
      pinentry.m
      pinentry.m
      show_install_config.sh
      )
    set(f_in "${GTM_SOURCE_DIR}/sr_unix/${f}")
    set(f_out "${GTM_BINARY_DIR}/plugin/gtmcrypt/${f}")
    add_custom_command(
      OUTPUT "${f_out}"
      DEPENDS "${f_in}"
      COMMAND ${CMAKE_COMMAND} -E copy "${f_in}" "${f_out}"
      )
    if("${f}" MATCHES "\\.sh$")
      set(permissions PERMISSIONS ${install_permissions_script})
    else()
      set(permissions "")
    endif()
    install(FILES "${f_out}" DESTINATION ${GTM_INSTALL_DIR}/plugin/gtmcrypt ${permissions})
    list(APPEND files_to_place "${f_out}")
  endforeach()
endif()

install(TARGETS
  mumps
  dse
  lke
  geteuid
  gtcm_server
  gtcm_gnp_server
  gtcm_pkdisp
  gtcm_play
  gtcm_shmclean
  semstat2
  ftok
  gtmsecshr
  mupip
  libgtmshr
  DESTINATION ${GTM_INSTALL_DIR}
  )
install(TARGETS gtmsecshr_real DESTINATION ${GTM_INSTALL_DIR}/gtmsecshrdir)

# <r>.m -> <R>.m
#file(GLOB m_files_sr_port RELATIVE ${GTM_SOURCE_DIR}/sr_port ${GTM_SOURCE_DIR}/sr_port/*.m)
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
file(GLOB m_files_sr_unix RELATIVE ${GTM_SOURCE_DIR}/sr_unix ${GTM_SOURCE_DIR}/sr_unix/*.m)
file(GLOB mpt_files_sr_port RELATIVE ${GTM_SOURCE_DIR}/sr_port ${GTM_SOURCE_DIR}/sr_port/*.mpt)
file(GLOB mpt_files_sr_unix RELATIVE ${GTM_SOURCE_DIR}/sr_unix ${GTM_SOURCE_DIR}/sr_unix/*.mpt)

set(gtm_chset_dir_ "")
set(gtm_chset_dir_UTF-8 "/utf8")

foreach(d sr_port sr_unix)
  foreach(m ${m_files_${d}})
    get_filename_component(m_name "${m}" NAME_WE)
    string(TOUPPER "${m_name}" m_upper)
    if("${m}" MATCHES "^gde")
      foreach(gtm_chset "" "UTF-8")
        set(m_out "${GTM_BINARY_DIR}${gtm_chset_dir_${gtm_chset}}/${m_upper}.m")
        add_custom_command(
          OUTPUT "${m_out}"
          DEPENDS ${GTM_SOURCE_DIR}/${d}/${m}
          COMMAND ${CMAKE_COMMAND} -E copy ${GTM_SOURCE_DIR}/${d}/${m} "${m_out}"
          )
        list(APPEND files_to_place "${m_out}")
        list(APPEND gtm_chset_routines_${gtm_chset} "${m_out}")
      endforeach()
    else()
      set(m_out "${GTM_BINARY_DIR}/${m_upper}.m")
      add_custom_command(
        OUTPUT "${m_out}"
        DEPENDS ${GTM_SOURCE_DIR}/${d}/${m}
        COMMAND ${CMAKE_COMMAND} -E copy ${GTM_SOURCE_DIR}/${d}/${m} "${m_out}"
        )
      install(FILES "${m_out}" DESTINATION ${GTM_INSTALL_DIR})
      list(APPEND files_to_place "${m_out}")
    endif()
  endforeach()
  foreach(m ${mpt_files_${d}})
    string(TOUPPER "_${m}" m_upper)
    string(REGEX REPLACE "MPT$" "m" m_out "${GTM_BINARY_DIR}/${m_upper}")
    add_custom_command(
      OUTPUT "${m_out}"
      DEPENDS ${GTM_SOURCE_DIR}/${d}/${m}
      COMMAND ${CMAKE_COMMAND} -E copy ${GTM_SOURCE_DIR}/${d}/${m} "${m_out}"
      )
    install(FILES "${m_out}" DESTINATION ${GTM_INSTALL_DIR})
    list(APPEND files_to_place "${m_out}")
  endforeach()
endforeach()

set(files)
foreach(f
  gtm_descript.h
  gtm_limits.h
  gtm_sizeof.h
  gtm_stdio.h
  gtm_stdlib.h
  gtm_string.h
  gtm_strings.h
  gtmxc_types.h
  main_pragma.h
  arch.gtc
  gtcm_run.gtc
  gtcm_slist.gtc
  gdedefaults.gtc
  gtmcshrc.gtc
  gtmprofile.gtc
  gtmprofile_preV54000.gtc
  gtmstart.gtc
  gtmstop.gtc
  dse.hlp
  gde.hlp
  lke.hlp
  mumps.hlp
  mupip.hlp
  custom_errors_sample.txt
  )
  select_file(src ${f})
  list(APPEND files ${src})
endforeach()
install(FILES ${files} DESTINATION ${GTM_INSTALL_DIR})

set(scripts)
foreach(f
  gtm.gtc
  gtmbase.gtc
  )
  select_file(src ${f})
  list(APPEND scripts ${src})
endforeach()
install(FILES ${scripts}
  DESTINATION ${GTM_INSTALL_DIR}
  PERMISSIONS ${install_permissions_script}
  )

find_program(ICUCONFIG NAMES icu-config)
if(ICUCONFIG)
  execute_process(
    COMMAND ${ICUCONFIG} --version
    OUTPUT_VARIABLE icu_version
    RESULT_VARIABLE icu_failed
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(icu_failed)
    message(FATAL_ERROR "Command\n ${ICUCONFIG} --version\nfailed (${icu_failed}).")
  elseif("x${icu_version}" MATCHES "^x([0-9]+\\.[0-9]+)")
    set(gtm_icu_version "${CMAKE_MATCH_1}")
  else()
    message(FATAL_ERROR "Command\n ${ICUCONFIG} --version\nproduced unrecognized output:\n ${icu_version}")
  endif()
else()
  message(FATAL_ERROR "Unable to find 'icu-config'.  Set ICUCONFIG in CMake cache.")
endif()

if (gtm_icu_version GREATER 10)
  message("libicu version ${gtm_icu_version} > 10")
  string(REGEX REPLACE "([0-9])([0-9]).*" "\\1.\\2" gtm_icu_version "${gtm_icu_version}")
  message("Fixing gtm_icu_version to ${gtm_icu_version}.\nThis had better be the default")
endif()

foreach(gtm_chset "" "UTF-8")
  foreach(in ${gtm_chset_routines_${gtm_chset}})
    string(REPLACE ".m" ".o" out "${in}")
    get_filename_component(out_dir "${out}" PATH)
    add_custom_command(
      OUTPUT ${out}
      DEPENDS ${in} ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake mumps
      COMMAND ${CMAKE_COMMAND}
        -D gtm_dist=${GTM_BINARY_DIR}
        -D gtmroutines=.
        -D gtm_chset=${gtm_chset}
        -D gtm_icu_version=${gtm_icu_version}
        -D mumps=$<TARGET_FILE:mumps>
        -D "args=${in}"
        -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
      WORKING_DIRECTORY ${out_dir}
      VERBATIM
      )
    list(APPEND files_to_place ${out})
    install(FILES "${out}" DESTINATION ${GTM_INSTALL_DIR}${gtm_chset_dir_${gtm_chset}})
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
    "Change -segment DEFAULT -block=2048 -file=\$gtm_dist/${help}help.dat
Change -region DEFAULT -record=1020 -key=255
exit")
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in
                 ${GTM_BINARY_DIR}/${help}help.in1)
  set(CMAKE_CONFIGURABLE_FILE_CONTENT
    "Do ^GTMHLPLD
${GTM_SOURCE_DIR}/sr_port/${${help}_hlp}
Halt")
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in
                 ${GTM_BINARY_DIR}/${help}help.in2)
  set(env
    -D gtm_dist=${GTM_BINARY_DIR}
    -D gtmroutines=.
    -D gtmgbldir=${GTM_BINARY_DIR}/${help}help
    )
  add_custom_command(
    OUTPUT ${help}help.dat ${help}help.gld
    DEPENDS ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake mumps mupip
            ${GTM_BINARY_DIR}/${help}help.in1
            ${GTM_BINARY_DIR}/${help}help.in2
    COMMAND ${CMAKE_COMMAND} -E remove ${help}help.dat ${help}help.gld
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:mumps> -D "args=-run;GDE"
      -D input_file=${GTM_BINARY_DIR}/${help}help.in1
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:mupip> -D "args=create"
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:mumps> -D "args=-direct"
      -D input_file=${GTM_BINARY_DIR}/${help}help.in2
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
    )
  list(APPEND files_to_place ${help}help.dat)
  install(FILES ${GTM_BINARY_DIR}/${help}help.dat DESTINATION ${GTM_INSTALL_DIR})
endforeach()
#-----------------------------------------------------------------------------

set(GTM_TOP "${GTM_BINARY_DIR}")
configure_file(sr_unix/gpgagent.tab.in plugin/gpgagent.tab @ONLY)
set(GTM_TOP "${CMAKE_INSTALL_PREFIX}/${GTM_INSTALL_DIR}")
configure_file(sr_unix/gpgagent.tab.in CMakeFiles/plugin/gpgagent.tab @ONLY)
install(FILES ${GTM_BINARY_DIR}/CMakeFiles/plugin/gpgagent.tab DESTINATION ${GTM_INSTALL_DIR}/plugin)

install(FILES sr_unix/configure.gtc DESTINATION ${GTM_INSTALL_DIR}
  PERMISSIONS ${install_permissions_script}
  RENAME configure
  )

install(FILES sr_unix/gtminstall.sh DESTINATION ${GTM_INSTALL_DIR}
  PERMISSIONS ${install_permissions_script}
  RENAME gtminstall
  )

install(FILES sr_unix/lowerc_cp.sh DESTINATION ${GTM_INSTALL_DIR}
  RENAME lowerc_cp
  )

install(FILES COPYING DESTINATION ${GTM_INSTALL_DIR})

add_custom_target(place_files ALL DEPENDS ${files_to_place})

