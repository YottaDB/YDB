#################################################################
#								#
#	Copyright 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
foreach(v
    gtm_dist
    gtmroutines
    gtm_chset
    gtm_icu_version
    gtmgbldir
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
  )
