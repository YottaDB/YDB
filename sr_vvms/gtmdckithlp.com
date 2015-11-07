$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!								!
$!	Copyright 2005 Fidelity Information Services, Inc	!
$!								!
$!	This source code contains the intellectual property	!
$!	of its copyright holder(s), and is made available	!
$!	under a license.  If you do not know the terms of	!
$!	the license, please stop and do not read further.	!
$!								!
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$!
$!      HELP TEXT PROCESSING FOR GT.M DBCERTIFY utility KITINSTAL.COM
$ IF F$EXTRACT(0,5,P1) .EQS. "HELP_" THEN GOTO 'P1'
$ EXIT VMI$_UNSUPPORTED
$HELP_PURGE:
$ TYPE SYS$INPUT

  If the GT.M DBCERTIFY utility is previously installed, there is no reason to
  keep older versions of the software online, unless you wish to test before
  purging.

$ EXIT
$HELP_STD_CNF:
$ TYPE SYS$INPUT
  The standard configuration performs the following:

   *  Places files in SYS$COMMON:[GTMDBCERTIFY] with SYSTEM as owner.
   *  Copies two GT.M DBCERTIFY utility command procedures to the
      above directory and SYS$MANAGER:.
   *  Uses the existing GT.M distribution from GTM$DIST:.
   *  Builds V5CBSU.EXE using the GT.M from GTM$DIST:.

  If the SYSTEM id is not set up, the installation will use [1,4].

$ EXIT
$HELP_DST_OWN:
$ TYPE SYS$INPUT
  Provide a UIC, normally SYSTEM, to own the files in the GT.M DBCERTIFY utility
  distribution.  The UIC can be a name, a group name and a user name separated
  by a comma,  or a pair of octal codes separated by a comma which specify
  group and user.

$ EXIT
$HELP_SYS_DST:
$ TYPE SYS$INPUT
  Usual practice is to place a system component such as the GT.M DBCERTIFY
  utility on the system disk.  If you have severe space constraints, you may
  need to use another volume.

$ EXIT
$HELP_SYS_DIR:
$ TYPE SYS$INPUT
  This directory  becomes a sub-directory of  SYS$COMMON  and   holds   the
  distribution.   If it does not  exist,  the installation creates it  with
  WORLD=RE access.   If you  are  not  concerned with  mixing software from
  different vendors, you may wish to use SYSLIB.

$ EXIT
$HELP_DST_DEV:
$ TYPE SYS$INPUT
  The disk must be mounted,  on-line and  have  adequate  space  to  hold  the
  GT.M DBCERTIFY utility distribution. The disk name may be physical or logical.

$ EXIT
$HELP_DST_DIR:
$ TYPE SYS$INPUT
  This  directory  holds  the  distribution.    If  it  does  not  exist,  the
  installation creates it with WORLD=RE access.

$ EXIT
$HELP_GTM_LOG:
$ TYPE SYS$INPUT
  The GT.M DBCERTIFY utility invokes utilities from the existing GT.M
  distribution.  The location of the distribution may be specified as
  a logical name or as device:[directory].  GTM$DIST is the default.

$ EXIT
$HELP_RUN_IVP:
$ TYPE SYS$INPUT
  This installation kit does not contain an installation verification
  procedure  (IVP) which you can run as part of the installation to
  verify the  correctness  of the software.

$ EXIT
$HELP_BLD_EXE:
$ TYPE SYS$INPUT
  The executable for V5CBSU must be built before the existing databases
  can be prepared for upgrade for use with the new GT.M version.  This is
  done using the GTMDCBUILD.COM procedure created by this installation.
  Answering yes to this question will invoke this procedure at the end
  of this installation.

$ EXIT
