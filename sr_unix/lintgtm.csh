#################################################################
#								#
#	Copyright 2001, 2010 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
##################################################################################
#
#	lintgtm.csh - lint C sources used to build GT.M
#
#	arguments:
#		$1 -	version number (without punctuation) or code letter:
#			e.g., "V123" => version "V1.2-3" or:
#				"a" => current active (in current process) release
#				"d" => current development release
#				"p" => current production release
#		$2 -	"gtm_bta" => lint bta images ($gtm_vrt/bta)
#			"gtm_dbg" => lint dbg images ($gtm_vrt/dbg)
#			"gtm_pro" => lint pro images ($gtm_vrt/pro)
#		$3 -	any lint options in addition to the defaults
#
##################################################################################

echo "Start of $0 `date`"
echo ""

echo "lint'ed on $HOST"
echo ""

echo "arguments: '$1' '$2' '$3'"
echo ""

echo ""
echo "lint-related aliases:"
echo ""
alias | grep lint | sort
echo ""
echo ""

echo "lint-related environment variables:"
echo ""
env | grep lint | sort
echo ""
echo ""

set lintgtm_status = 0

set lintgtm_start_directory = `pwd`

if ( "$HOSTOS" == "OSF1"  &&  "$MACHTYPE" == "alpha" ) then

	# Check to make certain the system header files have been properly edited
	# to include the #pragma's necessary to preserve 64-bit pointer sizes
	# independent of C compiler options.
	grep xtaso_header_edit /usr/include/stdio.h > /dev/null
	if ( $status != 0 ) then
		echo "lintgtm-E-xtaso_header_edit: system header files do not support 32-bit pointers"
		echo "lintgtm-I-xtaso_header_edit: you need to run \$gtm_tools/xtaso_header_edit as superuser"
		set lintgtm_status = -10
		goto lint.END
	endif
endif

# Verify arguments:

# Default to linting the current version.
if ( $1 == "" ) then
	set p1 = "$gtm_verno"
else
	set p1 = $1
endif

# Default to linting the current images.
if ( $2 == "" ) then

	switch ( `basename $gtm_exe` )
	case "gtm_bta":
			set p2 = "b"
			breaksw

	case "gtm_dbg":
			set p2 = "d"
			breaksw

	case "gtm_pro":
			set p2 = "p"
			breaksw

	endsw

else
	set p2 = $2

endif


# Define image type-specific information.

version $p1 $p2


# Default lint options.

set p3 = "$3"
switch ( $p2 )
case "gtm_bta":
	set gt_lint_options = "$gt_lint_options_common $gt_lint_options_bta $p3"
	breaksw

case "gtm_dbg":
	set gt_lint_options = "$gt_lint_options_common $gt_lint_options_dbg $p3"
	breaksw

case "gtm_pro":
	set gt_lint_options = "$gt_lint_options_common $gt_lint_options_pro $p3"
	breaksw

endsw



if ( $gt_ar_gtmrpc_name == "" ) then
	set gt_lint_gtmrpc_library_option = ""
else
	set gt_lint_gtmrpc_library_option = "llib-l$gt_ar_gtmrpc_name.ln"
endif

cd $gtm_exe


if ( ! -d ./lint ) then
	mkdir ./lint
endif
cd ./lint

# Remove any left-over files.
rm *

set eol_anchor = '$'
set gi = ($gtm_inc)
set gs = ($gtm_src)


cp $gtm_inc/*.h .
ls $gs[1] | egrep '\.c$' | xargs -i cp "$gs[1]/{}" .
chmod +w *.c *.h


set lintgtm_verbose = $?verbose

set lintgtm_liblist = "dse $gt_ar_gtmrpc_name lke mupip stub mumps"

foreach i ( $lintgtm_liblist )

	echo "Start of $i lint library creation: `date`"
	echo "lint options: ${gt_lint_option_output}$i $gt_lint_options" > llib-l$i.log
	echo "" >> llib-l$i.log

	switch ( $i )
	case "dse":
	case "gtmrpc":		# couldn't use "$gt_ar_gtmrpc_name", so had to hard-code "gtmrpc"
	case "lke":
	case "mupip":
	case "stub":
		pwd
		sed -f $gtm_tools/lib_list_lint.sed $gtm_tools/lib$i.list >& lib$i.list
		gt_lint ${gt_lint_option_output}$i $gt_lint_options $gt_lint_options_library `cat lib$i.list` >>& llib-l$i.log
		rm -f `cat lib$i.list`
		breaksw

	case "mumps":
		# (Almost) everything else goes into llib-lmumps.ln, but the list is too long for a single command line
		# so use xargs.  This case must be executed last in the switch statement (because it picks up "everything
		# else") and, hence, must appear last in the for statement.

		# Exclude files that define the same externals (e.g., "main" and the VMS CLI [command line interpreter]
		# emulator arrays):
		pwd
		rm -f gtm.c gtm_svc.c \
			lke.c lke_cmd.c \
			dse.c dse_cmd.c \
			mupip.c mupip_cmd.c \
			daemon.c gtmsecshr.c geteuid.c dtgbldir.c semstat2.c ftok.c

		gt_lint ${gt_lint_option_output}$i $gt_lint_options $gt_lint_options_library *.c >>& llib-l$i.log
		rm *.c
		breaksw

	endsw

	echo "" >> llib-l$i.log
	echo "End of $i lint library creation: `date`"
	echo ""
end

# $shell $gtm_tools/lintshr.csh $p1	# for true parallelism, the following commands would be in lintshr.csh
cp $gtm_src/{gtm.c .
gt_lint $gt_lint_options gtm.c llib-l{mumps,stub}.ln $gt_lint_gtmrpc_library_option $gt_lint_syslibs \
	>& lint.mumps.log

if ( $gt_ar_gtmrpc_name != "" ) then
	cp $gtm_src/gtm_svc.c .
	gt_lint $gt_lint_options gtm_svc.c \
		llib-l{mumps,stub}.ln $gt_lint_gtmrpc_library_option $gt_lint_syslibs >& lint.gtm_svc.log
endif

# $shell $gtm_tools/lintaux.csh $p1	# for true parallelism, the following commands would be in lingaux.csh
cp $gtm_src/{dse,dse_cmd}.c .
gt_lint $gt_lint_options {dse,dse_cmd}.c llib-l{dse,mumps,stub}.ln $gt_lint_syslibs >& lint.dse.log

cp $gtm_src/geteuid.c .
gt_lint $gt_lint_options geteuid.c llib-lmumps.ln $gt_lint_syslibs >& lint.geteuid.log

cp $gtm_src/gtmsecshr.c .
gt_lint $gt_lint_options gtmsecshr.c llib-lmumps.ln $gt_lint_syslibs >& lint.gtmsecshr.log

cp $gtm_src/{lke,lke_cmd}.c .
gt_lint $gt_lint_options {lke,lke_cmd}.c llib-l{lke,mumps,stub}.ln $gt_lint_syslibs >& lint.lke.log

cp $gtm_src/{mupip,mupip_cmd}.c .
gt_lint $gt_lint_options {mupip,mupip_cmd}.c llib-l{mupip,mumps,stub,dse}.ln $gt_lint_syslibs >& lint.mupip.log

cp $gtm_src/semstat2.c .
gt_lint $gt_lint_options semstat2.c $gt_lint_syslibs >& lint.semstat2.log

cp $gtm_src/ftok.c .
gt_lint $gt_lint_options ftok.c $gt_lint_syslibs >& lint.ftok.log

chmod +w *.c
rm *.h *.c


lint.END:

if ( $lintgtm_verbose == "0" ) then
	unset verbose
endif

# Return to starting directory:
cd $lintgtm_start_directory

# Clean up environment variables:
unsetenv lintgtm_gt_as
unsetenv lintgtm_gt_cc

# Clean up local shell variables:
unset lintgtm_liblist
unset lintgtm_start_directory
unset p1
unset p2
unset p3

echo ""
echo "End of $0 `date`"

exit $lintgtm_status
