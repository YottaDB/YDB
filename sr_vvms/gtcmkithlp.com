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
$!      HELP TEXT PROCESSING FOR GT.CM KITINSTAL.COM
$!      COPYRIGHT 1989 - 2000 Sanchez Computer Associates
$ IF F$EXTRACT(0,5,P1) .EQS. "HELP_" THEN GOTO 'P1'
$ EXIT VMI$_UNSUPPORTED
$HELP_PURGE:
$ TYPE SYS$INPUT
  If GT.CM is previously installed,  there is no reason to keep older versions
  of the software online, unless you wish to test before purging.

$ EXIT
$HELP_NDB_CNT:
$ TYPE SYS$INPUT
  The installation  inserts  this  value as the  default for  controlling the
  GT.CM Server quotas established in GTCMSERVER.COM.  If you provide a  value
  less than 1, the value will be set to 1.   This value can be easily changed
  later.

$ EXIT
$HELP_RC_CNT:
$ TYPE SYS$INPUT
  The installation  inserts  this  value as the  default for  controlling the
  GT.CM Server quotas established in GTCMSERVER.COM.  If you provide a  value
  less than 1, the value will be set to 1.   This value can be easily changed
  later.

$ EXIT
$HELP_SRV_UIC:
$ TYPE SYS$INPUT
  The GT.CM Server usually must have broad file access  so  running  under the
  SYSTEM  UIC  may  make  sense.   The  recommended  alternative  is  to use a
  distinguished UIC which has appropriate UIC or ACL based access to clustered
  files.

$ EXIT
$HELP_STD_CNF:
$ TYPE SYS$INPUT
  The standard configuration performs the following:

   *  Places files in SYS$COMMON:[GTM_DIST] with SYSTEM as owner
   *  Copies the GT.CM command procedures, except GTCMAUTOSRV, to SYS$MANAGER
   *  Adds GTCMSTART.COM to the system startup database
   *  Sets up GTCMSTART.COM to start the GT.CM Server
   *  Enables auto-start and disables auto-stop of the GT.CM Server
   *  Starts GT.CM at the end of the installation

  If the SYSTEM id is not set up, the installation will use [1,4].

$ EXIT
$HELP_DST_OWN:
$ TYPE SYS$INPUT
  Provide a UIC, normally SYSTEM, to own the files in the  GT.M  distribution.
  The UIC can be a name, a group name and a user name separated  by  a  comma,
  or a pair of octal codes separated by a comma which specify group and user.

$ EXIT
$HELP_SYS_DST:
$ TYPE SYS$INPUT
  Usual practice is to place a system component such as GT.CM  on  the  system
  disk.   If you have  severe  space constraints,  you may need to use another
  volume.

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
  distribution.  The disk name may be physical or logical.

$ EXIT
$HELP_DST_DIR:
$ TYPE SYS$INPUT
  This  directory  holds  the  distribution.    If  it  does  not  exist,  the
  installation creates it with WORLD=RE access.

$ EXIT
$HELP_STD_SRV:
$ TYPE SYS$INPUT
  Answering Yes  causes  the  installation  make  GTCMSTART.COM so it starts a
  GT.CM Server.  Having the GT.CM Server running at all times makes  for  more
  uniform response to network database access.  However, if network  access is
  sporadic the  Server may spend much time as an idle task.

$ EXIT
$HELP_AUTO_SRV:
  Answering Yes  causes an incoming network request to start a GT.CM Server if
  none is currently running.   When  combined  with  the  standard  start this
  provides resiliency for network operations.

$ TYPE SYS$INPUT
$ EXIT
$HELP_TIMEOUT:
  The timeout  specifies  the number of minutes  an auto-started  GT.CM Server
  waits between incoming database access requests before shutting itself down.
  A value of 0 inhibits auto-stops.

$ TYPE SYS$INPUT
$ EXIT
$HELP_STARTDB:
$ TYPE SYS$INPUT
  Answering yes causes the installation to place GTCMSTART.COM in the  startup
  database so the system startup automatically sets up  the  GT.CM environment
  whenever the system boots.

$ EXIT
$HELP_MGR_COM:
$ TYPE SYS$INPUT
  You may prevent the installation from moving the .COM files to  SYS$MANAGER.
  Copying the command  procedures  to  SYS$MANAGER  allows  system startup to
  access them  through  the  VMS  startup  database  and generally simplifies
  operations.  However, if you wish to have multiple versions of GT.M on your
  system at the same time, you would not have multiple copies of the  command
  procedures in SYS$MANAGER.

$ EXIT
$HELP_RUN_IVP:
$ TYPE SYS$INPUT
  This installation kit contains an installation verification procedure  (IVP)
  which you can run as part of the installation to verify the  correctness  of
  the software.   Note that if you choose this option, the  GT.M  images  must
  already be installed before or during this installation.

$ EXIT
$HELP_START_CM:
$ TYPE SYS$INPUT
  Answering yes causes the installation to start GT.CM.

$ EXIT
