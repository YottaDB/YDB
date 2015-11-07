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
$!      HELP TEXT PROCESSING FOR GT.M FOCUS INTERFACE KITINSTAL.COM
$!      COPYRIGHT 1989 - 2000 Sanchez Computer Associates
$ IF F$EXTRACT(0,5,P1) .EQS. "HELP_" THEN GOTO 'P1'
$ EXIT VMI$_UNSUPPORTED
$HELP_PURGE:
$ TYPE SYS$INPUT

  If the GT.M FOCUS interface is previously installed, there is no  reason  to
  keep older versions of the software online, unless you wish to  test  before
  purging.

$ EXIT
$HELP_STD_CNF:
$ TYPE SYS$INPUT
  The standard configuration performs the following:

   *  Places files in SYS$COMMON:[GTM_DIST] with SYSTEM as owner.
   *  Defines the DSM_API logical name.
   *  Defines the logical name GTMAPI$FCS$nod to GTM$DIST:FOCUS.GLD, where nod
      is the first three characters of the node name.  This makes a connection
      between a UCI of FCS and a volumeset of nod and the FOCUS.GLD.
   *  Copies the GT.M FOCUS interface command procedure to SYS$MANAGER.
   *  Adds GTMFISTART.COM to the system startup database.
   *  Sets up the GT.M FOCUS interface at the end of the installation.

  If the SYSTEM id is not set up, the installation will use [1,4].

$ EXIT
$HELP_DST_OWN:
$ TYPE SYS$INPUT
  Provide a UIC, normally SYSTEM, to own the files in the GT.M FOCUS interface
  distribution.  The UIC can be a name, a group name and a user name separated
  by a comma,  or a pair of octal codes separated by  a  comma  which  specify
  group and user.

$ EXIT
$HELP_SYS_DST:
$ TYPE SYS$INPUT
  Usual practice is  to  place  a  system  component  such  as  the GT.M FOCUS
  interface on the system disk.  If you have severe space constraints, you may
  need to use another volume.

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
  The disk must be mounted,  on-line and  have  adequate  space  to  hold  the
  GT.M FOCUS interface distribution. The disk name may be physical or logical.

$ EXIT
$HELP_DST_DIR:
$ TYPE SYS$INPUT
  This  directory  holds  the  distribution.    If  it  does  not  exist,  the
  installation creates it with WORLD=RE access.

$ EXIT
$HELP_GBLDIR:
$ TYPE SYS$INPUT
  The GT.M FOCUS interface requires one or more Global Directories in order to
  find the appropriate database files.   Each Global Directory  is  associated
  with  a  "UCI"  "Volume-set"  pair  by  a  logical  name  in  the  form   of
  GTMAPI$uci$vol where uci and vol are each 3  characters  long.    GTMFISTART
  will define these logical names for you if you provide it with a list in the
  form uci*vol*global-directory as P1.   GTMFISTART defaults this  list  to  a
  single element where vol is the first three characters of the node-name, and
  the uci and global directory are selected during the installation.  Remember
  that  if  the  global  directory file-specification contains a logical name,
  the logical must be accessible to a detached process.

$ EXIT
$HELP_UCI_NAME:
$ TYPE SYS$INPUT
  The GT.M FOCUS interface requires one or more Global Directories in order to
  find the appropriate database files.   Each Global Directory  is  associated
  with  a  "UCI"  "Volume-set"  pair  by  a  logical  name  in  the  form   of
  GTMAPI$uci$vol where uci and vol are each 3  characters  long.    GTMFISTART
  will define these logical names for you if you provide it with a list in the
  form uci*vol*global-directory as P1.   GTMFISTART defaults  this  list to  a
  single element where vol is the first three characters of the node-name, and
  the uci and global directory are selected during the installation.

$ EXIT
$HELP_STARTDB:
$ TYPE SYS$INPUT
  Answering yes  causes  the  installation  to  place  GTMFISTART.COM  in  the
  startup database so the system startup automatically sets up the  GT.M FOCUS
  interface whenever the system boots.

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
  already be installed.

$ EXIT
$HELP_START_GTMFI:
$ TYPE SYS$INPUT
  Answering yes causes the installation to set up the GT.M FOCUS interface.

$ EXIT
