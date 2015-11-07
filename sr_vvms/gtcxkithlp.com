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
$!      HELP TEXT PROCESSING FOR GT.CX KITINSTAL.COM
$!      COPYRIGHT 1989 - 2000 Sanchez Computer Associates
$ IF F$EXTRACT(0,5,P1) .EQS. "HELP_" THEN GOTO 'P1'
$ EXIT VMI$_UNSUPPORTED
$HELP_READY:
$ TYPE SYS$INPUT
  If GT.CX is installed, it should not be active during an installation.

$ EXIT
$HELP_PURGE:
$ TYPE SYS$INPUT
  If GT.CX is previously installed,  there is no reason to keep older versions
  of the software online, unless you wish to test before purging.

$ EXIT
$HELP_CDB_CNT:
$ TYPE SYS$INPUT
  The installation inserts this value as the  default for  controlling the CCP
  quotas  established in GTCXSTART.COM.   If you  provide a value less than 1,
  the value will be set to 1.  This value can be easily changed later.

$ EXIT
$HELP_STD_CNF:
$ TYPE SYS$INPUT
  The standard configuration performs the following:

   *  Places files in SYS$COMMON:[GTM_DIST] with SYSTEM as owner
   *  Copies the GT.CX command procedures to SYS$MANAGER
   *  Adds GTCXSTART.COM to the system startup database
   *  Leaves GT.CX help files in GTM$DIST (does not move them to SYS$HELP)
   *  Sets up GTCXSTART.COM to start the CCP under the SYSTEM UIC
   *  Starts the GT.CX CCP at the end of the installation

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
  Usual practice is to place a system component such as GT.CX  on  the  system
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
  The disk must be mounted, on-line and have adequate space to hold the  GT.CX
  distribution.  The disk name may be physical or logical.

$ EXIT
$HELP_DST_DIR:
$ TYPE SYS$INPUT
  This  directory  holds  the  distribution.    If  it  does  not  exist,  the
  installation creates it with WORLD=RE access.

$ EXIT
$HELP_CCP_OWN:
$ TYPE SYS$INPUT
  The CCP usually must have broad file access so running under the  SYSTEM UIC
  may make sense.  Alternatives involve using a distinguished  UIC  which  has
  appropriate UIC or ACL based access to clustered files.  This UIC MUST be in
  Group 1.

$ EXIT
$HELP_STARTDB:
$ TYPE SYS$INPUT
  Answering yes causes the installation to place GTCXSTART.COM in the  startup
  database so the system startup automatically sets up  the  GT.CX environment
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
$HELP_HLP_DIR:
$ TYPE SYS$INPUT
  You may place the GT.CX help files in SYS$HELP or leave them with  the  rest
  of the distribution.

$ EXIT
$HELP_RUN_IVP:
$ TYPE SYS$INPUT
  This installation kit contains an installation verification procedure  (IVP)
  which you can run as part of the installation to verify the  correctness  of
  the software.   Note that if you choose this option, the  GT.M  images  must
  already be installed.

$ EXIT
$HELP_START_CCP:
$ TYPE SYS$INPUT
  Answering yes causes the installation to start the CCP.

$ EXIT
