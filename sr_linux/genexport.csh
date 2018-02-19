#################################################################
#								#
# Copyright 2001, 2010 Fidelity Information Services, Inc	#
#								#
# Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.                                          #
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

############################################################################################
#
#	genexport.csh - to generate the linker script *.export to export
#			all call-in related symbols from libyottadb.so
#	Argument
#		$1 - The pathname of a .exp file that list out all symbols to be exposed
#		$2 - output version script file to be specified with ld --version-script.
#
# 	Example output:
#		{
#			global:
#        			ydb_ci;
#        			ydb_exit;
#        			ydb_zstatus;
#			local:
# 				*;
#		};
############################################################################################

echo "{" >$2
echo "global:"	>>$2
sed 's/\(.*\)/	\1;/g' $1 >>$2
echo "local:"	>>$2
echo " *;"	>>$2
echo "};"	>>$2
