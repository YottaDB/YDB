#!/bin/sh
#################################################################
#								#
# Copyright (c) 2001-2022 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
SHELL=/bin/sh
export SHELL
ext=.m
curdir=`pwd`
utf8=`basename $curdir`
for i in "$@"
do
	base=`basename $i $ext`
	if test $i != $base
	then {
		newf=`echo $base | tr '[:upper:]' '[:lower:]'`
		if test $base != $newf
		then {
			dir=`dirname $i`
			if [ "utf8" = $utf8 ]; then
				echo $dir/$i "---> "  $newf$ext "-> "../$newf$ext
				ln -fs ../$newf$ext $newf$ext
			else
				echo $dir/$i "---> " $dir/$newf$ext
				cp -p $i $dir/$newf$ext
			fi
		}
		fi
	}
	fi
done
