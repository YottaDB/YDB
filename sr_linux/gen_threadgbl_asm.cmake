#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#								#
# This cmake routine is derived from FIS code.			#
#								#
#################################################################

file(STRINGS ${gtmthreadgblasmaccess} asmaccesstypes REGEX "^[A-Za-z_]+")
foreach(asmaccess ${asmaccesstypes})
  string(REGEX REPLACE "^([A-Za-z_]+)[^A-Za-z_].*$" "ggo_\\1" asm "${asmaccess}")
  file(STRINGS ${gtmthreadgblasmhdr} asmdef REGEX ${asm})
  string(REGEX REPLACE "# +define +([A-Za-z_]+) +([0-9]+)" "\\1 = \\2" asmsign "${asmdef}")
  file(APPEND ${gtmthreadgblasmfile} "${asmsign}\n")
endforeach()
