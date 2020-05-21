;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%YDBPROCSTUCKEXEC

; When invoked by a process using the ydb_procstuckexec (gtm_procstuckexec)
; environemnt variable provides a file with helpful information in the directory
; pointed to by $ydb_log, $gtm_log, $ydb_tmp, $gtm_tmp (whichever is first found to
; be defined, and /tmp if not specified. Using gdb, it captures and summarizes the
; state of the process blocking the process that invoked this program.
; See https://docs.yottadb.com/AdminOpsGuide/basicops.html#environment-variables
; for YottaDB environment variables. The program expects the command line to specify:
; - a message as to the problem that caused a process to invoke this program
; - the pid of the process invoking the script
; - the pid of the blocking process
; - optionally, the number of times to snap the state of the blocking process
;   (to judge whether it is stuck or actually attempting to do work).
; Usage: yottadb -run %YDBPROCSTUCKEXEC mesage waiting_pid blocking_pid count
; Note: No error checking done as this needs to execute as quickly as possible.
	new blockcmd,blockexe,blockpid,callpid,count,env,file,i,io,line,msg,outdir,outfile,procfsdir,prog,tmp
	set io=$io
	set msg=$zpiece($zcmdline," ",1)
	set callpid=$zpiece($zcmdline," ",2)
	set blockpid=$zpiece($zcmdline," ",3)
	set count=$select(4>$zlength($zcmdline," "):1,1:+$zpiece($zcmdline," ",4))
	set outdir=$ztrnlnm("ydb_log")
	set:'$zlength(outdir) outdir=$ztrnlnm("gtm_log")
	set:'$zlength(outdir) outdir=$ztrnlnm("ydb_tmp")
	set:'$zlength(outdir) outdir=$ztrnlnm("gtm_tmp")
	set:'$zlength(outdir) outdir="/tmp/"
	set:"/"'=$zextract(outdir,$zlength(outdir)) outdir=outdir_"/"
	set outfile=outdir_$text(+0)_"_"_$zdate($horolog,"YEAR.MM.DD,24.60.SS")_"_"_msg_"_"_callpid_"_"_blockpid_"_"_count_"_"_$job_".out"
	open outfile:newversion use outfile
	set procfsdir="/proc/"_blockpid_"/"	; directory in /procfs with blocking pid info
	write "Invoked on ",blockpid," by ",callpid," for ",count,$select(1=count:"st",2=count:"nd",3=count:"rd",1:"th")," time; reason: ",msg,!
	write "--------------------",!
	; report command line of blocking pid
	set file=procfsdir_"cmdline"
	open file:readonly use file
	read line	   ; read command line of blocking process
	use outfile close file
	write "Command line of process ",blockpid,!
	write $zpiece(line,$zchar(0),1)
	for i=2:1:$zlength(line,$zchar(0)) write " ",$zpiece(line,$zchar(0),i)
	; report environment of blocking pid
	set file=procfsdir_"environ"
	open file:readonly use file
	read line	   ; read environment of blocking process
	use outfile close file
	write !,"--------------------",!,"Environment of process ",blockpid,!
	for i=1:1:$zlength(line,$zchar(0))-1 write $zpiece(line,$zchar(0),i),!
	write "--------------------",!
	set file="/proc/sys/kernel/yama/ptrace_scope"
	open file:readonly use file
	read line
	use outfile close file
	write file,"=",line," (non-zero value may impede using gdb to capture process state)",!
	write "--------------------",!
	set prog="gdb "_procfsdir_"exe -pid "_blockpid_" -ex ""set width 0"" -ex ""set backtrace limit 100"" -ex ""info threads"" -ex ""backtrace"" -ex ""detach"" -ex ""quit"""
	open "pipe":(shell="/bin/sh":command=prog:readonly)::"pipe" use "pipe"
	for  read line quit:$zeof  use outfile write line,! use "pipe"
	use outfile close "pipe"
	write "--------------------",!
	write "Sending USR1 to ",blockpid,!
	; for releases through 1.28 use 10 for USR1; use signal name for later releases
	set tmp=$zsigproc(blockpid,$select(1.28<+$zpiece($zpiece($zyrelease," ",2),"r",2):"USR1",1:10))
	write "$ZSIGPROC() returned ",tmp,!
	use io close outfile
	zhalt tmp
