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
$!	KITINSTAL.COM PROCEDURE FOR THE GT.CM PRODUCT
$!
$ ON CONTROL_Y THEN VMI$CALLBACK CONTROL_Y
$! ON WARNING THEN EXIT $STATUS		!! allow warning on install replace
$ IF P1 .EQS. "VMI$_INSTALL" THEN GOTO INSTALL
$ IF P1 .EQS. "VMI$_POSTINSTALL" THEN GOTO POSTINSTALL
$ IF P1 .EQS. "VMI$_IVP" THEN GOTO IVP
$ EXIT VMI$_UNSUPPORTED
$!
$INSTALL:
$ TYPE SYS$INPUT

  GT.CM  (c)  COPYRIGHT 1988, 2000  by  Sanchez Computer Associates, Inc.
                           ALL RIGHTS RESERVED

$!  the following 2 lines must be maintained
$ GTCM$VMS_VERSION :== 072	! Minimum VMS version required
$ GTCM$DISK_SPACE == 2000	! Minumum disk space on system disk required for install (2x result)
$ IF F$ELEMENT(0,",",VMI$VMS_VERSION) .EQS. "RELEASED"
$  THEN
$   GTCM$VMS_IS == F$ELEMENT(1,",",VMI$VMS_VERSION)
$   IF GTCM$VMS_IS .LTS. GTCM$VMS_VERSION
$    THEN
$     VMI$CALLBACK MESSAGE E VMSMISMATCH "This GT.CM kit requires an existing VMS''GTCM$VMS_VERSION' system."
$     EXIT VMI$_FAILURE
$   ENDIF
$  ELSE
$   GTCM$VMS_IS :==
$   WRITE SYS$OUTPUT "  No VMS version checking performed for field test versions."
$ ENDIF
$ IF (GTCM$VMS_IS .GES. "052") THEN T1 = F$VERIFY(VMI$KIT_DEBUG)
$ VMI$CALLBACK CHECK_NET_UTILIZATION GTCM$ROOM 'GTCM$DISK_SPACE'
$ IF .NOT. GTCM$ROOM
$  THEN
$   VMI$CALLBACK MESSAGE E NOSPACE "There is not enough disk space -- GT.CM needs ''GTCM$DISK_SPACE' blocks."
$   EXIT VMI$_FAILURE
$ ENDIF
$!  setup default answers
$ GTCM$DOPURGE :== YES
$ GTCM$RUN_IVP == 0	!! should be "YES", but no IVP yet
$ GTCM$NDB_CNT == 12
$ GTCM$RC_CNT == 30
$ GTCM$STD_CNF :== YES
$ GTCM$DST_OWN :== SYSTEM
$ IF F$IDENTIFIER(GTCM$DST_OWN,"NAME_TO_NUMBER") .EQ. 0 THEN GTCM$DST_OWN :== 1,4
$ GTCM$SRV_UIC :==
$ GTCM$SYS_DST :== YES
$ GTCM$DST_DIR :== GTM_DIST
$ GTCM$DST_CRE == GTCM$DST_DIR
$ GTCM$DST_DEV :==
$ GTCM$STD_SRV :== YES
$ GTCM$AUTO_SRV :== YES
$ GTCM$TIMEOUT == 0
$ GTCM$STARTDB :== YES
$ GTCM$MGR_COM :== YES
$ GTCM$START_SRV :== YES
$!
$ VMI$CALLBACK ASK GTCM$DOPURGE "Do you want to purge files replaced by this installation" 'GTCM$DOPURGE' B -
  "@VMI$KWD:GTCMKITHLP HELP_PURGE"
$ IF .NOT. GTCM$DOPURGE THEN VMI$CALLBACK SET PURGE NO
$ VMI$CALLBACK ASK GTCM$NDB_CNT "How many networked databases will this node serve" 'GTCM$NDB_CNT' I -
  "@VMI$KWD:GTCMKITHLP HELP_NDB_CNT"
$ IF GTCM$NDB_CNT .LT. 1
$  THEN
$   GTCM$NDB_CNT == 1
$   WRITE SYS$OUTPUT "  The installation set this value to 1 as 0 or negative values are not useful."
$ ENDIF
$ VMI$CALLBACK ASK GTCM$RC_CNT "How many client links will this node serve" 'GTCM$RC_CNT' I -
  "@VMI$KWD:GTCMKITHLP HELP_RC_CNT"
$ IF GTCM$RC_CNT .LT. 1
$  THEN
$   GTCM$RC_CNT == 1
$   WRITE SYS$OUTPUT "  The installation set this value to 1 as 0 or negative values are not useful."
$ ENDIF
$ VMI$CALLBACK ASK GTCM$SRV_UIC "Under what UIC should the Server operate" "''GTCM$SRV_UIC'" S -
  "@VMI$KWD:GTCMKITHLP HELP_SRV_UIC"
$ GTCM$SRV_UIC == GTCM$SRV_UIC - "[" - "]"
$ IF GTCM$SRV_UIC - "," .NES. GTCM$SRV_UIC THEN GTCM$SRV_UIC :== ['GTCM$SRV_UIC']
$ VMI$CALLBACK ASK GTCM$STD_CNF "Do you want the standard GT.CM configuration" 'GTCM$STD_CNF' B -
  "@VMI$KWD:GTCMKITHLP HELP_STD_CNF"
$ IF GTCM$STD_CNF
$  THEN
$   GTCM$SYS_DST == 1
$   GTCM$STD_SRV :== 1
$   GTCM$AUTO_SRV :== 1
$   GTCM$STARTDB == 1
$   GTCM$MGR_COM == 1
$   GTCM$START_CM == 1
$   GTCM$DST_LOG :== SYS$COMMON:['GTCM$DST_DIR']
$   GTCM$DIR_TYPE :== COMMON
$   GTCM$RUN_IVP == 0	!! no IVP yet
$  ELSE ! not standard configuration
$   VMI$CALLBACK ASK GTCM$DST_OWN "What UIC should own the GT.CM distribution" 'GTCM$DST_OWN' S "@VMI$KWD:GTCMKITHLP HELP_DST_OWN"
$   GTCM$DST_OWN == GTCM$DST_OWN - "[" - "]"
$   VMI$CALLBACK ASK GTCM$SYS_DST "Do you want the GT.CM distribution to go into a System Directory" 'GTCM$SYS_DST' B -
    "@VMI$KWD:GTCMKITHLP HELP_SYS_DST"
$   IF GTCM$SYS_DST
$    THEN
$     VMI$CALLBACK ASK GTCM$DST_DIR "In what System Directory do you want to place GT.CM" 'GTCM$DST_DIR' S -
      "@VMI$KWD:GTCMKITHLP HELP_SYS_DIR"
$     GTCM$DST_DIR == GTCM$DST_DIR - "[" - "]"
$     GTCM$DST_CRE == GTCM$DST_DIR
$     GTCM$DST_LOG :== SYS$COMMON:['GTCM$DST_DIR']
$     GTCM$DIR_TYPE :== COMMON
$    ELSE ! not system disk
$     VMI$CALLBACK ASK GTCM$DST_DEV "On which device do you want to place GT.CM" "''GTCM$DST_DEV'" S -
      "@VMI$KWD:GTCMKITHLP HELP_DST_DEV"
$     VMI$CALLBACK ASK GTCM$DST_DIR "In what directory on that device do you want to place GT.CM" 'GTCM$DST_DIR' S -
      "@VMI$KWD:GTCMKITHLP HELP_DST_DIR"
$     GTCM$DST_DEV == GTCM$DST_DEV - ":"
$     GTCM$DST_DIR == GTCM$DST_DIR - "[" - "]"
$     GTCM$DST_LOG :== 'GTCM$DST_DEV':['GTCM$DST_DIR']
$     GTCM$DST_CRE == GTCM$DST_LOG
$     GTCM$DIR_TYPE :== USER
$   ENDIF ! system disk
$   VMI$CALLBACK ASK GTCM$STD_SRV "Do you want GTCMSTART.COM to start the GT.CM Server" 'GTCM$STD_SRV' S -
    "@VMI$KWD:GTCMKITHLP HELP_STD_SRV"
$   VMI$CALLBACK ASK GTCM$AUTO_SRV "Do you want network requests to automatically start the GT.CM server" 'GTCM$AUTO_SRV S -
    "@VMI$KWD:GTCMKITHLP HELP_AUTO_SRV"
$   IF GTCM$AUTO_SRV
$    THEN
$     VMI$CALLBACK ASK GTCM$TIMEOUT "How many quiet minutes before the server terminates (0=>never)" 'GTCM$TIMEOUT' I -
      "@VMI$KWD:GTCMKITHLP HELP_TIMEOUT"
$     IF GTCM$TIMEOUT .LT. 0
$      THEN
$       GTCM$TIMEOUT == 0
$       WRITE SYS$OUTPUT "  The installation set this value to 0 as negative values are not useful."
$     ENDIF
$   ENDIF
$   VMI$CALLBACK ASK GTCM$STARTDB "Do you want GTCMSTART.COM in the startup database" 'GTCM$STARTDB' B -
    "@VMI$KWD:GTCMKITHLP HELP_STARTDB"
$   IF .NOT. GTCM$STARTDB
$    THEN
$     VMI$CALLBACK ASK GTCM$MGR_COM "Do you want the GT.M .COM files in SYS$MANAGER" 'GTCM$MGR_COM' B -
      "@VMI$KWD:GTCMKITHLP HELP_MGR_COM"
$   ENDIF
$!! no IVP yet
$   IF 0 THEN VMI$CALLBACK ASK GTCM$RUN_IVP "Do you want to run the IVP (requires GT.M)" 'GTCM$RUN_IVP' B -
    "@VMI$KWD:GTCMKITHLP HELP_RUN_IVP"
$   IF GTCM$RUN_IVP
$    THEN
$     GTCM$START_CCP == 1
$    ELSE
$     VMI$CALLBACK ASK GTCM$START_CM "Do you want to start GT.CM now" 'GTCM$START_SRV' B "@VMI$KWD:GTCMKITHLP HELP_START_CM"
$   ENDIF
$ ENDIF ! standard configuration
$ IF GTCM$MGR_COM
$  THEN
$   WRITE SYS$OUTPUT "  The following command files are created, except for GTCMAUTOSVR,"
$   WRITE SYS$OUTPUT "and copied to SYS$MANAGER:"
$  ELSE
$   WRITE SYS$OUTPUT "  The following command files are created:"
$ ENDIF
$ TYPE SYS$INPUT

        GTCMAUTOSVR.COM
        GTCMSERVER.COM
	GTCMSTART.COM
	GTCMSTOP.COM

  Each file contains its own user documentation.

  All the questions have been asked. Installation now proceeds without your
  manual intervention for about 5-10 minutes.
$ IF GTCM$RUN_IVP THEN WRITE SYS$OUTPUT "  Finally the installation verification procedure tests the installation."
$ WRITE SYS$OUTPUT ""
$ VMI$CALLBACK CREATE_DIRECTORY 'GTCM$DIR_TYPE' 'GTCM$DST_CRE' "/OWNER_UIC=[''GTCM$DST_OWN'] /PROTECTION=(WO:RE)"
$ VMI$CALLBACK MESSAGE I CRECOM "Creating command files."
$!  Create GTCMAUTOSVR.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTCMAUTOSVR.COM
$ WRITE OUFILE "$!	GTCMAUTOSVR.COM acts as a network object"
$ WRITE OUFILE "$!	 for auto-starting a GT.CM Server with an optional timeout."
$ WRITE OUFILE "$!      P1 is the timeout"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$ IF P1 .EQS. """" THEN P1 = ''GTCM$TIMEOUT'"
$ WRITE OUFILE "$ SERVER := $''GTCM$DST_LOG'GTCM_SERVER"
$ WRITE OUFILE "$ SERVER 'P1'"
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$!  Create GTCMSERVER.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTCMSERVER.COM
$ WRITE OUFILE "$!	GTCMSERVER.COM starts the GT.CM server with no timeout."
$ WRITE OUFILE "$!	The invoking user requires the following privileges:"
$ WRITE OUFILE "$!	 DETATCH, NETMBX, PSWAPM, SYSNAM and TMPMBX"
$ WRITE OUFILE "$!	P1 is the maximum number of databases served to the network."
$ WRITE OUFILE "$!	P2 is the maximum number of remote clients served."
$ WRITE OUFILE "$!	P3 is the default working set size."
$ WRITE OUFILE "$!	P4 is the priority and should be at an appropriate priority"
$ WRITE OUFILE "$!	 to balance network response with local load"
$ WRITE OUFILE "$!	P5 is the pagefile quota and should be large enough to"
$ WRITE OUFILE "$!	 accommodate the global buffers for all BG database files."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(""TMPMBX"")"
$ WRITE OUFILE "$ ON CONTROL_C THEN GOTO ERROR"
$ WRITE OUFILE "$ ON ERROR THEN GOTO ERROR"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(""DETACH,NETMBX,PSWAPM,SYSNAM,TMPMBX"") + "","" + CURPRV"
$ WRITE OUFILE "$ IF F$PRIVILEGE(""DETACH,NETMBX,PSWAPM,SYSNAM,TMPMBX"")"
$ WRITE OUFILE "$  THEN"
$ WRITE OUFILE "$   IF P6 .EQS. """""
$ WRITE OUFILE "$    THEN"
$ WRITE OUFILE "$      P6 := GTCM_SERVER"
$ WRITE OUFILE "$      IF F$TRNLNM(""GTCMSVRNAM"",""LNM$SYSTEM_TABLE"") .NES. """""
$ WRITE OUFILE "$       THEN"
$ WRITE OUFILE "$         DEASSIGN /SYSTEM GTCMSVRNAM"
$ WRITE OUFILE "$      ENDIF"
$ WRITE OUFILE "$    ELSE"
$ WRITE OUFILE "$      DEFINE /SYSTEM GTCMSVRNAM 'P6'"
$ WRITE OUFILE "$   ENDIF"
$ WRITE OUFILE "$   WRITE SYS$OUTPUT ""Starting the GT.CM Server as process "" + P6"
$ WRITE OUFILE "$   IF P1 .EQS. """" THEN P1 = ''GTCM$NDB_CNT'"
$ WRITE OUFILE "$   IF P2 .EQS. """" THEN P2 = ''GTCM$RC_CNT'"
$ WRITE OUFILE "$   AL = ((P1 + 1) * 3) + P2"
$ WRITE OUFILE "$   BL = (P2 + 3) * 1024"
$ WRITE OUFILE "$   EL = (P1 + 1) * 3"
$ WRITE OUFILE "$   FL = P1 + 3 + P2"
$ WRITE OUFILE "$   DL = P1 * 7"
$ WRITE OUFILE "$   TQ = (P1 + 1) * 2"
$ WRITE OUFILE "$   IF P3 .EQS. """" THEN P3 = FL * 200"
$ WRITE OUFILE "$   WSE = P3 + 200"
$ WRITE OUFILE "$   IF P4 .EQS. """" THEN P4 = 5"
$ WRITE OUFILE "$   IF P5 .EQS. """" THEN P5 = P1 * 10000"
$ WRITE OUFILE "$   RUN/DETACHED /PROCESS='P6' /PRIV=(SYSNAM) /ERROR=SYS$MANAGER:CME.LOG -"
$ WRITE OUFILE "     /DUMP /NOSWAPPING /UIC=''GTCM$SRV_UIC' /AST_LIMIT='AL' /ENQUEUE_LIMIT='EL' -"
$ WRITE OUFILE "     /FILE_LIMIT='FL'/IO_BUFFERED='BL' /IO_DIRECT='DL' /QUEUE_LIMIT='TQ' -"
$ WRITE OUFILE "     /PRIORITY='P4' /WORKING_SET='P3' /MAXIMUM_WORKING_SET='WSE' -"
$ WRITE OUFILE "     /PAGE_FILE='P5' ''GTCM$DST_LOG'GTCM_SERVER"
$ WRITE OUFILE "$  ELSE"
$ WRITE OUFILE "$   WRITE SYS$OUTPUT ""NOT starting GT.CM Server because of inadequate privileges"""
$ WRITE OUFILE "$ ENDIF"
$ WRITE OUFILE "$ERROR:"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(CURPRV)"
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$!  Create GTCMSTART.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTCMSTART.COM
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTCMSTART.COM should be placed in the VMS startup database."
$ WRITE OUFILE "$!	P1, P2, P3, P4 and P5 are passed on to GTCMSERVER."
$ WRITE OUFILE "$!	It defines the CMISHR logical name, installs GT.CM images,"
$ WRITE OUFILE "$!	 optionally sets up the autostart network object,"
$ WRITE OUFILE "$!	 and optionally starts the GT.CM Server for a node."
$ WRITE OUFILE "$!	It defines the GTM$DIST and GTMSECSHR logical names,"
$ WRITE OUFILE "$!	 to ensure they are /SYSTEM /EXEC, which doesn't work"
$ WRITE OUFILE "$!	 if they exist /SYSTEM /SUPER."
$ WRITE OUFILE "$!	 In that case modify the GT.M .COM files or invocation"
$ WRITE OUFILE "$!	 or add DEASSIGNs in this file before the DEFINEs."
$ WRITE OUFILE "$!       If GT.M and GT.CM are stored in different places,"
$ WRITE OUFILE "$!       delete these DEFINEs and make sure the GT.M .COM files are OK."
$ WRITE OUFILE "$!	The invoking user requires the following privileges:"
$ WRITE OUFILE "$!	 CMKRNL, DETATCH, OPER, NETMBX, PSWAPM, SYSNAM and TMPMBX"
$ WRITE OUFILE "$!      GTCM_SERVER is installed with privilege SYSNAM."
$ WRITE OUFILE "$!      CMISHR is the network interface and is installed for performance."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(""TMPMBX"")"
$ WRITE OUFILE "$ ON CONTROL_C THEN GOTO ERROR"
$ WRITE OUFILE "$ ON ERROR THEN GOTO ERROR"
$ WRITE OUFILE "$ MUPI*P := $''GTCM$DST_LOG'MUPIP.EXE"
$ WRITE OUFILE "$ MUPIP RUNDOWN   ! Prepare a clean start"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(""CMKRNL,OPER,NETMBX,SYSNAM"") + "","" + CURPRV"
$ WRITE OUFILE "$ IF F$PRIVILEGE(""CMKRNL,OPER,NETMBX,SYSNAM"")"
$ WRITE OUFILE "$  THEN"
$ WRITE OUFILE "$   DEFINE /SYSTEM/EXEC CMISHR ''GTCM$DST_LOG'CMISHR.EXE"
$ WRITE OUFILE "$   DEFINE /SYSTEM/EXEC GTM$DIST ''GTCM$DST_LOG'"
$ WRITE OUFILE "$   DEFINE /SYSTEM/EXEC GTMSECSHR ''GTCM$DST_LOG'GTMSECSHR.EXE"
$ WRITE OUFILE "$   INSTALL"
$ WRITE OUFILE "    REPLACE  /OPEN/HEADER/PRIV=SYSNAM	''GTCM$DST_LOG'GTCM_SERVER"
$ WRITE OUFILE "    REPLACE  /OPEN/SHARED/HEADER		CMISHR"
$ T1 = "!"
$ IF GTCM$AUTO_SRV THEN T1 :=
$ WRITE OUFILE "$''T1'   NCP :== $SYS$SYSTEM:NCP"
$ WRITE OUFILE "$''T1'   NCP SET OBJECT GTCMSVR NUMBER 0 FILE ''GTCM$DST_LOG'GTCMAUTOSVR.COM PROXY INCOMING USER ''GTCM$SRV_UIC'"
$ T1 = "!"
$ IF GTCM$STD_SRV THEN T1 :=
$ WRITE OUFILE "$''T1'   @''GTCM$DST_LOG'GTCMSERVER """"'P1' """"'P2' """"'P3' """"'P4' """"'P5' 'P6'"
$ WRITE OUFILE "$  ELSE"
$ WRITE OUFILE "$   WRITE SYS$OUTPUT ""NOT starting GT.CM because of inadequate privileges"""
$ WRITE OUFILE "$ ENDIF"
$ WRITE OUFILE "$ERROR:"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(CURPRV)"
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$!  Create GTCMSTOP.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTCMSTOP.COM
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTCMSTOP.COM stops the GT.CM Server for a node and does MUPIP RUNDOWN"
$ WRITE OUFILE "$!	Place an invocation or copy of this procedure in the site specific"
$ WRITE OUFILE "$!	 shutdown: SYS$MANAGER:SYSHUTDWN to ensure all GT.M databases are"
$ WRITE OUFILE "$!	 properly closed before VMS terminates.  GTCMSTOP should precede"
$ WRITE OUFILE "$!	 GTCXSTOP, if used, and GTMSTOP, in any case."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$ MUPIP := $''GTCM$DST_LOG'MUPIP.EXE"
$ WRITE OUFILE "$ RUN ''GTCM$DST_LOG'GTCM_STOP.EXE"
$ WRITE OUFILE "$ MUPIP RUNDOWN"
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$ VMI$CALLBACK MESSAGE I PREINS "Preparing files for installation."
$!  GTCMFILES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTCMFILES.KIT
$ IF GTCM$MGR_COM
$  THEN
$   WRITE OUFILE "GTCM$ GTCMSERVER.COM VMI$ROOT:[SYSMGR] C"
$   WRITE OUFILE "GTCM$ GTCMSTART.COM VMI$ROOT:[SYSMGR] C"
$   WRITE OUFILE "GTCM$ GTCMSTOP.COM VMI$ROOT:[SYSMGR] C"
$ ENDIF
$ WRITE OUFILE "GTCM$ GTCMAUTOSVR.COM ''GTCM$DST_LOG'"
$ WRITE OUFILE "GTCM$ GTCMSERVER.COM ''GTCM$DST_LOG'"
$ WRITE OUFILE "GTCM$ GTCMSTART.COM ''GTCM$DST_LOG'"
$ WRITE OUFILE "GTCM$ GTCMSTOP.COM ''GTCM$DST_LOG'"
$ CLOSE OUFILE
$!  GTCMIMAGES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTCMIMAGES.KIT
$ WRITE OUFILE "GTCM$ CMISHR.EXE ''GTCM$DST_LOG'"
$ WRITE OUFILE "GTCM$ GTCM_SERVER.EXE ''GTCM$DST_LOG'"
$ WRITE OUFILE "GTCM$ GTCM_STOP.EXE ''GTCM$DST_LOG'"
$ CLOSE OUFILE
$!  Provide with file.KITs
$ VMI$CALLBACK PROVIDE_FILE "" VMI$KWD:GTCMFILES.KIT "" T
$ VMI$CALLBACK PROVIDE_IMAGE "" VMI$KWD:GTCMIMAGES.KIT "" T
$ VMI$CALLBACK MESSAGE I FININS "Finalizing the installation."
$ IF GTCM$START_CM THEN VMI$CALLBACK SET POSTINSTALL YES
$ IF GTCM$RUN_IVP THEN VMI$CALLBACK SET IVP YES
$ IF GTCM$STARTDB THEN VMI$CALLBACK MODIFY_STARTUP_DB ADD GTCMSTART.COM END
$ EXIT VMI$_SUCCESS
$!
$POSTINSTALL:
$!
$ @'GTCM$DST_LOG'GTMLOGIN
$ SET NOON
$ DEFINE /USER_MODE SYS$ERROR NL:
$ DEFINE /USER_MODE SYS$OUTPUT NL:
$ IF GTCM$MGR_COM
$  THEN
$   T1 := SYS$MANAGER:
$  ELSE
$   T1 = GTCM$DST_LOG
$ ENDIF
$ @'T1'GTCMSTOP
$ SET ON
$ @'T1'GTCMSTART
$ EXIT VMI$_SUCCESS
$!
$IVP:
$!	The real Installation Verification Procedure.
$ TYPE SYS$INPUT

  GT.CM  Installation Verification Procedure

$!  Extract the IVP .COM file from the text library.
$ LIBRARIAN /EXTRACT=GTCM$IVP /OUTPUT=GTCM$IVP.COM GTCM$IVP.TLB
$ @GTCM$IVP
$ EXIT $STATUS
