$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$!
$!	KITINSTAL.COM PROCEDURE FOR THE GT.CX PRODUCT
$!
$ ON CONTROL_Y THEN VMI$CALLBACK CONTROL_Y
$ ON WARNING THEN EXIT $STATUS
$ IF P1 .EQS. "VMI$_INSTALL" THEN GOTO INSTALL
$ IF P1 .EQS. "VMI$_POSTINSTALL" THEN GOTO POSTINSTALL
$ IF P1 .EQS. "VMI$_IVP" THEN GOTO IVP
$ EXIT VMI$_UNSUPPORTED
$!
$INSTALL:
$ TYPE SYS$INPUT

  GT.CX  (c)  COPYRIGHT 1987 - 2000  by  Sanchez Computer Associates
                           ALL RIGHTS RESERVED


$!  the following 2 lines must be maintained
$ GTCX$VMS_VERSION :== 072	! Minimum VMS version required
$ GTCX$DISK_SPACE == 3000	! Minumum disk space on system disk required for install (2x result)
$!
$ IF F$ELEMENT(0,",",VMI$VMS_VERSION) .EQS. "RELEASED"
$  THEN
$   GTCX$VMS_IS == F$ELEMENT(1,",",VMI$VMS_VERSION)
$   IF GTCX$VMS_IS .LTS. GTCX$VMS_VERSION
$    THEN
$     VMI$CALLBACK MESSAGE E VMSMISMATCH "This GT.CX kit requires an existing VMS''GTCX$VMS_VERSION' system."
$     EXIT VMI$_FAILURE
$   ENDIF
$  ELSE
$   GTCX$VMS_IS :==
$   WRITE SYS$OUTPUT "  No VMS version checking performed for field test versions."
$ ENDIF
$ IF (GTCX$VMS_IS .GES. "052") THEN T1 = F$VERIFY(VMI$KIT_DEBUG)
$ VMI$CALLBACK CHECK_NET_UTILIZATION GTCX$ROOM 'GTCX$DISK_SPACE'
$ IF .NOT. GTCX$ROOM
$  THEN
$   VMI$CALLBACK MESSAGE E NOSPACE "There is not enough disk space -- GT.CX needs ''GTCX$DISK_SPACE' blocks."
$   EXIT VMI$_FAILURE
$ ENDIF
$!  check for running users
$ SET NOON
$ DEFINE SYS$ERROR NL:
$ DEFINE SYS$OUTPUT NL:
$ @SYS$COMMON:[GTM_DIST]:GTMLOGIN
$ CCE :== $GTM$DIST:CCE
$ CCE SHO CCP
$ T1 = $SEVERITY
$ DEASSIGN SYS$OUTPUT
$ DEASSIGN SYS$ERROR
$ IF T1 .EQ. 1 THEN CCE SHO CCP
$ SET ON
$ DELETE /SYMBOL/GLOBAL CCE
$ VMI$CALLBACK ASK GTCX$READY "Have you verified there are no current GT.CX users" "NO" B "@VMI$KWD:GTCXKITHLP HELP_READY"
$ IF .NOT. GTCX$READY THEN EXIT VMI$_FAILURE
$!  setup default answers
$ GTCX$DOPURGE :== YES
$ GTCX$RUN_IVP == 0	!! should be "YES", but no IVP yet
$ GTCX$CDB_CNT == 12
$ GTCX$STD_CNF :== YES
$ GTCX$DST_OWN :== SYSTEM
$ IF F$IDENTIFIER(GTCX$DST_OWN,"NAME_TO_NUMBER") .EQ. 0 THEN GTCX$DST_OWN :== 1,4
$ GTCX$CCP_UIC == GTCX$DST_OWN
$ GTCX$SYS_DST :== YES
$ GTCX$DST_DIR :== GTM_DIST
$ GTCX$DST_CRE == GTCX$DST_DIR
$ GTCX$DST_DEV :==
$ GTCX$STARTDB :== YES
$ GTCX$MGR_COM :== YES
$ GTCX$HLP_DIR :== NO
$ GTCX$START_CCP :== YES
$!
$ VMI$CALLBACK ASK GTCX$DOPURGE "Do you want to purge files replaced by this installation" 'GTCX$DOPURGE' B -
  "@VMI$KWD:GTCXKITHLP HELP_PURGE"
$ IF .NOT. GTCX$DOPURGE THEN VMI$CALLBACK SET PURGE NO
$ VMI$CALLBACK ASK GTCX$CDB_CNT "How many clustered databases will this node access" 'GTCX$CDB_CNT' I -
  "@VMI$KWD:GTCXKITHLP HELP_CDB_CNT"
$ IF GTCX$CDB_CNT .LT. 1
$  THEN
$   GTCX$CDB_CNT == 1
$   WRITE SYS$OUTPUT "  The installation set this value to 1 as 0 or negative values are not useful."
$ ENDIF
$ VMI$CALLBACK ASK GTCX$STD_CNF "Do you want the standard GT.CX configuration" 'GTCX$STD_CNF' B -
  "@VMI$KWD:GTCXKITHLP HELP_STD_CNF"
$ IF GTCX$STD_CNF
$  THEN
$   GTCX$SYS_DST == 1
$   GTCX$STARTDB == 1
$   GTCX$MGR_COM == 1
$   GTCX$HLP_DIR == 0
$   GTCX$START_CCP == 1
$   GTCX$DST_LOG :== SYS$COMMON:['GTCX$DST_DIR']
$   GTCX$DIR_TYPE :== COMMON
$   GTCX$RUN_IVP == 0	!! "YES" no IVP yet
$  ELSE ! not standard configuration
$   VMI$CALLBACK ASK GTCX$DST_OWN "What UIC should own the GT.CX distribution" 'GTCX$DST_OWN' S "@VMI$KWD:GTCXKITHLP HELP_DST_OWN"
$   GTCX$DST_OWN == GTCX$DST_OWN - "[" - "]"
$   VMI$CALLBACK ASK GTCX$SYS_DST "Do you want the GT.CX distribution to go into a System Directory" 'GTCX$SYS_DST' B -
    "@VMI$KWD:GTCXKITHLP HELP_SYS_DST"
$   IF GTCX$SYS_DST
$    THEN
$     VMI$CALLBACK ASK GTCX$DST_DIR "In what System Directory do you want to place GT.CX" 'GTCX$DST_DIR' S -
      "@VMI$KWD:GTCXKITHLP HELP_SYS_DIR"
$     GTCX$DST_DIR == GTCX$DST_DIR - "[" - "]"
$     GTCX$DST_CRE == GTCX$DST_DIR
$     GTCX$DST_LOG :== SYS$COMMON:['GTCX$DST_DIR']
$     GTCX$DIR_TYPE :== COMMON
$    ELSE ! not system disk
$     VMI$CALLBACK ASK GTCX$DST_DEV "On which device do you want to place GT.CX" "''GTCX$DST_DEV'" S -
      "@VMI$KWD:GTCXKITHLP HELP_DST_DEV"
$     VMI$CALLBACK ASK GTCX$DST_DIR "In what directory on that device do you want to place GT.CX" 'GTCX$DST_DIR' S -
      "@VMI$KWD:GTCXKITHLP HELP_DST_DIR"
$     GTCX$DST_DEV == GTCX$DST_DEV - ":"
$     GTCX$DST_DIR == GTCX$DST_DIR - "[" - "]"
$     GTCX$DST_LOG :== 'GTCX$DST_DEV':['GTCX$DST_DIR']
$     GTCX$DST_CRE == GTCX$DST_LOG
$     GTCX$DIR_TYPE :== USER
$   ENDIF ! system disk
$   VMI$CALLBACK ASK GTCX$CCP_UIC "Under what UIC should the CCP operate (must be Group 1)" 'GTCX$CCP_UIC' S -
    "@VMI$KWD:GTCXKITHLP HELP_CCP_OWN"
$   GTCX$CCP_UIC == GTCX$CCP_UIC - "[" - "]"
$   IF F$ELEMENT(0,",",GTCX$CCP_UIC) .NE. 1
$    THEN
$     T1 = F$FAO("!%U",'F$IDENTIFIER(GTCX$CCP_UIC,"NAME_TO_NUMBER")')' -"["-"]"
$     IF F$ELEMENT(0,",",T1) .NE. 1
$      THEN
$       GTCX$CCP_UIC :== SYSTEM
$       IF F$IDENTIFIER(GTCX$CCP_UIC,"NAME_TO_NUMBER") .EQ. 0 THEN GTCX$CCP_UIC :== 1,4
$       WRITE SYS$OUTPUT "  The installation is using the default because the Group must be 1."
$     ENDIF
$   ENDIF
$   VMI$CALLBACK ASK GTCX$STARTDB "Do you want GTCXSTART.COM in the startup database" 'GTCX$STARTDB' B -
    "@VMI$KWD:GTCXKITHLP HELP_STARTDB"
$   IF .NOT. GTCX$STARTDB
$    THEN
$     VMI$CALLBACK ASK GTCX$MGR_COM "Do you want the GT.M .COM files in SYS$MANAGER" 'GTCX$MGR_COM' B -
      "@VMI$KWD:GTCXKITHLP HELP_MGR_COM"
$   ENDIF
$   VMI$CALLBACK ASK GTCX$HLP_DIR "Do you want the GT.CX help files in SYS$HELP" 'GTCX$HLP_DIR' B "@VMI$KWD:GTCXKITHLP HELP_HLP_DIR"
$!! no IVP yet
$   IF 0 THEN VMI$CALLBACK ASK GTCX$RUN_IVP "Do you want to run the IVP (requires GT.M)" 'GTCX$RUN_IVP' B -
    "@VMI$KWD:GTCXKITHLP HELP_RUN_IVP"
$   IF GTCX$RUN_IVP
$    THEN
$     GTCX$START_CCP == 1
$    ELSE
$     VMI$CALLBACK ASK GTCX$START_CCP "Do you want to start a GT.CX CCP now" 'GTCX$START_CCP' B "@VMI$KWD:GTCXKITHLP HELP_START_CCP"
$   ENDIF
$ ENDIF ! standard configuration
$ IF GTCX$MGR_COM
$  THEN
$   WRITE SYS$OUTPUT "  The following command files are created and copied to SYS$MANAGER:"
$  ELSE
$   WRITE SYS$OUTPUT "  The following command files are created:"
$ ENDIF
$ TYPE SYS$INPUT

	GTCXSTART.COM
	GTCXSTOP.COM

  Each file contains its own user documentation.

  All the questions have been asked. Installation now proceeds without your
  manual intervention for about 5-10 minutes.
$ IF GTCX$RUN_IVP THEN WRITE SYS$OUTPUT "  Finally the installation verification procedure tests the installation."
$ WRITE SYS$OUTPUT ""
$ VMI$CALLBACK CREATE_DIRECTORY 'GTCX$DIR_TYPE' 'GTCX$DST_CRE' "/OWNER_UIC=[''GTCX$DST_OWN'] /PROTECTION=(WO:RE)"
$ VMI$CALLBACK MESSAGE I CRECOM "Creating command files."
$!  Create GTCXSTART.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTCXSTART.COM
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTCXSTART.COM should be placed in the VMS startup database."
$ WRITE OUFILE "$!	It starts the GT.CX Cluster Controller Program (CCP) for a node."
$ WRITE OUFILE "$!	The invoking user requires the following privileges:"
$ WRITE OUFILE "$!	 ALTPRI, DETATCH, OPER, PSWAPM, SYSLCK, SYSNAM and TMPMBX"
$ WRITE OUFILE "$!	P1 is the number of clustered databases."
$ WRITE OUFILE "$!	P2 is the priority and should be at or just above the highest priority"
$ WRITE OUFILE "$!	 used by a any GT.M process which accesses clustered database files."
$ WRITE OUFILE "$!	P3 is the default working set size."
$ WRITE OUFILE "$!	P4 is the Page File Quota, which should be the sum for all"
$ WRITE OUFILE "$!       clustered databases of (GLOBAL_BUFFERS*BLOCKSIZE/512+LOCKSPACE+50),"
$ WRITE OUFILE "$!       and which this procedure approximates by defaulting to P1 * 10000."
$ WRITE OUFILE "$ CURPRV = F$SETPRV(""TMPMBX"")"
$ WRITE OUFILE "$ ON CONTROL_C THEN GOTO ERROR"
$ WRITE OUFILE "$ ON ERROR THEN GOTO ERROR"
$ WRITE OUFILE "$ CCE := $''GTCX$DST_LOG'CCE.EXE"
$ WRITE OUFILE "$ MUPI*P := $''GTCX$DST_LOG'MUPIP.EXE"
$ WRITE OUFILE "$ MUPIP RUNDOWN   ! Prepare a clean start"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(""ALTPRI,DETACH,OPER,PSWAPM,SYSLCK,SYSNAM,TMPMBX"") + - "
$ WRITE OUFILE "            "","" + CURPRV"
$ WRITE OUFILE "$ IF F$PRIVILEGE(""ALTPRI,DETACH,OPER,PSWAPM,SYSLCK,SYSNAM,TMPMBX"")"
$ WRITE OUFILE "$  THEN"
$ WRITE OUFILE "$   WRITE SYS$OUTPUT ""Starting the CCP as process GT.CX_CONTROL"""
$ WRITE OUFILE "$   IF P1 .EQS. """" THEN P1 = ''GTCX$CDB_CNT'"
$ WRITE OUFILE "$   FL = P1 + 3"
$ WRITE OUFILE "$   AL = FL * 4"
$ WRITE OUFILE "$   EL = FL * 5"
$ WRITE OUFILE "$   DL = FL * 10"
$ WRITE OUFILE "$   TQ = FL * 3"
$ WRITE OUFILE "$   IF P2 .EQS. """" THEN P2 = 5"
$ WRITE OUFILE "$   IF P3 .EQS. """" THEN P3 = FL * 200"
$ WRITE OUFILE "$   WSE = P3 + 200"
$ WRITE OUFILE "$   IF P4 .EQS. """" THEN P4 =  P1 * 10000"
$ WRITE OUFILE "$   RUN/DETACHED/PROC=""GT.CX_CONTROL""/PRIV=(OPER,SYSLCK,SYSNAM,TMPMBX) -"
$ WRITE OUFILE "     /ERROR=SYS$MANAGER:CCPERR.LOG /PAGE_FILE='P4' -"
$ WRITE OUFILE "     /DUMP/NOSWAPPING/UIC=''GTCX$CCP_UIC'/AST_LIMIT='AL'/ENQUEUE_LIMIT='EL' -"
$ WRITE OUFILE "     /FILE_LIMIT='FL'/IO_DIRECT='DL'/QUEUE_LIMIT='TQ'/PRIORITY='P2' -"
$ WRITE OUFILE "     /WORKING_SET='P3'/MAXIMUM_WORKING_SET='WSE' ''GTCX$DST_LOG'CCP.EXE"
$ WRITE OUFILE "$  ELSE"
$ WRITE OUFILE "$   WRITE SYS$OUTPUT ""NOT starting the CCP because of inadequate privileges"""
$ WRITE OUFILE "$ ENDIF"
$ WRITE OUFILE "$ERROR:"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(CURPRV)"
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$!  Create GTCXSTOP.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTCXSTOP.COM
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTCXSTOP.COM stops the GT.CX CCP for a node and does MUPIP RUNDOWN"
$ WRITE OUFILE "$!	Place an invocation or copy of this procedure in the site specific"
$ WRITE OUFILE "$!	 shutdown: SYS$MANAGER:SYSHUTDWN to ensure all GT.M databases are"
$ WRITE OUFILE "$!	 properly closed before VMS terminates.  GTCXSTOP should follow"
$ WRITE OUFILE "$!	 GTCMSTOP, if used and GTMSTOP, in any case."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$ CCE := $''GTCX$DST_LOG'CCE.EXE"
$ WRITE OUFILE "$ MUPIP := $''GTCX$DST_LOG'MUPIP.EXE"
$ WRITE OUFILE "$ CCE STOP"
$ WRITE OUFILE "$ MUPIP RUNDOWN"
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$ VMI$CALLBACK MESSAGE I PREINS "Preparing files for installation."
$!  GTCXFILES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTCXFILES.KIT
$ IF GTCX$MGR_COM
$  THEN
$   WRITE OUFILE "GTCX$ GTCXSTART.COM VMI$ROOT:[SYSMGR] C"
$   WRITE OUFILE "GTCX$ GTCXSTOP.COM VMI$ROOT:[SYSMGR] C"
$ ENDIF
$ WRITE OUFILE "GTCX$ GTCXSTART.COM ''GTCX$DST_LOG'"
$ WRITE OUFILE "GTCX$ GTCXSTOP.COM ''GTCX$DST_LOG'"
$ CLOSE OUFILE
$!  GTCXIMAGES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTCXIMAGES.KIT
$ WRITE OUFILE "GTCX$ CCE.EXE ''GTCX$DST_LOG'"
$ WRITE OUFILE "GTCX$ CCP.EXE ''GTCX$DST_LOG'"
$ CLOSE OUFILE
$!  Create GTCXHLB.KIT - don't know why this requires a separate file.
$ GTCX$HLP_LOG == GTCX$DST_LOG
$ IF GTCX$HLP_DIR THEN GTCX$HLP_LOG :== VMI$ROOT:[SYSHLP]
$ OPEN /WRITE OUFILE VMI$KWD:GTCXHLB.KIT
$ WRITE OUFILE "GTCX$ CCE.HLB ''GTCX$HLP_LOG'"
$ CLOSE OUFILE
$!  Provide with file.KITs
$ VMI$CALLBACK PROVIDE_FILE "" VMI$KWD:GTCXFILES.KIT "" T
$ VMI$CALLBACK PROVIDE_IMAGE "" VMI$KWD:GTCXIMAGES.KIT "" T
$ VMI$CALLBACK PROVIDE_FILE "" VMI$KWD:GTCXHLB.KIT "" T
$ VMI$CALLBACK MESSAGE I FININS "Finalizing the installation."
$ IF GTCX$START_CCP THEN VMI$CALLBACK SET POSTINSTALL YES
$ IF GTCX$RUN_IVP THEN VMI$CALLBACK SET IVP YES
$ IF GTCX$STARTDB THEN VMI$CALLBACK MODIFY_STARTUP_DB ADD GTCXSTART.COM END
$ EXIT VMI$_SUCCESS
$!
$POSTINSTALL:
$!
$! do a gtmlogin
$ @'GTCX$DST_LOG'GTMLOGIN
$ CCE := $GTM$DIST:CCE.EXE
$ SET NOON
$ DEFINE /USER_MODE SYS$ERROR NL:
$ DEFINE /USER_MODE SYS$OUTPUT NL:
$ CCE STOP
$ SET ON
$ IF GTCX$MGR_COM
$  THEN
$   T1 := SYS$MANAGER:
$  ELSE
$   T1 = GTM$DST_LOG
$ ENDIF
$ @'T1'GTCXSTART
$ EXIT VMI$_SUCCESS
$!
$IVP:
$!	The real Installation Verification Procedure.
$ TYPE SYS$INPUT

  GT.CX  Installation Verification Procedure

$!  Extract the IVP .COM file from the text library.
$ LIBRARIAN /EXTRACT=GTCX$IVP /OUTPUT=GTCX$IVP.COM GTCX$IVP.TLB
$ @GTCX$IVP
$ EXIT $STATUS
