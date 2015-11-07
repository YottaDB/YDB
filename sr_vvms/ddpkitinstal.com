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
$!	KITINSTAL.COM PROCEDURE FOR THE GT.CM DDP PRODUCT
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

  GT.CM DDP  (c)  COPYRIGHT 1988, 2002  by  Sanchez Computer Associates, Inc
                           ALL RIGHTS RESERVED

$!  the following 2 lines must be maintained
$ GTCM$VMS_VERSION :== 072	! Minimum VMS version required
$ GTCM$DISK_SPACE == 1600	! Minumum disk space on system disk required for install (2x result)
$ IF F$ELEMENT(0,",",VMI$VMS_VERSION) .EQS. "RELEASED"
$  THEN
$   GTCM$VMS_IS == F$ELEMENT(1,",",VMI$VMS_VERSION)
$   IF GTCM$VMS_IS .LTS. GTCM$VMS_VERSION
$    THEN
$     VMI$CALLBACK MESSAGE E VMSMISMATCH "This GT.CM DDP kit requires an existing VMS''GTCM$VMS_VERSION' system."
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
$   VMI$CALLBACK MESSAGE E NOSPACE "There is not enough disk space -- GT.CM DDP needs ''GTCM$DISK_SPACE' blocks."
$   EXIT VMI$_FAILURE
$ ENDIF
$!  setup default answers
$ GTCM$DOPURGE :== YES
$ GTCM$RUN_IVP == 0	!! should be "YES", but no IVP yet
$ GTCM$NDB_CNT == 12
$ GTCM$RC_CNT == 16
$ GTCM$STD_CNF :== YES
$ GTCM$DST_OWN :== SYSTEM
$ IF F$IDENTIFIER(GTCM$DST_OWN,"NAME_TO_NUMBER") .EQ. 0 THEN GTCM$DST_OWN :== 1,4
$ GTCM$SRV_UIC :==
$ GTCM$SYS_DST :== YES
$ GTCM$DST_DIR :== GTM_DIST
$ GTCM$DST_CRE == GTCM$DST_DIR
$ GTCM$DST_DEV :==
$ GTCM$STARTDB :== YES
$ GTCM$UCI_NAME :== DDP
$ GTCM$VOL_NAME :== GTM
$ GTCM$VOLCONF == "DDP_VOLCONF.LIS"
$ GTCM$START_SRV :== YES
$!
$ VMI$CALLBACK ASK GTCM$DOPURGE "Do you want to purge files replaced by this installation" 'GTCM$DOPURGE' B -
  "@VMI$KWD:DDPKITHLP HELP_PURGE"
$ IF .NOT. GTCM$DOPURGE THEN VMI$CALLBACK SET PURGE NO
$ VMI$CALLBACK ASK GTCM$NDB_CNT "How many networked databases will this node serve" 'GTCM$NDB_CNT' I -
  "@VMI$KWD:DDPKITHLP HELP_NDB_CNT"
$ IF GTCM$NDB_CNT .LT. 1
$  THEN
$   GTCM$NDB_CNT == 1
$   WRITE SYS$OUTPUT "  The installation set this value to 1 as 0 or negative values are not useful."
$ ENDIF
$ VMI$CALLBACK ASK GTCM$RC_CNT "How many client links will this node serve" 'GTCM$RC_CNT' I -
  "@VMI$KWD:DDPKITHLP HELP_RC_CNT"
$ IF GTCM$RC_CNT .LT. 1
$  THEN
$   GTCM$RC_CNT == 1
$   WRITE SYS$OUTPUT "  The installation set this value to 1 as 0 or negative values are not useful."
$ ENDIF
$ VMI$CALLBACK ASK GTCM$SRV_UIC "Under what UIC should the Server operate" "''GTCM$SRV_UIC'" S -
  "@VMI$KWD:DDPKITHLP HELP_SRV_UIC"
$ GTCM$SRV_UIC == GTCM$SRV_UIC - "[" - "]"
$ IF GTCM$SRV_UIC - "," .NES. GTCM$SRV_UIC THEN GTCM$SRV_UIC :== ['GTCM$SRV_UIC']
$ VMI$CALLBACK ASK GTCM$STD_CNF "Do you want the standard GT.CM DDP configuration" 'GTCM$STD_CNF' B -
  "@VMI$KWD:DDPKITHLP HELP_STD_CNF"
$ IF GTCM$STD_CNF
$  THEN
$   GTCM$SYS_DST == 1
$   GTCM$STARTDB == 1
$   GTCM$START_CM == 1
$   GTCM$DST_LOG :== SYS$COMMON:['GTCM$DST_DIR']
$   GTCM$DIR_TYPE :== COMMON
$   GTCM$GBLDIR == "DDP_SERVER.GLD"
$   GTCM$RUN_IVP == 0	!! no IVP yet
$  ELSE ! not standard configuration
$   VMI$CALLBACK ASK GTCM$DST_OWN "What UIC should own the GT.CM DDP distribution" 'GTCM$DST_OWN' S -
$   "@VMI$KWD:DDPKITHLP HELP_DST_OWN"
$   GTCM$DST_OWN == GTCM$DST_OWN - "[" - "]"
$   VMI$CALLBACK ASK GTCM$SYS_DST "Do you want the GT.CM DDP distribution to go into a System Directory" 'GTCM$SYS_DST' B -
    "@VMI$KWD:DDPKITHLP HELP_SYS_DST"
$   IF GTCM$SYS_DST
$    THEN
$     VMI$CALLBACK ASK GTCM$DST_DIR "In what System Directory do you want to place GT.CM DDP" 'GTCM$DST_DIR' S -
      "@VMI$KWD:DDPKITHLP HELP_SYS_DIR"
$     GTCM$DST_DIR == GTCM$DST_DIR - "[" - "]"
$     GTCM$DST_CRE == GTCM$DST_DIR
$     GTCM$DST_LOG :== SYS$COMMON:['GTCM$DST_DIR']
$     GTCM$DIR_TYPE :== COMMON
$    ELSE ! not system disk
$     VMI$CALLBACK ASK GTCM$DST_DEV "On which device do you want to place GT.CM DDP" "''GTCM$DST_DEV'" S -
      "@VMI$KWD:DDPKITHLP HELP_DST_DEV"
$     VMI$CALLBACK ASK GTCM$DST_DIR "In what directory on that device do you want to place GT.CM DDP" 'GTCM$DST_DIR' S -
      "@VMI$KWD:DDPKITHLP HELP_DST_DIR"
$     GTCM$DST_DEV == GTCM$DST_DEV - ":"
$     GTCM$DST_DIR == GTCM$DST_DIR - "[" - "]"
$     GTCM$DST_LOG :== 'GTCM$DST_DEV':['GTCM$DST_DIR']
$     GTCM$DST_CRE == GTCM$DST_LOG
$     GTCM$DIR_TYPE :== USER
$   ENDIF ! system disk
$   GTCM$GBLDIR == "DDP_SERVER.GLD"
$   VMI$CALLBACK ASK GTCM$VOLCONF "What file do you want as volume-set configuration file for the DDP Server" 'GTCM$VOLCONF' S -
    "@VMI$KWD:DDPKITHLP HELP_VOLCONF"
$   VMI$CALLBACK ASK GTCM$VOL_NAME "What ""VOLSET"" do you want to assign to the GT.CM DDP Server" 'GTCM$VOL_NAME' S -
    "@VMI$KWD:DDPKITHLP HELP_VOL_NAME"
$   VMI$CALLBACK ASK GTCM$UCI_NAME "What ""UCI"" do you want to assign to the GT.CM DDP Server" 'GTCM$UCI_NAME' S -
    "@VMI$KWD:DDPKITHLP HELP_UCI_NAME"
$   VMI$CALLBACK ASK GTCM$GBLDIR "What file do you want as the global directory for the GT.CM DDP Server" 'GTCM$GBLDIR' S -
    "@VMI$KWD:DDPKITHLP HELP_GBLDIR"
$   VMI$CALLBACK ASK GTCM$STARTDB "Do you want GTCMDDPSTART.COM in the startup database" 'GTCM$STARTDB' B -
    "@VMI$KWD:DDPKITHLP HELP_STARTDB"
$!! no IVP yet
$   IF 0 THEN VMI$CALLBACK ASK GTCM$RUN_IVP "Do you want to run the IVP (requires GT.M)" 'GTCM$RUN_IVP' B -
    "@VMI$KWD:DDPKITHLP HELP_RUN_IVP"
$   IF GTCM$RUN_IVP
$    THEN
$     GTCM$START_CCP == 1
$    ELSE
$     VMI$CALLBACK ASK GTCM$START_CM "Do you want to start GT.CM DDP now" 'GTCM$START_SRV' B "@VMI$KWD:DDPKITHLP HELP_START_CM"
$   ENDIF
$ ENDIF ! standard configuration
$ TYPE SYS$INPUT

  The following files are created and copied to appropriate destination

        GTCMDDPSTART.COM copied to SYS$MANAGER:
        GTCMDDPSTOP.COM  copied to SYS$MANAGER:
$ WRITE SYS$OUTPUT "        ''GTCM$VOLCONF' copied to ''GTCM$DST_LOG'"
$ TYPE SYS$INPUT

  Each file contains its own user documentation.

  All the questions have been asked. Installation now proceeds without your
  manual intervention for about 5-10 minutes.
$ IF GTCM$RUN_IVP THEN WRITE SYS$OUTPUT "  Finally the installation verification procedure tests the installation."
$ WRITE SYS$OUTPUT ""
$ VMI$CALLBACK CREATE_DIRECTORY 'GTCM$DIR_TYPE' 'GTCM$DST_CRE' "/OWNER_UIC=[''GTCM$DST_OWN'] /PROTECTION=(WO:RE)"
$ VMI$CALLBACK MESSAGE I CRECOM "Creating command files."
$!  Create GTCMDDPSTART.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTCMDDPSTART.COM
$ WRITE OUFILE "$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
$ WRITE OUFILE "$!								!"
$ WRITE OUFILE "$!	Copyright 1988, 2003 Sanchez Computer Associates, Inc.	!"
$ WRITE OUFILE "$!								!"
$ WRITE OUFILE "$!	This source code contains the intellectual property	!"
$ WRITE OUFILE "$!	of its copyright holder(s), and is made available	!"
$ WRITE OUFILE "$!	under a license.  If you do not know the terms of	!"
$ WRITE OUFILE "$!	the license, please stop and do not read further.	!"
$ WRITE OUFILE "$!								!"
$ WRITE OUFILE "$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! The invoking user requires the following privileges:"
$ WRITE OUFILE "$! CMKRNL, DETATCH, OPER, NETMBX, PSWAPM, SYSNAM, SYSGBL and TMPMBX."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! The parameters are:"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	P1 = VOLSET_CONFIGURATION_FILE"
$ WRITE OUFILE "$!		if not specified, or specifed as """", defaults to"
$ WRITE OUFILE "$!		what is specified during the installation. For"
$ WRITE OUFILE "$!		standard installation, GT.CM sets the default to"
$ WRITE OUFILE "$!		GTM$DIST:DDP_VOLCONF.LIS"
$ WRITE OUFILE "$!	P2 = CIRCUIT_NAME"
$ WRITE OUFILE "$!		if not specified, or specified as """", defaults to"
$ WRITE OUFILE "$!		NOD, where NOD is the first three characters of the"
$ WRITE OUFILE "$!		node name"
$ WRITE OUFILE "$!	Note, only the first three characters of CIRCUIT_NAME"
$ WRITE OUFILE "$! 	are used. If the specification is less than 3 characters in length,"
$ WRITE OUFILE "$!	the specification is suffixed with the appropriate number of"
$ WRITE OUFILE "$!	_ (underbar) characters."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	P3 = ETHERNET_DEVICE"
$ WRITE OUFILE "$!		no default"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	P4 = GROUPS (comma separated group numbers 0-15)"
$ WRITE OUFILE "$!		if not specified, or specified as """", defaults to 0"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	P5 = comma separated list of GT.CM DDP tunable parameters"
$ WRITE OUFILE "$!	Note, the delimiter is comma alone and not comma-space."
$ WRITE OUFILE "$!	To omit specifying a parameter, specify the empty string """", or omit"
$ WRITE OUFILE "$!	everything (including double-quotes) in the appropriate"
$ WRITE OUFILE "$!	comma-delimited position"
$ WRITE OUFILE "$!	P5 = P5A,P5B,P5C"
$ WRITE OUFILE "$!	where"
$ WRITE OUFILE "$!	P5A = MAX RECORD SIZE"
$ WRITE OUFILE "$!		if not specified, or specified as """", defaults to MINRECSIZE"
$ WRITE OUFILE "$!		MINRECSIZE is 1024 bytes"
$ WRITE OUFILE "$!		GT.CM rounds up (MAX RECORD SIZE + 39) to the nearest 512 byte"
$ WRITE OUFILE "$!		value and uses that for outbound/incoming buffer space. 39"
$ WRITE OUFILE "$!		is the overhead needed by GT.CM and the DDP protocol."
$ WRITE OUFILE "$!		Considering that ethernet messages cannot exceed 1500 bytes"
$ WRITE OUFILE "$!		and that the DDP protocol doesn't appear to support message"
$ WRITE OUFILE "$!		fragmenting, the value MINRECSIZE is sufficient to support all"
$ WRITE OUFILE "$!		database operations (buffer space is 1536 for record size 1024)."
$ WRITE OUFILE "$!		If we account for the overhead for a database request and"
$ WRITE OUFILE "$!		response, the maximum outbound request size (SET operation)"
$ WRITE OUFILE "$!		is 1478, and the maximum incoming result size is 1484."
$ WRITE OUFILE "$!		Keep this in mind while loading data into files that are"
$ WRITE OUFILE "$!		served DDP servers. Note, GT.CM imposes a limit of 255 for the"
$ WRITE OUFILE "$!		key length (maximum combined length of subscripts) and so"
$ WRITE OUFILE "$!		does the DDP protocol. In some cases, although GT.CM's format"
$ WRITE OUFILE "$!		for keys is shorter than this maximum, the format required"
$ WRITE OUFILE "$!		by DDP might be longer and hence might cause"
$ WRITE OUFILE "$!		GVSUBOFLOW/REC2BIG errors"
$ WRITE OUFILE "$!	P5B = ETHERNET RECEIVE BUFFER COUNT"
$ WRITE OUFILE "$!		if not specified, or specified as """", defaults to 64."
$ WRITE OUFILE "$!		GT.CM pre-allocates this many Ethernet Receive Buffers."
$ WRITE OUFILE "$!	P5C = MAXIMUM REQUEST CREDITS"
$ WRITE OUFILE "$!		if not specified, or specified as """", defaults to 4."
$ WRITE OUFILE "$!		GT.CM sends this value for the request credits in the protocol"
$ WRITE OUFILE "$!		handshake exchange (WI, II announce) messages.  This value"
$ WRITE OUFILE "$!		is used for flow control between client and server nodes."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! The name of the server will be"
$ WRITE OUFILE "$!	GTMDDP_SERV_<CIRCUIT_NAME>"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! Following logicals are defined in the system table by the startup script"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!      <Process-name-of-server> = <CIRCUIT_NAME>"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	DDPGVUSR = GTM$DIST:DDPGVUSR.EXE"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTMDDP_VOLCONF_<CIRCUIT_NAME> = P1"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	If P3 is specifed and not """","
$ WRITE OUFILE "$!		GTMDDP_CONTROLLER_<CIRCUIT_NAME> = <ETHERNET_DEVICE>"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	If P3 is not specified (or specifed as """"), or the DDP server fails to"
$ WRITE OUFILE "$!	open the specified <ETHERNET_DEVICE>, then the server attempts to open"
$ WRITE OUFILE "$!	the following devices in the order specified, and uses the first device"
$ WRITE OUFILE "$!	successfully opened."
$ WRITE OUFILE "$!		ECA0, ESA0, ETA0, EWA0, EXA0, EZA0, XEA0, XQA0"
$ WRITE OUFILE "$!	If the server fails to open all devices, it exits with an error."
$ WRITE OUFILE "$!	One can use SHOW PROC/ID=<DDP Server PID> to find the ethernet device"
$ WRITE OUFILE "$!	being used by the DDP server."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!      GTMDDP_GROUPS_<CIRCUIT_NAME> = P4"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTMDDP_MAXRECSIZE_<CIRCUIT_NAME> = P5A"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTMDDP_ETHRCVBUFCNT_<CIRCUIT_NAME> = P5B"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTMDDP_MAXREQCREDITS_<CIRCUIT_NAME> = P5C"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! In the below discussion, the units for quotas and limits are as per VMS"
$ WRITE OUFILE "$! documentation."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! P6 = comma seprated list of process quotas for the DDP server"
$ WRITE OUFILE "$! Note, the delimiter is comma alone and not comma-space."
$ WRITE OUFILE "$! To omit specifying a quota, specify the empty string """", or omit"
$ WRITE OUFILE "$! everything (including double-quotes) in the appropriate"
$ WRITE OUFILE "$! comma-delimited position"
$ WRITE OUFILE "$! P6 = P6A,P6B,P6C,P6D,P6E"
$ WRITE OUFILE "$! where"
$ WRITE OUFILE "$!	P6A is the maximum number of database files served to the network."
$ WRITE OUFILE "$!		if not specifed, or specified as """", defaults to value"
$ WRITE OUFILE "$!		specified during installation. For standard installation,"
$ WRITE OUFILE "$!		GT.CM sets the default to 12."
$ WRITE OUFILE "$!		This parameter is used to compute the following parameters"
$ WRITE OUFILE "$!		for the server"
$ WRITE OUFILE "$!		AST_LIMIT, ENQUEUE_LIMIT, FILE_LIMIT, IO_DIRECT, QUEUE_LIMIT,"
$ WRITE OUFILE "$!		PAGE_FILE "
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	P6B is the maximum number of remote clients served."
$ WRITE OUFILE "$!		if not specified, or specified as """", defaults to value"
$ WRITE OUFILE "$!		specified during installation. For standard installation,"
$ WRITE OUFILE "$!		GT.CM sets the default to 16."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	P6C is the default working set size"
$ WRITE OUFILE "$!		if not specified, or specified as """", defaults to"
$ WRITE OUFILE "$!		FILE_LIMIT * 200 (see how FILE_LIMIT is computed below)"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	P6D is the priority and should be at an appropriate priority"
$ WRITE OUFILE "$!	 	to balance network response with local load"
$ WRITE OUFILE "$!		if not specified, or specified as """", defaults to 5"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	P6E is the byte limit and should be greater than or equal to 30000"
$ WRITE OUFILE "$!		if not specified, or specified as """", defaults to 30000"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! The server is run with UIC specified during installation. For standard"
$ WRITE OUFILE "$! installation, GT.CM sets the UIC to [1,4]"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! Server errors are logged to SYS$MANAGER:DDP_CME.LOG"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! The server has DUMP enabled, and is run with option to inhibit SWAPPING while"
$ WRITE OUFILE "$! in wait state."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! Based on GT.CM requirements, each quota or limit is computed as"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$! AST_LIMIT = (P6A + 1) * 3 + P6B"
$ WRITE OUFILE "$! ENQUEUE_LIMIT = (P6A + 1) * 3"
$ WRITE OUFILE "$! FILE_LIMIT = P6A + P6B + 3"
$ WRITE OUFILE "$! IO_BUFFERED = (P6B + 3) * 1024"
$ WRITE OUFILE "$! IO_DIRECT = P6A * 7"
$ WRITE OUFILE "$! QUEUE_LIMIT = (P6A + 1) * 2"
$ WRITE OUFILE "$! PRIORITY = P6D"
$ WRITE OUFILE "$! WORKING_SET = P6C"
$ WRITE OUFILE "$! MAXIMUM_WORKING_SET = WORKING_SET + 200 "
$ WRITE OUFILE "$! BUFFER_LIMIT = P6E"
$ WRITE OUFILE "$! PAGE_FILE = P6A * 10000"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(""TMPMBX"")"
$ WRITE OUFILE "$ ON CONTROL_C THEN GOTO ERROR"
$ WRITE OUFILE "$ ON ERROR THEN GOTO ERROR"
$ WRITE OUFILE "$ CURPRV=F$SETPRV(""CMKRNL,DETACH,NETMBX,OPER,PSWAPM,SYSNAM,SYSGBL,TMPMBX"")+"",""+CURPRV"
$ WRITE OUFILE "$ IF F$PRIVILEGE(""CMKRNL,DETACH,NETMBX,OPER,PSWAPM,SYSNAM,SYSGBL,TMPMBX"")"
$ WRITE OUFILE "$  THEN"
$ WRITE OUFILE "$   DEFINE /SYSTEM DDPGVUSR GTM$DIST:DDPGVUSR.EXE"
$ WRITE OUFILE "$   NODE = F$GETSYI(""SCSNODE"")"
$ WRITE OUFILE "$   NOD = F$EDIT(F$EXTRACT(0,3,NODE),""TRIM"")"
$ WRITE OUFILE "$   IF F$LENGTH(NOD) .LT. 3 THEN NOD = NOD + F$EXTRACT(0,3-F$LENGTH(NOD),""___"")"
$ WRITE OUFILE "$   P2 = F$EDIT(P2,""COLLAPSE"")"
$ WRITE OUFILE "$   IF P2 .EQS. """""
$ WRITE OUFILE "$    THEN"
$ WRITE OUFILE "$     P2 = NOD"
$ WRITE OUFILE "$    ELSE"
$ WRITE OUFILE "$     P2 = F$EDIT(F$EXTRACT(0,3,P2),""TRIM"")"
$ WRITE OUFILE "$     IF F$LENGTH(P2) .LT. 3 THEN P2 = P2 + F$EXTRACT(0,3-F$LENGTH(P2),""___"")"
$ WRITE OUFILE "$   ENDIF"
$ WRITE OUFILE "$   P1 = F$EDIT(P1,""COLLAPSE"")"
$ WRITE OUFILE "$   IF P1 .EQS. """" THEN P1 = ""GTM$DIST:''GTCM$VOLCONF'"""
$ WRITE OUFILE "$   DEFINE /SYSTEM GTMDDP_VOLCONF_'P2' 'P1'"
$ WRITE OUFILE "$   IF P3 .NES. """""
$ WRITE OUFILE "$    THEN"
$ WRITE OUFILE "$     DEFINE /SYSTEM GTMDDP_CONTROLLER_'P2' 'P3'"
$ WRITE OUFILE "$    ELSE"
$ WRITE OUFILE "$     IF F$TRNLNM(""GTMDDP_CONTROLLER_'","'P2'"",""LNM$SYSTEM_TABLE"") .NES. """""
$ WRITE OUFILE "$      THEN"
$ WRITE OUFILE "$       DEASSIGN /SYSTEM GTMDDP_CONTROLLER_'P2'"
$ WRITE OUFILE "$     ENDIF"
$ WRITE OUFILE "$   ENDIF"
$ WRITE OUFILE "$   IF P4 .EQS. """" THEN P4 = ""0"""
$ WRITE OUFILE "$   DEFINE /SYSTEM GTMDDP_GROUPS_'P2' ""'","'P4'"""
$ WRITE OUFILE "$   P5A = F$ELEMENT(0,"","",P5)"
$ WRITE OUFILE "$   IF P5A .EQS. "," THEN P5A :="
$ WRITE OUFILE "$   P5B = F$ELEMENT(1,"","",P5)"
$ WRITE OUFILE "$   IF P5B .EQS. "," THEN P5B :="
$ WRITE OUFILE "$   P5C = F$ELEMENT(2,"","",P5)"
$ WRITE OUFILE "$   IF P5C .EQS. "," THEN P5C :="
$ WRITE OUFILE "$   MINRECSIZE = 1024"
$ WRITE OUFILE "$   IF P5A .LT. MINRECSIZE THEN P5A = MINRECSIZE"
$ WRITE OUFILE "$   DEFINE /SYSTEM GTMDDP_MAXRECSIZE_'P2' 'P5A'"
$ WRITE OUFILE "$   IF P5B .NES. """""
$ WRITE OUFILE "$    THEN"
$ WRITE OUFILE "$     DEFINE /SYSTEM GTMDDP_ETHRCVBUFCNT_'P2' 'P5B'"
$ WRITE OUFILE "$    ELSE"
$ WRITE OUFILE "$     IF F$TRNLNM(""GTMDDP_ETHRCVBUFCNT_'","'P2'"",""LNM$SYSTEM_TABLE"") .NES. """""
$ WRITE OUFILE "$      THEN"
$ WRITE OUFILE "$       DEASSIGN /SYSTEM GTMDDP_ETHRCVBUFCNT_'P2'"
$ WRITE OUFILE "$     ENDIF"
$ WRITE OUFILE "$   ENDIF"
$ WRITE OUFILE "$   IF P5C .NES. """""
$ WRITE OUFILE "$    THEN"
$ WRITE OUFILE "$     DEFINE /SYSTEM GTMDDP_MAXREQCREDITS_'P2' 'P5C'"
$ WRITE OUFILE "$    ELSE"
$ WRITE OUFILE "$     IF F$TRNLNM(""GTMDDP_MAXREQCREDITS_'","'P2'"",""LNM$SYSTEM_TABLE"") .NES. """""
$ WRITE OUFILE "$      THEN"
$ WRITE OUFILE "$       DEASSIGN /SYSTEM GTMDDP_MAXREQCREDITS_'P2'"
$ WRITE OUFILE "$     ENDIF"
$ WRITE OUFILE "$   ENDIF"
$ WRITE OUFILE "$   PROCNAME := GTMDDP_SERV_'P2'"
$ WRITE OUFILE "$   WRITE SYS$OUTPUT ""Starting the GT.CM DDP Server as process "",PROCNAME"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!  Note, all logicals are keyed by the <CIRCUIT_NAME>, but to pass the"
$ WRITE OUFILE "$!  the circuit name to the server, we use a logical <server-process-name>,"
$ WRITE OUFILE "$!  the value of which will be set to the CIRCUIT_NAME by this script."
$ WRITE OUFILE "$!  Server will find its circuit name by translating the logical"
$ WRITE OUFILE "$!  <server-process-name>"
$ WRITE OUFILE "$   DEFINE /SYSTEM 'PROCNAME' 'P2'"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$   P6A = F$ELEMENT(0,"","",P6)"
$ WRITE OUFILE "$   IF P6A .EQS. "","" then P6A :="
$ WRITE OUFILE "$   P6B = F$ELEMENT(1,"","",P6)"
$ WRITE OUFILE "$   IF P6B .EQS. "","" THEN P6B :="
$ WRITE OUFILE "$   P6C = F$ELEMENT(2,"","",P6)"
$ WRITE OUFILE "$   IF P6C .EQS. "","" THEN P6C :="
$ WRITE OUFILE "$   P6D = F$ELEMENT(3,"","",P6)"
$ WRITE OUFILE "$   IF P6D .EQS. "","" THEN P6D :="
$ WRITE OUFILE "$   P6E = F$ELEMENT(4,"","",P6)"
$ WRITE OUFILE "$   IF P6E .EQS. "","" THEN P6E :="
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$   IF P6A .EQS. """" THEN P6A = ''GTCM$NDB_CNT'"
$ WRITE OUFILE "$   IF P6B .EQS. """" THEN P6B = ''GTCM$RC_CNT'"
$ WRITE OUFILE "$   AL = ((P6A + 1) * 3) + P6B"
$ WRITE OUFILE "$   BL = (P6B + 3) * 1024"
$ WRITE OUFILE "$   EL = (P6A + 1) * 3"
$ WRITE OUFILE "$   FL = P6A + 3 + PP7"
$ WRITE OUFILE "$   DL = P6A * 7"
$ WRITE OUFILE "$   TQ  = (P6A + 1) * 2"
$ WRITE OUFILE "$   IF P6C .EQS. """" THEN P6C = FL * 200"
$ WRITE OUFILE "$   WSE = P6C + 200"
$ WRITE OUFILE "$   IF P6D .EQS. """" THEN P6D = 5"
$ WRITE OUFILE "$   IF P6E .EQS. """" THEN P6E = 30000"
$ WRITE OUFILE "$   PF = P6A * 10000"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$   RUN /DETACHED /PROCESS='PROCNAME' /PRIV=(SYSNAM) -"
$ WRITE OUFILE "        /ERROR=SYS$MANAGER:DDP_CME.LOG -"
$ WRITE OUFILE "        /DUMP /NOSWAPPING /UIC=''GTCM$SRV_UIC' /AST_LIMIT='AL' /ENQUEUE_LIMIT='EL' -"
$ WRITE OUFILE "        /FILE_LIMIT='FL' /IO_BUFFERED='BL' /IO_DIRECT='DL' /QUEUE_LIMIT='TQ' -"
$ WRITE OUFILE "        /PRIORITY='P6D' /WORKING_SET='P6C' /MAXIMUM_WORKING_SET='WSE' -"
$ WRITE OUFILE "        /BUFFER_LIMIT='P6E' /PAGE_FILE='PF' GTM$DIST:DDPSERVER"
$ WRITE OUFILE "$  ELSE"
$ WRITE OUFILE "$   WRITE SYS$OUTPUT ""NOT starting GT.CM DDP Server because of inadequate privileges"""
$ WRITE OUFILE "$ ENDIF"
$ WRITE OUFILE "$ERROR:"
$ WRITE OUFILE "$ CURPRV = F$SETPRV(CURPRV)"
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$!  Create GTCMDDPSTOP.COM
$ OPEN /WRITE OUFILE VMI$KWD:GTCMDDPSTOP.COM
$ WRITE OUFILE "$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
$ WRITE OUFILE "$!								!"
$ WRITE OUFILE "$!	Copyright 1988, 2003 Sanchez Computer Associates, Inc.	!"
$ WRITE OUFILE "$!								!"
$ WRITE OUFILE "$!	This source code contains the intellectual property	!"
$ WRITE OUFILE "$!	of its copyright holder(s), and is made available	!"
$ WRITE OUFILE "$!	under a license.  If you do not know the terms of	!"
$ WRITE OUFILE "$!	the license, please stop and do not read further.	!"
$ WRITE OUFILE "$!								!"
$ WRITE OUFILE "$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$!	GTCMDDPSTOP.COM stops the GT.CM DDP Server for a node."
$ WRITE OUFILE "$!	Place an invocation or copy of this procedure in the site specific"
$ WRITE OUFILE "$!	 shutdown: SYS$MANAGER:SYSHUTDWN to ensure all GT.M databases are"
$ WRITE OUFILE "$!	 properly closed before VMS terminates.  GTCMDDPSTOP should precede"
$ WRITE OUFILE "$!	 GTCXSTOP, if used, and GTMSTOP, in any case."
$ WRITE OUFILE "$!"
$ WRITE OUFILE "$ STOPIMAGE := $GTM$DIST:GTCMDDPSTOP.EXE"
$ WRITE OUFILE "$ STOPIMAGE DDPSERVER"
$ WRITE OUFILE "$ EXIT"
$ CLOSE OUFILE
$! Create the volume configuration file
$ OPEN /WRITE OUFILE VMI$KWD:'GTCM$VOLCONF'
$ WRITE OUFILE "#################################################################"
$ WRITE OUFILE "#								#"
$ WRITE OUFILE "#	Copyright 2002, 2003 Sanchez Computer Associates, Inc.	#"
$ WRITE OUFILE "#								#"
$ WRITE OUFILE "#	This source code contains the intellectual property	#"
$ WRITE OUFILE "#	of its copyright holder(s), and is made available	#"
$ WRITE OUFILE "#	under a license.  If you do not know the terms of	#"
$ WRITE OUFILE "#	the license, please stop and do not read further.	#"
$ WRITE OUFILE "#								#"
$ WRITE OUFILE "#################################################################"
$ WRITE OUFILE ""
$ WRITE OUFILE "# The first argument to the GT.M DDP startup script (GTCMDDPSTART.COM)"
$ WRITE OUFILE "# is the name of the configuration file listing the VOLUMEs and UCIs"
$ WRITE OUFILE "# that the server serves. If the first argument is not specified,"
$ WRITE OUFILE "# this file is used."
$ WRITE OUFILE ""
$ WRITE OUFILE "# Empty lines and those beginning with # are ignored by the server."
$ WRITE OUFILE ""
$ WRITE OUFILE "# One VOLUME, UCI, GLD triple can be specified on a line as"
$ WRITE OUFILE "# VOL	UCI	GLD"
$ WRITE OUFILE "# VOL and UCI must be three characters long and consist of only"
$ WRITE OUFILE "# uppercase letters."
$ WRITE OUFILE ""
$ WRITE OUFILE "# There can be any number of white space characters between the VOLUME"
$ WRITE OUFILE "# and UCI, and UCI and GLD. All characters after the first white space"
$ WRITE OUFILE "# after GLD are ignored."
$ WRITE OUFILE ""
$ WRITE OUFILE "# There must be at least one valid entry for the server to start"
$ WRITE OUFILE "# succesfully. No more than 16 entries are accepted by the server."
$ WRITE OUFILE "# If multiple entries exist for the same VOLUME, UCI pair, the last"
$ WRITE OUFILE "# entry is accepted and all previous entries are ignored."
$ WRITE OUFILE ""
$ WRITE OUFILE "''GTCM$VOL_NAME'	''GTCM$UCI_NAME'	GTM$DIST:''GTCM$GBLDIR'"
$ CLOSE OUFILE
$!
$ VMI$CALLBACK MESSAGE I PREINS "Preparing files for installation."
$!  GTCMFILES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTCMFILES.KIT
$ WRITE OUFILE "GTCM$ GTCMDDPSTART.COM VMI$ROOT:[SYSMGR] C"
$ WRITE OUFILE "GTCM$ GTCMDDPSTOP.COM VMI$ROOT:[SYSMGR] C"
$ WRITE OUFILE "GTCM$ GTCMDDPSTART.COM ''GTCM$DST_LOG'"
$ WRITE OUFILE "GTCM$ GTCMDDPSTOP.COM ''GTCM$DST_LOG'"
$ WRITE OUFILE "GTCM$ ''GTCM$VOLCONF' ''GTCM$DST_LOG'"
$ CLOSE OUFILE
$!  GTCMIMAGES.KIT must be maintained as kit contents change
$ OPEN /WRITE OUFILE VMI$KWD:GTCMIMAGES.KIT
$ WRITE OUFILE "GTCM$ DDPGVUSR.EXE ''GTCM$DST_LOG'"
$ WRITE OUFILE "GTCM$ DDPSERVER.EXE ''GTCM$DST_LOG'"
$ WRITE OUFILE "GTCM$ GTCMDDPSTOP.EXE ''GTCM$DST_LOG'"
$ CLOSE OUFILE
$!  Provide the command procedures and configuration file(s)
$ VMI$CALLBACK PROVIDE_FILE "" VMI$KWD:GTCMFILES.KIT "" T
$!  Make sure the owner is who the installer asked for
$ VMI$CALLBACK SECURE_FILE 'GTCM$DST_LOG'GTCMDDPSTART.COM ['GTCM$DST_OWN']
$ VMI$CALLBACK SECURE_FILE 'GTCM$DST_LOG'GTCMDDPSTOP.COM ['GTCM$DST_OWN']
$ VMI$CALLBACK SECURE_FILE 'GTCM$DST_LOG''GTCM$VOLCONF' ['GTCM$DST_OWN']
$!
$!  Provide the images
$ VMI$CALLBACK PROVIDE_IMAGE "" VMI$KWD:GTCMIMAGES.KIT "" T
$!  Make sure the owner is who the installer asked for
$ VMI$CALLBACK SECURE_FILE 'GTCM$DST_LOG'DDPGVUSR.EXE ['GTCM$DST_OWN']
$ VMI$CALLBACK SECURE_FILE 'GTCM$DST_LOG'DDPSERVER.EXE ['GTCM$DST_OWN']
$ VMI$CALLBACK SECURE_FILE 'GTCM$DST_LOG'GTCMDDPSTOP.EXE ['GTCM$DST_OWN']
$!
$ VMI$CALLBACK MESSAGE I FININS "Finalizing the installation."
$ IF GTCM$START_CM THEN VMI$CALLBACK SET POSTINSTALL YES
$ IF GTCM$RUN_IVP THEN VMI$CALLBACK SET IVP YES
$ IF GTCM$STARTDB THEN VMI$CALLBACK MODIFY_STARTUP_DB ADD GTCMDDPSTART.COM END
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
$ @'T1'GTCMDDPSTOP
$ SET ON
$ @'T1'GTCMDDPSTART
$ EXIT VMI$_SUCCESS
$!
$IVP:
$!	The real Installation Verification Procedure.
$ TYPE SYS$INPUT

  GT.CM DDP Installation Verification Procedure

$!  Extract the IVP .COM file from the text library.
$ LIBRARIAN /EXTRACT=GTCMDDP$IVP /OUTPUT=GTCMDDP$IVP.COM GTCMDDP$IVP.TLB
$ @GTCMDDP$IVP
$ EXIT $STATUS
