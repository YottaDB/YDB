#################################################################
#								#
# Copyright (c) 2012-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
foreach(v
    ydb_dist
    gtmroutines
    gtm_chset
    gtm_icu_version
    gtm_inc
    gtm_tools
    ydb_gbldir
    LC_ALL
    )
  if(DEFINED ${v})
    set("ENV{${v}}" "${${v}}")
  endif()
endforeach()
if(input_file)
  set(input_file INPUT_FILE ${input_file})
endif()
if(output_file)
  set(output_file OUTPUT_FILE ${output_file})
endif()
execute_process(
  COMMAND ${mumps} ${args}
  ${input_file}
  ${output_file}
  RESULT_VARIABLE res_var
  )
if(NOT "${res_var}" STREQUAL "0")
  # do something here about the failed "process" call...
  message(FATAL_ERROR "Command <${mumps} ${args} ${input_file} ${output_file}> failed with result ='${res_var}'")
endif()
