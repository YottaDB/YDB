#################################################################
#								#
# Copyright (c) 2013-2016 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.                                          #
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

set(srdir "sr_linux")
if("${CMAKE_SIZEOF_VOID_P}" EQUAL 4)
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
else()
  set(arch "x86_64")
  set(bits 64)
endif()

# Platform directories
list(APPEND gt_src_list sr_linux)
if(${bits} EQUAL 32)
  list(APPEND gt_src_list sr_i386 sr_x86_regs sr_unix_nsb)
else()
  list(APPEND gt_src_list sr_x86_64 sr_x86_regs)
  set(gen_xfer_desc 1)
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
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi -fPIC ")
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
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-maybe-uninitialized -Wno-char-subscripts -Wno-unused-but-set-variable")

# Below is an optimization flag related description copied from sr_linux/gtm_env_sp.csh
#	-fno-defer-pop to prevent problems with assembly/generated code with optimization
#	-fno-strict-aliasing since we don't comply with the rules
#	-ffloat-store for consistent results avoiding rounding differences
#	-fno-omit-frame-pointer so %rbp always gets set up (required by caller_id()). Default changed in gcc 4.6.
# All these are needed only in case of pro builds (if compiler optimization if turned on).
# But they are no-ops in case of a dbg build when optimization is turned off so we include them in all cmake builds.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-defer-pop -fno-strict-aliasing -ffloat-store -fno-omit-frame-pointer")

add_definitions(
  #-DNOLIBGTMSHR #gt_cc_option_DBTABLD=-DNOLIBGTMSHR
  -D_GNU_SOURCE
  -D_FILE_OFFSET_BITS=64
  -D_XOPEN_SOURCE=600
  -D_LARGEFILE64_SOURCE
  )

# Linker
set(gtm_link  "-Wl,-u,gtm_filename_to_id -Wl,-u,gtm_zstatus -Wl,--version-script,\"${GTM_BINARY_DIR}/gtmexe_symbols.export\"")
set(gtm_dep   "${GTM_BINARY_DIR}/gtmexe_symbols.export")

set(libgtmshr_link "-Wl,-u,gtm_ci -Wl,-u,gtm_filename_to_id -Wl,-u,gtm_is_main_thread")
set(libgtmshr_link "${libgtmshr_link} -Wl,-u,accumulate -Wl,-u,is_big_endian -Wl,-u,to_ulong")
set(libgtmshr_link "${libgtmshr_link} -Wl,--version-script,\"${GTM_BINARY_DIR}/gtmshr_symbols.export\"")
set(libgtmshr_dep  "${GTM_BINARY_DIR}/gtmexe_symbols.export")

if(${bits} EQUAL 32)
  set(libmumpslibs "-lncurses -lm -ldl -lc -lpthread -lrt")
else()
  set(libmumpslibs "-lelf -lncurses -lm -ldl -lc -lpthread -lrt")
endif()
