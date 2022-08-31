#!/usr/bin/env tcsh

#################################################################
#								#
# Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Original code by Zachary Minneker from Security Innovation.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# This script is a wrapper on top of YDB/fuzzing/fuzz.sh that does fuzz testing with various YottaDB env vars
# set to random values (YDB#841). This script requires "tcsh" to be installed.
#
# This script accepts one parameter.
#
# $1 is output directory where YDB repository is cloned and fuzz testing occurs

# -----------------------------------------------------------
# Kill previously running fuzz tests (if any)
tmux kill-server

if (! -e $1) then
	echo "Directory $1 does not exist. Exiting."
	exit -1
endif

cd $1
rm -rf YDB
git clone https://gitlab.com/YottaDB/DB/YDB.git
cd YDB/fuzzing

# -----------------------------------------------------------
# Before building for fuzzing, unset all env vars that might affect the build
# While unsetting YottaDB env var, we need to also unset the GT.M env var just in case it is set as otherwise it will take effect.
unsetenv ydb_routines gtmroutines
unsetenv ydb_gbldir gtmgbldir
unsetenv ydb_chset gtm_chset
unsetenv ydb_boolean gtm_boolean
unsetenv ydb_side_effects gtm_side_effects
unsetenv ydb_badchar gtm_badchar
unsetenv ydb_compile gtmcompile
unsetenv ydb_etrap gtm_etrap
unsetenv ydb_mstack_size gtm_mstack_size
setenv LC_ALL C

# -----------------------------------------------------------
# Instrument build for fuzzing
make instrument

# -----------------------------------------------------------
# Ready test corpus
make corpus

# -----------------------------------------------------------
# Ready random set of env vars before fuzz testing
touch settings.csh

# Randomly set ydb_gbldir env var
set rand = `shuf -i 0-1 -n 1`
if ($rand) then
	# Create database randomly
	cat > tmp.com << CAT_EOF
	change -segment DEFAULT -file=${PWD}/yottadb.dat
CAT_EOF

	rm -f yottadb.gld yottadb.dat
	setenv ydb_gbldir `pwd`/yottadb.gld	# Need this before the GDE command
	$ydb_dist/yottadb -run GDE @tmp.com
	$ydb_dist/mupip create
	echo "setenv ydb_gbldir $ydb_gbldir" >> settings.csh
else
	echo "unsetenv ydb_gbldir gtmgbldir" >> settings.csh
endif

# Randomly set ydb_chset env var
set rand = `shuf -i 0-1 -n 1`
if ($rand) then
	set chset = "M"
	echo "setenv LC_ALL C" >> settings.csh
	# Set ydb_routines env var based on chset
	echo "setenv ydb_routines \".(. $PWD/build-instrumented)\"" >> settings.csh
else
	set chset = "UTF-8"
	echo "setenv LC_ALL en_US.utf8" >> settings.csh
	echo "setenv ydb_routines \".(. $PWD/build-instrumented/utf8)\"" >> settings.csh
endif
echo "setenv ydb_chset \"$chset\"" >> settings.csh

# Randomly set ydb_boolean env var
set rand = `shuf -i 0-2 -n 1`
if (0 == $rand) then
	set bool = "0"
else if (1 == $rand) then
	set bool = "1"
else
	set bool = "2"
endif
echo "setenv ydb_boolean $bool" >> settings.csh

# Randomly set ydb_side_effects env var
set rand = `shuf -i 0-2 -n 1`
if (0 == $rand) then
	set se = "0"
else if (1 == $rand) then
	set se = "1"
else
	set se = "2"
endif
echo "setenv ydb_side_effects $se" >> settings.csh

# Randomly set ydb_badchar env var
set rand = `shuf -i 0-1 -n 1`
if ($rand) then
	set badchar = "no"
else
	set badchar = "yes"
endif
echo "setenv ydb_badchar \"$badchar\"" >> settings.csh

# Randomly set ydb_compile env var
set rand = `shuf -i 0-7 -n 1`
set compile = ""
set bit = `expr $rand % 4`
if ($bit) then
	set compile = "$compile -dynamic_literals"
endif
set bit = `expr $rand % 2`
if ($bit) then
	set compile = "$compile -embed_source"
endif
set bit = `expr $rand % 1`
if ($bit) then
	set compile = "$compile -noline_entry"
endif
echo "setenv ydb_compile \"$compile\"" >> settings.csh

# Randomly set ydb_etrap env var
set rand = `shuf -i 0-1 -n 1`
if ($rand) then
	set etrap = ""
else
	set etrap = 'Write $ZStatus'
endif
echo "setenv ydb_etrap \'$etrap\'" >> settings.csh

# Randomly set ydb_mstack_size env var
set rand = `shuf -i 0-2 -n 1`
if (0 == $rand) then
	set stacksize = "25"	# minimum
else if (1 == $rand) then
	set stacksize = "272"	# default
else
	set stacksize = "10000"	# maximum
endif
echo "setenv ydb_mstack_size \"$stacksize\"" >> settings.csh

# -----------------------------------------------------------
# Note down settings.csh for historical purposes
set timestamp=`date +%Y_%m_%d_%H:%M:%S`
if (! -e ~/fuzzing) then
	mkdir ~/fuzzing
endif
cp settings.csh ~/fuzzing/settings.csh_$timestamp

# -----------------------------------------------------------
# Now that all pertinent env vars are randomly set, source them more before starting fuzz testing
source settings.csh

# -----------------------------------------------------------
# Start fuzzing
make go

# Watch afl-whatsup output (particularly 'Crashes saved' line)
\watch "afl-whatsup output | tail -11"

