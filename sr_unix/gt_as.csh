#################################################################
#								#
#	Copyright 2001, 2013 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
#################################################################################################
#
#	gt_as.csh - Assemble Unix assembly language program using GT.M assembly language options.
#		    Note: this procedure only assembles one source file at a time.
#
#	Argument
#		The pathname of a single assembly language program in native dialect.
#
#	Output
#		Object code in file with suffix ".o".
#
#	Environment
#		comlist_gt_as	current assembler command set by comlist.csh for appropriate
#				version and image type
#
#################################################################################################

if ( $?comlist_gt_as == "0" ) then
	echo "gt_as-E-nocomlist: gt_as.csh should only be invoked from comlist.csh"
	exit 1
endif

alias	gt_as_local	"$comlist_gt_as"

set os = `uname`
set platform_name = ${os:gs/-//:s,/,,:al}
set mach_type = `uname -m`

set asmlist=($*)
set cmdfile="$gtm_log/gt_as_$$__batch.csh"
set background="&"
if ($HOST:r:r:r =~ {snail,turtle,lespaul,pfloyd,strato}) set background=""

echo 'alias gt_as_local "$comlist_gt_as"' >> $cmdfile

foreach asm ($asmlist)
	set outfile="$gtm_log/gt_as_$$_${asm:t:r}.out"
	set redir=">& $outfile"
	if ( "ia64" == $mach_type && "linux" == $platform_name ) then
		set file=$asm:t:r:r
		set sfile="${gtm_obj}/${file}_cpp.s"
		set ofile="${gtm_obj}/${file}.o"
		echo "(eval 'gt_cpp -E $asm' > $sfile ; eval 'gt_as_local $sfile -o $ofile' ; /bin/rm $sfile)"	\
		     "$redir $background" >> $cmdfile
	else if ( "os390" == $platform_name ) then
		set file=$asm:t:r
		set dbgfile="${gtm_obj}/${file}.dbg"
		echo "(eval 'gt_as_local $asm' ; if ( -e $dbgfile ) chmod ugo+r $dbgfile) $redir $background" >> $cmdfile
	else
		echo "eval 'gt_as_local $asm' $redir $background" >> $cmdfile
	endif
end

echo "wait" >> $cmdfile

set cmdout="$gtm_log/gt_as_$$__batch.out"
source $cmdfile >& $cmdout

set stat=$status

foreach asm ($asmlist)
	set outfile="$gtm_log/gt_as_$$_${asm:t:r}.out"
	/bin/cat $outfile
	/bin/rm $outfile
end

if ($stat) then
	/bin/cat $cmdout
else
	/bin/rm $cmdfile
	/bin/rm $cmdout
endif

exit 0
