$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$ IF F$EXTRACT(0,5,P1) .EQS. "HELP_" THEN GOTO 'P1'
$ EXIT VMI$_UNSUPPORTED
$HELP_PURGE:
$ TYPE SYS$INPUT

  If GT.CM DDP is previously installed,  there  is  no  reason  to keep  older
  versions of the software online, unless you wish to test before purging.

$ EXIT
$HELP_NDB_CNT:
$ TYPE SYS$INPUT
  The installation inserts this  value as  the  default  for  controlling  the
  DDP Server quotas established in GTCMDDPSTART.COM.  If you  provide a  value
  less than 1, the value will be set to  1.  This value can be easily  changed
  later.

$ EXIT
$HELP_RC_CNT:
$ TYPE SYS$INPUT
  The installation  inserts  this  value as the  default for  controlling  the
  DDP Server quotas established in GTCMDDPSTART.COM.  If you  provide a  value
  less than 1, the value will be set to  1.  This value can be easily  changed
  later.

$ EXIT
$HELP_SRV_UIC:
$ TYPE SYS$INPUT
  The DDP Server usually must have broad  file  access  so  running  under the
  SYSTEM  UIC  may  make  sense.   The  recommended  alternative  is  to use a
  distinguished UIC which has appropriate UIC or ACL based  access  to  served
  files.

$ EXIT
$HELP_STD_CNF:
$ TYPE SYS$INPUT
  The standard configuration performs the following:

   *  Places files in SYS$COMMON:[GTM_DIST] with SYSTEM as owner
   *  Gives the DDP server a "UCI" of DDP, and a "VOLUME SET" of GTM
   *  Gives the DDP server a global directory of GTM$DIST:DDP_SERVER.GLD
   *  Creates a volume configuration file DDP_VOLCONF.LIS and copies it to GTM$DIST
   *  Copies the GT.CM DDP command procedures to SYS$MANAGER
   *  Adds GTCMDDPSTART.COM to the system startup database
   *  Starts GT.CM DDP at the end of the installation

  If the SYSTEM id is not set up, the installation will use [1,4].
  If you answer YES, no more questions will be asked.

$ EXIT
$HELP_DST_OWN:
$ TYPE SYS$INPUT
  Provide a UIC, normally SYSTEM, to own the files in the  GT.M  distribution.
  The UIC can be a name, a group name and a user name separated  by  a  comma,
  or a pair of octal codes separated by a comma which specify group and user.

$ EXIT
$HELP_SYS_DST:
$ TYPE SYS$INPUT
  Usual practice is to place a system component  such  as  GT.CM  DDP  on  the
  system disk.   If you have severe space constraints,  you may  need  to  use
  another volume.

$ EXIT
$HELP_SYS_DIR:
$ TYPE SYS$INPUT
  This directory  becomes be a sub-directory of  SYS$COMMON  and   holds   the
  distribution.   If it does not  exist,  the  installation  creates  it  with
  WORLD=RE access.   If you  are  not  concerned  with  mixing  software  from
  different vendors, you may wish to use SYSLIB.

$ EXIT
$HELP_DST_DEV:
$ TYPE SYS$INPUT
  The disk must be mounted, on-line and have adequate space to hold the  GT.CM
  DDP distribution.  The disk name may be physical or logical.

$ EXIT
$HELP_DST_DIR:
$ TYPE SYS$INPUT
  This  directory  holds  the  distribution.    If  it  does  not  exist,  the
  installation creates it with WORLD=RE access.

$ EXIT
$HELP_VOLCONF:
$ TYPE SYS$INPUT
  The GT.CM DDP Server requires one or more Global  Directories  in  order  to
  find the appropriate database files.   Each Global Directory  is  associated
  with a  "UCI"  "Volume-set"  pair  by  an entry  in the volume configuration
  file  in the form  of

  VOL	UCI	GlobalDirectory

  where  VOL  and  UCI  are each  3  characters  long.  The name of the volume
  configuration   file  is  parameter  P1  to  the  script   GTCMDDPSTART.COM.
  The file you  specify now will be generated and used as the default if P1 is
  not specified for GTCMDDPSTART. The installation  creates  the configuration
  file with one entry using the  "Volume-Set",  "UCI",  "Global Directory" you
  specify in response to the following questions.

$ EXIT
$HELP_VOL_NAME:
$ TYPE SYS$INPUT
  The GT.CM DDP Server requires one or more Global  Directories  in  order  to
  find the appropriate database files.   Each Global Directory  is  associated
  with a  "UCI"  "Volume-set"  pair  by  an entry  in the volume configuration
  file.  Specify the   "Volume-set"  name  that  you  wish  to  enter  in  the
  configuration file.

$ EXIT
$HELP_UCI_NAME:
$ TYPE SYS$INPUT
  The GT.CM DDP Server requires one or more Global  Directories  in  order  to
  find the appropriate database files.   Each Global Directory  is  associated
  with a  "UCI"  "Volume-set"  pair  by  an entry  in the volume configuration
  file.  Specify  the  "UCI"  name  for  the "Volume-set" name  you  specified
  before.

$ EXIT
$HELP_GBLDIR:
$ TYPE SYS$INPUT
  The GT.CM DDP Server requires one or more Global  Directories  in  order  to
  find the appropriate database files.   Each Global Directory  is  associated
  with a  "UCI"  "Volume-set"  pair  by  an entry  in the volume configuration
  file.   Specify  the  "Global Directory"  file  name  for  the  "Volume-set"
  "UCI" pair you  specified before.

$ EXIT
$HELP_STARTDB:
$ TYPE SYS$INPUT
  Answering yes causes the  installation  to  place  GTCMDDPSTART.COM  in  the
  startup database so the system  startup  automatically  sets  up  the  GT.CM
  DDP server whenever the system boots.

$ EXIT
$HELP_RUN_IVP:
$ TYPE SYS$INPUT
  This installation kit contains an installation verification procedure  (IVP)
  which you can run as part of the installation to verify the  correctness  of
  the software.   Note that if you choose this option, the  GT.M  images  must
  already be  installed.

$ EXIT
$HELP_START_CM:
$ TYPE SYS$INPUT
  Answering yes causes the installation to start GT.CM DDP Server.

$ EXIT
