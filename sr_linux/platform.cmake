#################################################################
#								#
# Copyright (c) 2013-2018 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2017-2018 Stephen L Johnson.			#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

set(srdir "sr_linux")
if("${CMAKE_SIZEOF_VOID_P}" EQUAL 4)
  if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv6l")
    set(arch "armv6l")
    set(bits 32)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -marm -march=armv6")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,--defsym,__armv6l__=1 -Wa,-mcpu=arm1176jzf-s")
  elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l")
    set(arch "armv7l")
    set(bits 32)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -marm -march=armv7-a")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,--defsym,__armv7l__=1 -Wa,-march=armv7-a")
  else()
    set(arch "x86")
    set(bits 32)
    set(FIND_LIBRARY_USE_LIB64_PATHS FALSE)
    # Set arch to i586 in order to compile for Galileo
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -march=i586")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,-march=i586")
# (Sam): I am not really sure if we need this at all. The Linker has
#        no issues finding the symbols. If we add _, it now has trouble.
# For Cygwin, we need to change the assembly symbols to start with _.
# See http://www.drpaulcarter.com/pcasm/faq.php, esp. the examples in the zip files.
# UPDATE: Now I figure out that this is needed for 32 bit CYGWIN ONLY. 64 bit keeps the same symbols!
    if(CYGWIN)
      list(APPEND CMAKE_ASM_COMPILE_OBJECT "objcopy --prefix-symbols=_ <OBJECT>")
    endif()
  endif()
else()
  if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
    set(bits 64)
    set(arch "aarch64")
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -march=armv8-a -mcpu=cortex-a53")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,-march=armv8-a")
    # gcc on AARCH64 works with -ggdb3 but clang on AARCH64 does not so temporarily enable the below only for gcc
    if (CMAKE_COMPILER_IS_GNUCC)
      set(CMAKE_ASM_FLAGS_DEBUG "-Wa,-ggdb3 ${CMAKE_ASM_FLAGS_DEBUG}")
    endif()
  else()
    set(arch "x86_64")
    set(bits 64)
  endif()
endif()
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -include ${YDB_SOURCE_DIR}/sr_port/ydbmerrors.h")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -x assembler-with-cpp")
# Platform directories
list(APPEND gt_src_list sr_linux)
if(${bits} EQUAL 32)
  if("${arch}" MATCHES "armv[67]l")
    list(APPEND gt_src_list sr_armv7l)
  else()
    list(APPEND gt_src_list sr_i386 sr_x86_regs sr_unix_nsb)
  endif()
else()
  if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
    list(APPEND gt_src_list sr_aarch64)
  else()
    list(APPEND gt_src_list sr_x86_64 sr_x86_regs)
    set(gen_xfer_desc 1)
  endif()
endif()

# Assembler
set(CMAKE_INCLUDE_FLAG_ASM "-Wa,-I") # gcc -I does not make it to "as"

# Compiler
if(${CYGWIN})
  # (VEN/SMH): Looks like we need to add the defsym to tell the assembler to define 'cygwin'
  set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,--defsym,cygwin=1")
else()
  # Cygwin must have -ansi undefined (it adds __STRICT_ANSI__ which undefines some important prototypes like fdopen())
  #   See http://stackoverflow.com/questions/21689124/mkstemp-and-fdopen-in-cygwin-1-7-28
  # Cygwin warns if you add -fPIC that the compiled code is already position
  # independent. So don't add -fPIC
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -fPIC ")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsigned-char -Wmissing-prototypes -Wreturn-type -Wpointer-sign")
# Add flags for warnings that we want and don't want.
# First enable Wall. That will include a lot of warnings. In them, disable a few. Below is a comment from sr_linux/gtm_env_sp.csh
# on why these warnings specifically are disabled.
#	The following warnings would be desirable, but together can result in megabytes of warning messages. We
#	should look into how hard they would be to clean up. It is possible that some header changes could
#	reduce a large number of these.
#		set desired_warnings = ( $desired_warnings -Wconversion -Wsign-compare )
#	We should also look into how hard these would be to restore. Some of the warnings come from generated
#	code and macro use, making them harder to deal with.
# Note: -Wimplicit not explicitly mentioned since it is enabled by Wall
# Note: -Wuninitialized not explicitly mentioned since it is enabled by Wall
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-result -Wno-parentheses -Wno-unused-value -Wno-unused-variable")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-char-subscripts")
if (CMAKE_COMPILER_IS_GNUCC)
  # In gcc 10.1.0 we noticed the below two warnings show up at link time and not at compile time (see commit message for details).
  # Those warnings highlighted code that knew what it was doing and it was not easy to restructure the code to avoid the warning.
  # So disable this warning for now at least. Not yet sure if it is a gcc regression. If so this is a reminder to re-enable
  # these warnings. If not (i.e. it is a gcc feature), keep this permanently disabled.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-return-local-addr")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-overflow")
  # There was a bogus warning involving iosocket_close.c which we could not address and believe to be a GCC bug.
  # Therefore, we have disabled the -Wmaybe-uninitialized warning until that bug plus many others that make this warning
  # largely unusable are fixed.
  #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmaybe-uninitialized")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-maybe-uninitialized -Wno-unused-but-set-variable")
  # gcc 6.3.0 is known to have -Wmisleading-indentation. And gcc 4.8.5 is known to not have that.
  # Not sure what the intermediate versions support so we add this warning flag only for versions >= 6.3.0
  if(${CMAKE_C_COMPILER_VERSION} STRGREATER "6.3.0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmisleading-indentation")
  endif()
else()
  # -Wno-unused-but-set-variable and -Wmaybe-uninitialized are unsupported on clang/llvm
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wvla")

# Below is an optimization flag related description copied from sr_linux/gtm_env_sp.csh
#	-fno-defer-pop to prevent problems with assembly/generated code with optimization
#	-fno-strict-aliasing since we don't comply with the rules
#	-ffloat-store for consistent results avoiding rounding differences
#	-fno-omit-frame-pointer so %rbp always gets set up (required by caller_id()). Default changed in gcc 4.6.
# All these are needed only in case of pro builds (if compiler optimization if turned on).
# But they are no-ops in case of a dbg build when optimization is turned off so we include them in all cmake builds.
if (CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-defer-pop -fno-strict-aliasing -ffloat-store -fno-omit-frame-pointer")
else()
  # -fno-defer-pop is unsupported on clang/llvm
  # -ffloat-store is unsupported on clang/llvm
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing -fno-omit-frame-pointer")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	# Newer versions of Linux by default include -fstack-protector in gcc. This causes the build to slightly bloat
	# in size and have a runtime overhead (as high as 5% extra CPU cost in our experiments). So keep that option
	# enabled only for DEBUG builds of YottaDB.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all")
else()
	# For "Release" or "RelWithDebInfo" type of builds, keep this option disabled for performance reasons
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector")
  if (CMAKE_COMPILER_IS_GNUCC)
    # Avoid using LTO on armv6l due to excessive link time plus creates many undefs due to failed/missing plugins
    if (NOT (("${arch}" STREQUAL "armv6l") OR ("${arch}" STREQUAL "aarch64")))
      # Enable (a) link time optimization and (b) use gold linker.
      # (a) was seen to reduce the size of libyottadb.so by 5% and improve runtimes by 7% on a simple database test
      # (b) gold linker was seen to slightly (~ 0.1%) improve build times and run times compared to default ld linker.
      # Use -flto=N where N is number of available CPUs to speed up the link time.
      include(ProcessorCount)
      ProcessorCount(NUMCPUS)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=${NUMCPUS} -fuse-ld=gold")
      set(CMAKE_AR "gcc-ar")		# needed on some versions of gcc to get -flto working
      set(CMAKE_RANLIB "gcc-ranlib")	# needed on some versions of gcc to get -flto working
      message("*** Production build using LTO")
    else()
      message("*** Production build not using LTO due to issues building on armv6l or aarch64 (see #498)")
    endif()
  endif()
endif()

# On ARM Linux, gcc by default does not include -funwind-tables whereas it does on x86_64 Linux.
# This is needed to get backtrace() (used by caller_id.c etc.) working correctly.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -funwind-tables")

add_definitions(
  #-DNOLIBGTMSHR #gt_cc_option_DBTABLD=-DNOLIBGTMSHR
  -D_GNU_SOURCE
  -D_FILE_OFFSET_BITS=64
  -D_XOPEN_SOURCE=600
  -D_LARGEFILE64_SOURCE
  )

# Linker
set(ydb_link  "-Wl,-u,gtm_filename_to_id -Wl,-u,ydb_zstatus -Wl,--version-script,\"${YDB_BINARY_DIR}/ydbexe_symbols.export\"")
set(gtm_dep   "${YDB_BINARY_DIR}/ydbexe_symbols.export")

set(libyottadb_link "-Wl,-u,ydb_ci -Wl,-u,gtm_filename_to_id -Wl,-u,gtm_is_main_thread")
set(libyottadb_link "${libyottadb_link} -Wl,-u,accumulate -Wl,-u,is_big_endian -Wl,-u,to_ulong")
set(libyottadb_link "${libyottadb_link} -Wl,-u,gtm_ci")
set(libyottadb_link "${libyottadb_link} -Wl,-u,gtm_cip")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_call_variadic_plist_func")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_child_init")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_ci_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_cip")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_cip_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_ci_get_info")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_ci_get_info_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_ci_tab_open")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_ci_tab_open_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_ci_tab_switch")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_ci_tab_switch_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_data_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_data_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_delete_excl_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_delete_excl_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_delete_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_delete_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_file_id_free")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_file_id_free_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_file_is_identical")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_file_is_identical_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_file_name_to_id")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_file_name_to_id_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_fork_n_core")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_free")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_get_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_get_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_hiber_start")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_hiber_start_wait_any")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_incr_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_incr_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_lock_decr_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_lock_decr_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_lock_incr_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_lock_incr_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_lock_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_lock_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_malloc")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_message")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_message_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_node_next_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_node_next_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_node_previous_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_node_previous_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_set_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_set_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_stdout_stderr_adjust")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_stdout_stderr_adjust_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_str2zwr_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_str2zwr_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_subscript_next_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_subscript_next_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_subscript_previous_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_subscript_previous_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_thread_is_main")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_timer_cancel")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_timer_cancel_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_timer_start")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_timer_start_t")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_tp_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_tp_st")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_zwr2str_s")
set(libyottadb_link "${libyottadb_link} -Wl,-u,ydb_zwr2str_st")
set(libyottadb_link "${libyottadb_link} -Wl,--version-script,\"${YDB_BINARY_DIR}/yottadb_symbols.export\"")
set(libyottadb_dep  "${YDB_BINARY_DIR}/ydbexe_symbols.export")

if(${bits} EQUAL 32)
  if("${arch}" MATCHES "armv[67]l")
    set(libsyslibs "-lelf -lncurses -lm -ldl -lc -lpthread -lrt")
  else()
    set(libsyslibs "-lncurses -lm -ldl -lc -lpthread -lrt")
  endif()
else()
  set(libsyslibs "-lelf -lncurses -lm -ldl -lc -lpthread -lrt")
endif()

if(ENABLE_PROFILING)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
endif()
