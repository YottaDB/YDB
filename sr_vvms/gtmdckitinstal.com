$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2005, 2010 Fidelity Information Services, Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$!
$!	KITINSTAL.COM PROCEDURE FOR THE GT.M DBCERTIFY UTILITY
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

GT.M DBCERTIFY  (c)  COPYRIGHT 2005  by  Fidelity Information Services, Inc
                           ALL RIGHTS RESERVED

$!  the following 2 lines must be maintained
$ GTMDC$VMS_VERSION :== 072	! Minimum VMS version required
$ GTMDC$DISK_SPACE == 840	! Minumum disk space on system disk required for install (2x result)
$ IF F$ELEMENT(0,",",VMI$VMS_VERSION) .EQS. "RELEASED"
$  THEN
$   GTMDC$VMS_IS == F$ELEMENT(1,",",VMI$VMS_VERSION)
$   IF GTMDC$VMS_IS .LTS. GTMDC$VMS_VERSION
$    THEN
$     VMI$CALLBACK MESSAGE E VMSMISMATCH "This GT.M DBCERTIFY utility kit requires an existing VMS''GTMDC$VMS_VERSION' system."
$     EXIT VMI$_FAILURE
$   ENDIF
$  ELSE
$   GTMDC$VMS_IS :==
$   WRITE SYS$OUTPUT "  No VMS version checking performed for field test versions."
$ ENDIF
$ IF (GTMDC$VMS_IS .GES. "052") THEN T1 = F$VERIFY(VMI$KIT_DEBUG)
$ VMI$CALLBACK CHECK_NET_UTILIZATION GTMDC$ROOM 'GTMDC$DISK_SPACE'
$ IF .NOT. GTMDC$ROOM
$  THEN
$   VMI$CALLBACK MESSAGE E NOSPACE "There is not enough disk space -- GT.M DBCERTIFY utility kit needs ''GTMDC$DISK_SPACE' blocks."
$   EXIT VMI$_FAILURE
$ ENDIF
$!  setup default answers
$ GTMDC$DOPURGE :== YES
$ GTMDC$RUN_IVP == 0	!! should be "YES", but no IVP yet
$ GTMDC$BLD_EXE :== YES	!! build V5CBSU.EXE in postinstall
$ GTMDC$STD_CNF :== YES
$ GTMDC$DST_OWN :== SYSTEM
$ IF F$IDENTIFIER(GTMDC$DST_OWN,"NAME_TO_NUMBER") .EQ. 0 THEN GTMDC$DST_OWN :== 1,4
$ GTMDC$SYS_DST :== YES
$ GTMDC$DST_DIR :== GTMDBCERTIFY
$ GTMDC$DST_CRE == GTMDC$DST_DIR
$ GTMDC$DST_DEV :==
$ GTMDC$MGR_COM :== YES
$ GTMDC$GTM_LOG :== GTM$DIST:
$!
$ VMI$CALLBACK ASK GTMDC$DOPURGE "Do you want to purge files replaced by this installation" 'GTMDC$DOPURGE' B -
  "@VMI$KWD:GTMDCKITHLP HELP_PURGE"
$ IF .NOT. GTMDC$DOPURGE THEN VMI$CALLBACK SET PURGE NO
$ VMI$CALLBACK ASK GTMDC$STD_CNF "Do you want the standard GT.M DBCERTIFY utility configuration" 'GTMDC$STD_CNF' B -
  "@VMI$KWD:GTMDCKITHLP HELP_STD_CNF"
$ IF GTMDC$STD_CNF
$  THEN
$   GTMDC$SYS_DST == 1
$   GTMDC$MGR_COM == 1
$   GTMDC$DST_LOG :== SYS$COMMON:['GTMDC$DST_DIR']
$   GTMDC$DIR_TYPE :== COMMON
$   GTMDC$GTM_LOG :== GTM$DIST:
$   GTMDC$RUN_IVP == 0	!! no IVP yet
$  ELSE ! not standard configuration
$   VMI$CALLBACK ASK GTMDC$DST_OWN "What UIC should own the GT.M DBCERTIFY utility distribution" 'GTMDC$DST_OWN' S -
    "@VMI$KWD:GTMDCKITHLP HELP_DST_OWN"
$   GTMDC$DST_OWN == GTMDC$DST_OWN - "[" - "]"
$   VMI$CALLBACK ASK GTMDC$SYS_DST "Do you want the GT.M DBCERTIFY utility distribution to go into a System Directory" -
    'GTMDC$SYS_DST' B "@VMI$KWD:GTMDCKITHLP HELP_SYS_DST"
$   IF GTMDC$SYS_DST
$    THEN
$     VMI$CALLBACK ASK GTMDC$DST_DIR "In what System Directory do you want to place GT.M DBCERTIFY utility" 'GTMDC$DST_DIR' S -
      "@VMI$KWD:GTMDCKITHLP HELP_SYS_DIR"
$     GTMDC$DST_DIR == GTMDC$DST_DIR - "[" - "]"
$     GTMDC$DST_CRE == GTMDC$DST_DIR
$     GTMDC$DST_LOG :== SYS$COMMON:['GTMDC$DST_DIR']
$     GTMDC$DIR_TYPE :== COMMON
$    ELSE ! not system disk
$     GTMDC$MGR_COM == 0
$     VMI$CALLBACK ASK GTMDC$DST_DEV "On which device do you want to place the GT.M DBCERTIFY utility" "" S -
      "@VMI$KWD:GTMDCKITHLP HELP_DST_DEV"
$     VMI$CALLBACK ASK GTMDC$DST_DIR "In what directory on that device do you want to place the GT.M DBCERTIFY utility" -
      'GTMDC$DST_DIR' S "@VMI$KWD:GTMDCKITHLP HELP_DST_DIR"
$     GTMDC$DST_DEV == GTMDC$DST_DEV - ":"
$     GTMDC$DST_DIR == GTMDC$DST_DIR - "[" - "]"
$     GTMDC$DST_LOG :== 'GTMDC$DST_DEV':['GTMDC$DST_DIR']
$     GTMDC$DST_CRE == GTMDC$DST_LOG
$     GTMDC$DIR_TYPE :== USER
$     VMI$CALLBACK ASK GTMDC$GTM_LOG "Where is the existing GT.M distribution located" -
      'GTMDC$GTM_LOG' S "@VMI$KWD:GTMDCKITHLP HELP_GTM_LOG"
$     IF F$LOCATE(":", GTMDC$GTM_LOG) .NE. F$LENGTH(GTMDC$GTM_LOG) .AND. -
         F$LOCATE("]", GTMDC$GTM_LOG) .NE. F$LENGTH(GTMDC$GTM_LOG)
$     THEN
$          GTMDC$GTM_LOG :== 'GTMDC$GTM_LOG':
$     ENDIF
$   ENDIF ! system disk
$   VMI$CALLBACK ASK GTMDC$BLD_EXE "Do you want to build V5CBSU.EXE now" 'GTMDC$BLD_EXE' B -
    "@VMI$KWD:GTMDCKITHLP HELP_BLD_EXE"
$!! no IVP yet
$   IF FALSE THEN VMI$CALLBACK ASK GTMDC$RUN_IVP "Do you want to run the IVP (requires GT.M)" 'GTMDC$RUN_IVP' B -
    "@VMI$KWD:GTMDCKITHLP HELP_RUN_IVP"
$ ENDIF ! standard configuration
$ IF GTMDC$MGR_COM
$  THEN
$   WRITE SYS$OUTPUT "  The following command files are created and copied to SYS$MANAGER:"
$  ELSE
$   WRITE SYS$OUTPUT "  The following command files are created:"
$ ENDIF
$ TYPE SYS$INPUT

        GTMDCBUILD.COM
        GTMDCDEFINE.COM

  Each file contains its own user documentation.

  All the questions have been asked. Installation now proceeds without your
  manual intervention for about 5-10 minutes.
$ WRITE SYS$OUTPUT ""
$ VMI$CALLBACK CREATE_DIRECTORY 'GTMDC$DIR_TYPE' 'GTMDC$DST_CRE' "/OWNER_UIC=[''GTMDC$DST_OWN'] /PROTECTION=(WO:RE)"
$ VMI$CALLBACK MESSAGE I CRECOM "Creating command files."
$!  Create GTMDCBUILD.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTMDCBUILD.COM
$ WRITE OUFILE "$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
$ WRITE OUFILE "$!                                                              !"
$ WRITE OUFILE "$!      Copyright 2005, 2010 Fidelity Information Services, Inc !"
$ WRITE OUFILE "$!                                                              !"
$ WRITE OUFILE "$!      This source code contains the intellectual property     !"
$ WRITE OUFILE "$!      of its copyright holder(s), and is made available       !"
$ WRITE OUFILE "$!      under a license.  If you do not know the terms of       !"
$ WRITE OUFILE "$!      the license, please stop and do not read further.       !"
$ WRITE OUFILE "$!                                                              !"
$ WRITE OUFILE "$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTMDCBUILD.COM builds V5CBSU.EXE from V5CBSU.M."
$ WRITE OUFILE "$!	It calls GTMLOGIN.COM to setup the existing GT.M environment"
$ WRITE OUFILE "$!	 then compiles and links V5CBSU."
$ WRITE OUFILE "$! "
$ WRITE OUFILE "$ @''GTMDC$GTM_LOG'GTMLOGIN"
$ WRITE OUFILE "$ SET DEFAULT ''GTMDC$DST_LOG'"
$ WRITE OUFILE "$ SET NOON"
$ WRITE OUFILE "$ MUMPS V5CBSU"
$ WRITE OUFILE "$ IF $STATUS THEN LINK V5CBSU,''GTMDC$GTM_LOG'_DH,''GTMDC$GTM_LOG'_EXP,''GTMDC$GTM_LOG'_UCASE"
$ WRITE OUFILE "$! "
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$!  Create GTMDCDEFINE.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTMDCDEFINE.COM
$ WRITE OUFILE "$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
$ WRITE OUFILE "$!                                                              !"
$ WRITE OUFILE "$!      Copyright 2005, 2010 Fidelity Information Services, Inc       !"
$ WRITE OUFILE "$!                                                              !"
$ WRITE OUFILE "$!      This source code contains the intellectual property     !"
$ WRITE OUFILE "$!      of its copyright holder(s), and is made available       !"
$ WRITE OUFILE "$!      under a license.  If you do not know the terms of       !"
$ WRITE OUFILE "$!      the license, please stop and do not read further.       !"
$ WRITE OUFILE "$!                                                              !"
$ WRITE OUFILE "$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTMDCDEFINE.COM defines symbols for DBCERTIFY.EXE and V5CBSU.EXE."
$ WRITE OUFILE "$!	It calls GTMLOGIN.COM to setup the existing GT.M environment"
$ WRITE OUFILE "$!	 then defines the symbols."
$ WRITE OUFILE "$! "
$ WRITE OUFILE "$ @''GTMDC$GTM_LOG'GTMLOGIN"
$ WRITE OUFILE "$ DBCERTIFY:==$''GTMDC$DST_LOG'DBCERTIFY.EXE"
$ WRITE OUFILE "$ V5CBSU:==$''GTMDC$DST_LOG'V5CBSU.EXE"
$ WRITE OUFILE "$! "
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$ VMI$CALLBACK MESSAGE I PREINS "Preparing files for installation."
$!  GTMDCFILES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTMDCFILES.KIT
$ IF GTMDC$MGR_COM
$  THEN
$   WRITE OUFILE "GTMDC$ GTMDCBUILD.COM VMI$ROOT:[SYSMGR] C"
$   WRITE OUFILE "GTMDC$ GTMDCDEFINE.COM VMI$ROOT:[SYSMGR] C"
$ ENDIF
$ WRITE OUFILE "GTMDC$ GTMDCBUILD.COM ''GTMDC$DST_LOG'"
$ WRITE OUFILE "GTMDC$ GTMDCDEFINE.COM ''GTMDC$DST_LOG'"
$ WRITE OUFILE "GTMDC$ V5CBSU.M ''GTMDC$DST_LOG'"
$ CLOSE OUFILE
$!  GTMDCIMAGES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTMDCIMAGES.KIT
$ WRITE OUFILE "GTMDC$ DBCERTIFY.EXE ''GTMDC$DST_LOG'"
$ CLOSE OUFILE
$!  Provide with file.KITs
$ VMI$CALLBACK PROVIDE_FILE "" VMI$KWD:GTMDCFILES.KIT "" T
$ VMI$CALLBACK PROVIDE_IMAGE "" VMI$KWD:GTMDCIMAGES.KIT "" T
$ VMI$CALLBACK MESSAGE I FININS "Finalizing the installation."
$ IF GTMDC$BLD_EXE THEN VMI$CALLBACK SET POSTINSTALL YES
$ EXIT VMI$_SUCCESS
$!
$POSTINSTALL:
$!
$ TYPE SYS$INPUT

  Building V5CBSU.EXE

$ SET ON
$ ON CONTROL_Y THEN EXIT VMI$_FAILURE
$ IF GTMDC$MGR_COM
$  THEN
$   T1 := SYS$MANAGER:
$  ELSE
$   T1 = GTMDC$DST_LOG
$ ENDIF
$ @'T1'GTMDCBUILD
$ ON CONTROL_Y THEN EXIT VMI$_FAILURE
$ EXIT VMI$_SUCCESS
$!
$IVP:
$!	The real Installation Verification Procedure.
$ TYPE SYS$INPUT

  There is no GT.M DBCERTIFY utility Installation Verification Procedure

$ EXIT $STATUS
