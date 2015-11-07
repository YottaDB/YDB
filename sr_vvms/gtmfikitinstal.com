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
$!	KITINSTAL.COM PROCEDURE FOR THE GT.M FOCUS INTERFACE
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

GT.M FOCUS INTERFACE  (c)  COPYRIGHT 1991-2000  by  Sanchez Computer Associates
                           ALL RIGHTS RESERVED

$!  the following 2 lines must be maintained
$ GTMFI$VMS_VERSION :== 072	! Minimum VMS version required
$ GTMFI$DISK_SPACE == 400	! Minumum disk space on system disk required for install (2x result)
$ IF F$ELEMENT(0,",",VMI$VMS_VERSION) .EQS. "RELEASED"
$  THEN
$   GTMFI$VMS_IS == F$ELEMENT(1,",",VMI$VMS_VERSION)
$   IF GTMFI$VMS_IS .LTS. GTMFI$VMS_VERSION
$    THEN
$     VMI$CALLBACK MESSAGE E VMSMISMATCH "This GT.M FOCUS interface kit requires an existing VMS''GTMFI$VMS_VERSION' system."
$     EXIT VMI$_FAILURE
$   ENDIF
$  ELSE
$   GTMFI$VMS_IS :==
$   WRITE SYS$OUTPUT "  No VMS version checking performed for field test versions."
$ ENDIF
$ IF (GTMFI$VMS_IS .GES. "052") THEN T1 = F$VERIFY(VMI$KIT_DEBUG)
$ VMI$CALLBACK CHECK_NET_UTILIZATION GTMFI$ROOM 'GTMFI$DISK_SPACE'
$ IF .NOT. GTMFI$ROOM
$  THEN
$   VMI$CALLBACK MESSAGE E NOSPACE "There is not enough disk space -- GT.M FOCUS interface needs ''GTMFI$DISK_SPACE' blocks."
$   EXIT VMI$_FAILURE
$ ENDIF
$!  setup default answers
$ GTMFI$DOPURGE :== YES
$ GTMFI$RUN_IVP == 0	!! should be "YES", but no IVP yet
$ GTMFI$STD_CNF :== YES
$ GTMFI$DST_OWN :== SYSTEM
$ IF F$IDENTIFIER(GTMFI$DST_OWN,"NAME_TO_NUMBER") .EQ. 0 THEN GTMFI$DST_OWN :== 1,4
$ GTMFI$SYS_DST :== YES
$ GTMFI$DST_DIR :== GTM_DIST
$ GTMFI$DST_CRE == GTMFI$DST_DIR
$ GTMFI$DST_DEV :==
$ GTMFI$STARTDB :== YES
$ GTMFI$MGR_COM :== YES
$ GTMFI$START_GTMFI :== YES
$ GTMFI$UCI_NAME :== FCS
$!
$ VMI$CALLBACK ASK GTMFI$DOPURGE "Do you want to purge files replaced by this installation" 'GTMFI$DOPURGE' B -
  "@VMI$KWD:GTMFIKITHLP HELP_PURGE"
$ IF .NOT. GTMFI$DOPURGE THEN VMI$CALLBACK SET PURGE NO
$ VMI$CALLBACK ASK GTMFI$STD_CNF "Do you want the standard GT.M FOCUS interface configuration" 'GTMFI$STD_CNF' B -
  "@VMI$KWD:GTMFIKITHLP HELP_STD_CNF"
$ IF GTMFI$STD_CNF
$  THEN
$   GTMFI$SYS_DST == 1
$   GTMFI$STARTDB == 1
$   GTMFI$MGR_COM == 1
$   GTMFI$START_GTMFI == 1
$   GTMFI$DST_LOG :== SYS$COMMON:['GTMFI$DST_DIR']
$   GTMFI$DIR_TYPE :== SYSTEM
$   GTMFI$GBLDIR == GTMFI$DST_LOG + "FOCUS.GLD"
$   GTMFI$RUN_IVP == 0	!! no IVP yet
$  ELSE ! not standard configuration
$   VMI$CALLBACK ASK GTMFI$DST_OWN "What UIC should own the GT.M FOCUS interface distribution" 'GTMFI$DST_OWN' S -
    "@VMI$KWD:GTMFIKITHLP HELP_DST_OWN"
$   GTMFI$DST_OWN == GTMFI$DST_OWN - "[" - "]"
$   VMI$CALLBACK ASK GTMFI$SYS_DST "Do you want the GT.M FOCUS interface distribution to go into a System Directory" -
    'GTMFI$SYS_DST' B "@VMI$KWD:GTMFIKITHLP HELP_SYS_DST"
$   IF GTMFI$SYS_DST
$    THEN
$     VMI$CALLBACK ASK GTMFI$DST_DIR "In what System Directory do you want to place GT.M FOCUS interface" 'GTMFI$DST_DIR' S -
      "@VMI$KWD:GTMFIKITHLP HELP_SYS_DIR"
$     GTMFI$DST_DIR == GTMFI$DST_DIR - "[" - "]"
$     GTMFI$DST_CRE == GTMFI$DST_DIR
$     GTMFI$DST_LOG :== SYS$COMMON:['GTMFI$DST_DIR']
$     GTMFI$DIR_TYPE :== SYSTEM
$     GTMFI$GBLDIR == GTMFI$DST_LOG + "FOCUS.GLD"
$    ELSE ! not system disk
$     VMI$CALLBACK ASK GTMFI$DST_DEV "On which device do you want to place the GT.M FOCUS interface" "''GTMFI$DST_DEV'" S -
      "@VMI$KWD:GTMFIKITHLP HELP_DST_DEV"
$     VMI$CALLBACK ASK GTMFI$DST_DIR "In what directory on that device do you want to place the GT.M FOCUS interface" -
      'GTMFI$DST_DIR' S "@VMI$KWD:GTMFIKITHLP HELP_DST_DIR"
$     GTMFI$DST_DEV == GTMFI$DST_DEV - ":"
$     GTMFI$DST_DIR == GTMFI$DST_DIR - "[" - "]"
$     GTMFI$DST_LOG :== 'GTMFI$DST_DEV':['GTMFI$DST_DIR']
$     GTMFI$DST_CRE == GTMFI$DST_LOG
$     GTMFI$DIR_TYPE :== USER
$     GTMFI$GBLDIR == GTMFI$DST_LOG + "FOCUS.GLD"
$   ENDIF ! system disk
$   VMI$CALLBACK ASK GTMFI$LM_FILE "What do you want to call the configuration database file" 'GTMFI$LM_FILE' S -
    "@VMI$KWD:GTMFIKITHLP HELP_LM_FILE"
$   VMI$CALLBACK ASK GTMFI$GBLDIR "What file do you want as the global directory for the GT.M FOCUS interface" 'GTMFI$GBLDIR' S -
    "@VMI$KWD:GTMFIKITHLP HELP_GBLDIR"
$   VMI$CALLBACK ASK GTMFI$UCI_NAME "What ""UCI"" do you want to assign to the GT.M FOCUS interface" 'GTMFI$UCI_NAME' S -
    "@VMI$KWD:GTMFIKITHLP HELP_UCI_NAME"
$   VMI$CALLBACK ASK GTMFI$STARTDB "Do you want GTMFISTART.COM in the startup database" 'GTMFI$STARTDB' B -
    "@VMI$KWD:GTMFIKITHLP HELP_STARTDB"
$   IF .NOT. GTMFI$STARTDB
$    THEN
$     VMI$CALLBACK ASK GTMFI$MGR_COM "Do you want the GTMFISTART.COM file in SYS$MANAGER" 'GTMFI$MGR_COM' B -
      "@VMI$KWD:GTMFIKITHLP HELP_MGR_COM"
$   ENDIF
$!! no IVP yet
$   IF FALSE THEN VMI$CALLBACK ASK GTMFI$RUN_IVP "Do you want to run the IVP (requires GT.M)" 'GTMFI$RUN_IVP' B -
    "@VMI$KWD:GTMFIKITHLP HELP_RUN_IVP"
$   IF GTMFI$RUN_IVP
$    THEN
$     GTMFI$START_GTMFI == 1
$    ELSE
$     VMI$CALLBACK ASK GTMFI$START_GTMFI "Do you want to set up the GT.M FOCUS interface now" 'GTMFI$START_GTMFI' B -
      "@VMI$KWD:GTMFIKITHLP HELP_START_GTMFI"
$   ENDIF
$ ENDIF ! standard configuration
$ IF GTMFI$MGR_COM
$  THEN
$   WRITE SYS$OUTPUT "  The following command file is created and copied to SYS$MANAGER:"
$  ELSE
$   WRITE SYS$OUTPUT "  The following command file is created:"
$ ENDIF
$ TYPE SYS$INPUT

        GTMFISTART.COM

  The file contains its own user documentation.

  All the questions have been asked. Installation now proceeds without your
  manual intervention for about 5-10 minutes.
$ IF GTMFI$RUN_IVP THEN WRITE SYS$OUTPUT "  Finally the installation verification procedure tests the installation."
$ WRITE SYS$OUTPUT ""
$!! When VMS 5.2 is required, the following line should be removed in favor of changing all GTMFI$DIR_TYPE :== SYSTEM to :== COMMON
$ IF (GTMFI$VMS_IS .GES. "052") .AND. (GTMFI$DIR_TYPE .EQS. "SYSTEM") THEN GTMFI$DIR_TYPE :== COMMON
$ VMI$CALLBACK CREATE_DIRECTORY 'GTMFI$DIR_TYPE' 'GTMFI$DST_CRE' "/OWNER_UIC=[''GTMFI$DST_OWN'] /PROTECTION=(WO:RE)"
$ VMI$CALLBACK MESSAGE I CRECOM "Creating command files."
$!  Create GTMFISTART.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTMFISTART.COM
$ WRITE OUFILE "$!	GTMFISTART.COM provides GT.M FOCUS interface for a node."
$ WRITE OUFILE "$!	It defines the IBI_XFM logical name, and the logical names"
$ WRITE OUFILE "$!	 relating UCIs and Volumesets to global directories the interface."
$ WRITE OUFILE "$!	The invoking user requires the SYSNAM privilege."
$ WRITE OUFILE "$!	P1 is a list of volume-set, UCI, file-specification triples, where "
$ WRITE OUFILE "$!       the 3 parts are separated by asterisks (*) and the list is separated "
$ WRITE OUFILE "$!       by commas.  The volume-set and UCI must be 3 character names, and "
$ WRITE OUFILE "$!       the file-specification may be a logical name.  This defaults to a "
$ WRITE OUFILE "$!       single triple consisting of the 1st 3 characters of the node name,"
$ WRITE OUFILE "$!       the literal FCS and GTM$DIST:FOCUS.GLD.  e.g., if P1 is"
$ WRITE OUFILE "$!       null for node MIKE - MIK*FCS*GTM$DIST:FOCUS.GLD."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(""TMPMBX"")"
$ WRITE OUFILE "$ ON CONTROL_C THEN GOTO ERROR"
$ WRITE OUFILE "$ ON ERROR THEN GOTO ERROR"
$ WRITE OUFILE "$ CURPRV=F$SETPRV(""SYSNAM"")+"",""+CURPRV"
$ WRITE OUFILE "$ IF F$PRIVILEGE(""SYSNAM"")"
$ WRITE OUFILE "$  THEN"
$ WRITE OUFILE "$   DEFINE /SYSTEM IBI_XFM ''GTMFI$DST_LOG'IBI_XFM.EXE"
$ WRITE OUFILE "$   P1 = F$EDIT(P1,""COLLAPSE"")"
$ WRITE OUFILE "$   IF P1 .EQS. """" "
$ WRITE OUFILE "$    THEN"
$ WRITE OUFILE "$     P1=F$EXTRACT(0,3,F$EDIT(F$GETSYI(""SCSNODE""),""TRIM"") + ""___"") + ""*''GTMFI$UCI_NAME'*''GTMFI$GBLDIR'"""
$ WRITE OUFILE "$   ENDIF"
$ WRITE OUFILE "$   N = 0"
$ WRITE OUFILE "$   GOSUB UCI"
$ WRITE OUFILE "$ ENDIF"
$ WRITE OUFILE "$ERROR:"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(CURPRV)"
$ WRITE OUFILE "$ EXIT"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$UCI:"
$ WRITE OUFILE "$ T1 = F$ELEMENT(N,"","",P1)"
$ WRITE OUFILE "$ IF T1 .EQS. "","" THEN RETURN"
$ WRITE OUFILE "$ VLS = F$EXTRACT(0,3,F$ELEMENT(0,""*"",T1)+""___"")"
$ WRITE OUFILE "$ UCI = F$EXTRACT(0,3,F$ELEMENT(1,""*"",T1)+""___"")"
$ WRITE OUFILE "$ GLD = F$ELEMENT(2,""*"",T1)"
$ WRITE OUFILE "$ DEFINE /SYSTEM GTMAPI$'UCI'$'VLS' 'GLD'"
$ WRITE OUFILE "$ N = N + 1"
$ WRITE OUFILE "$ GOTO UCI"
$ CLOSE OUFILE
$ VMI$CALLBACK MESSAGE I PREINS "Preparing files for installation."
$!  GTMFIFILES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTMFIFILES.KIT
$ IF GTMFI$MGR_COM
$  THEN
$   WRITE OUFILE "GTMFI$ GTMFISTART.COM VMI$ROOT:[SYSMGR] C"
$ ENDIF
$ WRITE OUFILE "GTMFI$ GTMFISTART.COM ''GTMFI$DST_LOG'"
$ CLOSE OUFILE
$!  GTMFIIMAGES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTMFIIMAGES.KIT
$ WRITE OUFILE "GTMFI$ IBI_XFM.EXE ''GTMFI$DST_LOG'"
$ CLOSE OUFILE
$!  Provide with file.KITs
$ VMI$CALLBACK PROVIDE_FILE "" VMI$KWD:GTMFIFILES.KIT "" T
$ VMI$CALLBACK PROVIDE_IMAGE "" VMI$KWD:GTMFIIMAGES.KIT "" T
$ VMI$CALLBACK MESSAGE I FININS "Finalizing the installation."
$ IF GTMFI$START_GTMFI THEN VMI$CALLBACK SET POSTINSTALL YES
$ IF GTMFI$RUN_IVP THEN VMI$CALLBACK SET IVP YES
$ IF GTMFI$STARTDB THEN VMI$CALLBACK MODIFY_STARTUP_DB ADD GTMFISTART.COM END
$ EXIT VMI$_SUCCESS
$!
$POSTINSTALL:
$!
$ SET ON
$! do a gtmlogin
$ @'GTMFI$DST_LOG'GTMLOGIN
$ IF GTMFI$MGR_COM
$  THEN
$   T1 := SYS$MANAGER:
$  ELSE
$   T1 = GTMFI$DST_LOG
$ ENDIF
$ @'T1'GTMFISTART
$ EXIT VMI$_SUCCESS
$!
$IVP:
$!	The real Installation Verification Procedure.
$ TYPE SYS$INPUT

  GT.M FOCUS interface Installation Verification Procedure

$!  Extract the IVP .COM file from the text library.
$ LIBRARIAN /EXTRACT=GTMFI$IVP /OUTPUT=GTMFI$IVP.COM GTMFI$IVP.TLB
$ @GTMFI$IVP
$ EXIT $STATUS
