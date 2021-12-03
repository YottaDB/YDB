#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2011-2021 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#
# Create GTMDefinedTypesInit.m (currently) used by offset.m and gtmpcat.m
#
# Unset gtmcompile to prevent possible issues with production version
#
unsetenv gtmcompile
setenv pushdsilent
set gtmver = ""
set gtmtyp = ""
set ourdir = `pwd`
#
# Parse arguments
#
@ argc1 = 1
foreach arg ($argv)
    @ argc1 = $argc1 + 1
    if ($?skip) then
	unset skip
    else
	switch($arg)
	    case "-help":
	    case "-?":
		echo " "
		echo "Command format is:"
		echo " "
		echo "$0 <gtmver> <p<ro> | d<bg>>"
		echo " "
		echo "Where:"
		echo " "
		echo "    - gtmver is version name - e.g. V51000 or V54002. Only one version can be specified."
		echo "    - Indicate pro or debug version. Default is whatever is current. They generate somewhat"
		echo "      different files."
		echo " "
		echo "The GTM defined types file (GTMDefinedTypesInit.m) is created in the current working directory."
		echo " "
		exit 0
	    default:
		if ("" == "$gtmver") then
		    set gtmver = "$arg"
		else if ("" == "$gtmtyp") then
		    if ("p" == "$arg" || "pro" == "$arg") then
			set gtmtyp = "p"
		    else if ("d" == "$arg" || "dbg" == "$arg") then
		        set gtmtyp = "d"
		    else
			echo "GENGTMDEFTYPES-E-INVTYP Invalid type specified - should be {p|pro} for pro version or"
			echo "                        {d|dbg} for a debug version"
			exit 1
		    endif
		else
		    echo "GENGTMDEFTYPES-E-ONEVER Only one GT.M version can be specified"
		    exit 1
		endif
	endsw
    endif
end
if ("" == "$gtmver") set gtmver = ${gtm_dist:h:t}
if ("" == "$gtmtyp") set gtmtyp = ${gtm_dist:t:s/bg//:s/ro//:s/ta//}
if ("b" == "$gtmtyp") set gtmtyp = "p" # Switch BTA to PRO
echo
echo "Starting generation of GTM defined types initialization routine for version $gtmver ($gtmtyp)"
echo
#
# Create a temporary directory to put the object and other temporary file(s) in. Solves the
# problems of cluttering up directories with remnants and also HPUX's issues with creating a
# GT.M object file on an NFS directory.
#
set tmpdir = "/tmp/gengtmdeftypes.${USER}.$$"
if (! -e $tmpdir) then
    mkdir $tmpdir
    if (0 != $status) then
	echo "GENGTMDEFTYPES-E-OBJDIRFAIL Failed to create object directory -- terminating -- tmpdir $tmpdir not removed"
	exit 1
    endif
endif
#
# Set the version we will run with - note $gtmroutines not reset by version (setactive*.csh)
#
if ($?usecurpro) then
	set setactive_parms=(p $gtmtyp); source $gtm_tools/setactive.csh
	set proddist=$gtm_dist
	setenv gtmroutines "$tmpdir $gtm_dist"
endif
pushd $tmpdir
#

# Run in normal M mode since this build has no UTF-8 dependencies but has some complications
# due to how it switches versions around.
#
unsetenv gtm_chset
#
set setactive_parms=($gtmver $gtmtyp); source $gtm_tools/setactive.csh
if ($?proddist) then
	setenv gtm_dist $proddist
endif

# If gengtmdeftypes.csh is invoked for old versions, point cpfrom_tools/cpfrom_pct to the location of
# the helper files, typically $gtm_tools/$gtm_pct of gtm_curpro
if (! $?cpfrom_tools) set cpfrom_tools = $gtm_tools
if (! $?cpfrom_pct) set cpfrom_pct = $gtm_pct
cp $cpfrom_pct/{decomment.m,scantypedefs.m} .
cp $cpfrom_tools/{gtmexcludetypelist.txt,stripmine.awk,xtrgtmtypes.awk} .
#
# Create a list of types defined by GTM by reading the header files for types defined there. This allows us to only
# pay attention to these GTM defined structures later when we process compiler pre-processor output where system header
# files will contribute lots of irrelevant definitions. The steps takes are - for each include file:
#   1. Run it through decomment.m which strips comments from the module to ease parsing.
#   2. Run through xtrgtmtypes.awk script which locates typedef statements and records their types (topname and bottom name)
#      recording them in the output file.
#   3. Sort the generated names and eliminate duplicates.
#
rm -f gtmtypelist.txt >& /dev/null
rm -f gtmtypelist.tmp >& /dev/null
ls -1 $gtm_inc/*.h | awk '{x="$gtm_dist/mumps -run decomment "$1" | awk -f xtrgtmtypes.awk >> gtmtypelist.tmp"; system(x);}'
sort gtmtypelist.tmp | uniq > gtmtypelist.txt
rm -f gtmtypelist.tmp >& /dev/null
if (! -e gtmtypelist.txt) then
    echo "GENGTMDEFTYPES-E-CANTCONT Cannot continue due to failure of earlier commands -- tmpdir $tmpdir not removed"
    popd
    exit 1
endif
#
# Drive the big kahuna (scantypedefs) to:
#   1. Read the list of GTM types we want to pay attention to created in the last step.
#   2. Read a list of ignored types which we don't pay attention to because they are irrelevant for our purposes
#      or they cause errors, etc.
#   3. Create a C program with every GT.M include (with some exceptions listed in scantypedefs.m) and
#      run it through the C pre-processor.
#   4. Process the pre-processor output with the awk script stripmine.awk which performs the following actions:
#        a. Eliminates all statements other than "typedef" statements.
#        b. Puts spaces around all tokens, all special chars.
#        c. Eliminates multiple adjacent white space chars to make easy parsing for $ZPiece().
#   5. Reads the created input file, parsing the typedef statements into a structure.
#   6. Resolves types to their simplest types (undoing typedefs)
#   7. Imbedded structures are expanded so each field in a structure is resolved to the maximum point
#      we can de-evolve it to using GTM types and base types.
#   8. Create another C program generating macros to extract information about each field within a struct or union
#      and print it. Information includes name, type, offset, length, and dimension plus overal length and type
#      of the struct.
#   9. Compile and link the created program.
#  10. Run the program.
#  11. Parse the output and fill in the length/offset fields of the structure.
#  12. Write out GTMDefinedTypesInit.m which initializes gtmtypes, gtmstructs, and gtmunions arrays for use by
#      gtmpcat, offset, and eventually perhaps other utilities.
#
rm -f GTMDefinedTypesInit.m scantypedefs-fail.zshowdmp-*.txt >& /dev/null
rm -f scantypedefs-DEBUG-zshowdump.txt >& /dev/null
if (`uname -s` == "HP-UX" && `uname -m` != "ia64") setenv gt_cc_options_common "$gt_cc_options_common +W 454" # Suppress -O0 warning
$gtm_dist/mumps -run scantypedefs $gtmver
ls scantypedefs-fail.zshowdmp-*.txt >& /dev/null
if (0 == $status) then
    echo
    echo "*** $0 halted due to failures -- tmpdir $tmpdir not removed"
    echo
    popd
    exit 1
endif
if (-e scantypedefs-DEBUG-zshowdump.txt) mv -f scantypedefs-DEBUG-zshowdump.txt $ourdir
if ( ! -e GTMDefinedTypesInit.m )  then
    echo "*** $0 halted due to absence of GTMDefinedTypesInit.m -- tmpdir $tmpdir not removed"
    popd
    exit 1
endif
mv -f GTMDefinedTypesInit.m $ourdir
if (-e struct_padding_warn.out) then
	cp struct_padding_warn.out $ourdir
endif
popd
rm -fr $tmpdir >& /dev/null
echo "gengtmdeftypes.csh complete"
exit 0
