$!
$!      HELP TEXT PROCESSING FOR GT.M KITINSTAL.COM
$!      COPYRIGHT 1989 - 2000 Sanchez Computer Associates
$ IF F$EXTRACT(0,5,P1) .EQS. "HELP_" THEN GOTO 'P1'
$ EXIT VMI$_UNSUPPORTED
$HELP_PURGE:
$ TYPE SYS$INPUT
  If GT.M is previously installed,   there is no reason to keep older versions
  of the software online, unless you wish to test before purging.

$ EXIT
$HELP_STD_CNF:
$ TYPE SYS$INPUT
  The standard configuration performs the following:

   *  Places files in SYS$COMMON:[GTM_DIST] with SYSTEM as owner
   *  Copies the GT.M command procedures to SYS$MANAGER
   *  Adds GTMSTART.COM to the system startup database
   *  Leaves GT.M help files in GTM$DIST (does not move them to SYS$HELP)
   *  Defines DCL commands for GT.M in the system command table
   *  Defines logical names, including LNK$LIBRARY*, in the system table
   *  INSTALLs all the appropriate GT.M images during the installation
   *  Defines the default global directory as MUMPS.GLD
   *  Defines the default routine search list as "[],GTM$DIST:"
   *  Compiles the MUMPS percent routines

  If the SYSTEM id is not set up, the installation will use [1,4].
  If you answer YES,  there are no other questions.

$ EXIT
$HELP_DST_OWN:
$ TYPE SYS$INPUT
  Provide a UIC,  normally SYSTEM, to own the files in the GT.M  distribution.
  The UIC can be a name,  a group name and a user name separated by  a  comma,
  or a pair of octal codes separated by a comma which specify group and user.

$ EXIT
$HELP_SYS_DST:
$ TYPE SYS$INPUT
  Usual practice is to place a language processor, such as GT.M, on the system
  disk.   If you have severe space constraints,  you may need to  use  another
  volume.

$ EXIT
$HELP_SYS_DIR:
$ TYPE SYS$INPUT
  This directory becomes a  sub-directory  of  SYS$COMMON,   and   holds   the
  distribution.   If it does not  exist,  the  installation  creates  it  with
  WORLD=RE access.   If you  are  not  concerned  with  mixing  software  from
  different vendors, you may wish to use SYSLIB.

$ EXIT
$HELP_DST_DEV:
$ TYPE SYS$INPUT
  The disk must be mounted, on-line,  and have adequate space to hold the GT.M
  distribution.  The disk name may be physical or logical.

$ EXIT
$HELP_DST_DIR:
$ TYPE SYS$INPUT
  This  directory  holds  the  distribution.    If  it  does  not  exist,  the
  installation creates it with WORLD=RE access. 

$ EXIT
$HELP_STARTDB:
$ TYPE SYS$INPUT
  Answering yes causes the installation to place GTMSTART.COM in  the  startup
  database so the system startup automatically sets up  the  GT.M  environment
  whenever the system boots.

$ EXIT
$HELP_MGR_COM:
$ TYPE SYS$INPUT
  You may prevent the installation from moving the .COM files to  SYS$MANAGER.
  Copying the command  procedures  to  SYS$MANAGER  allows  system startup  to
  access them through  the  VMS  startup  database  and  generally  simplifies
  operations.  However, if you wish to have multiple versions of GT.M on  your
  system at the same time,  multiple  copies  of  the  command  procedures  in 
  SYS$MANAGER would require intervention to provide alternative naming.

$ EXIT
$HELP_HLP_DIR:
$ TYPE SYS$INPUT
  You may place the GT.M help files in SYS$HELP or leave them with the rest of
  the distribution.

$ EXIT
$HELP_DEF_DCL:
$ TYPE SYS$INPUT
  Common practice is (YES) to define commands in  the  system  command  tables
  (SYS$LIBRARY:DCLTABLES.EXE).  A less efficient, but sometimes more flexible,
  alternative, has each process define them as it starts.  Another alternative
  uses alternate command tables.  GT.M uses the MUMPS command.

$ EXIT
$HELP_DEF_SYS:
$ TYPE SYS$INPUT
  Common practice is (YES) to define widely used logical  names in the  SYSTEM
  logical  name  table.     A  less  efficient  but  sometimes  more  flexible
  alternative has each process define them as it starts.   Other  alternatives
  use definitions in one or more GROUP logical name  tables,  or  in a logical
  name table other than those supplied with VMS.

$ EXIT
$HELP_LNK_LOG:
$ TYPE SYS$INPUT
  LNK$LIBRARYn logical names  point to  libraries  included  by  default  when
  performing an image LINK under VMS.   GT.M links require an object  library,
  GTMLIB.OLB,  and  a  shareable  image  library,  GTMSHR.OLB.   Adding  these
  libraries to your list of LNK$LIBRARY logical  names  simplifies  your  LINK
  commands for GT.M.  If you answer YES, then the installation procedure finds
  the next two available LNK$LIBRARYn logical  names  FOR  THIS  PROCESS,  and  
  defines them so they point to the appropriate GT.M libraries.

$ EXIT
$HELP_RUN_IVP:
$ TYPE SYS$INPUT
  This installation kit contains an installation verification procedure  (IVP)
  which you can run as part of the installation to verify the  correctness  of 
  the software.   Note that if you choose this option,  the  GT.M  images  are
  automatically INSTALLed.

$ EXIT
$HELP_PCT_RTN:
$ TYPE SYS$INPUT
  The GT.M Mumps System  is  distributed  with  a  set  of  programmer  tools,
  so-called  percent  (%) routines.   If you answer YES to this question,  the 
  installation compiles these routines.  Note that if you choose this  option,
  the GT.M images are automatically INSTALLed.

$ EXIT
$HELP_INSTALL:
$ TYPE SYS$INPUT
  In order to run GT.M,  certain images must be INSTALLed in your system  with
  the INSTALL utility.   If you answer YES,  then these images  are  INSTALLed 
  during  this  installation procedure.   You  may  wish  to  defer  this  for 
  operational reasons, or until you have completed a brief evaluation.

$ EXIT
$HELP_DEF_RTN:
$ TYPE SYS$INPUT
  GTM$ROUTINES is a logical name for your MUMPS routine search list.   If  you
  answer YES,  the installation adds a default definition for this search list 
  to GTMLOGICALS.COM.

$ EXIT
$HELP_RTN_DIR:
$ TYPE SYS$INPUT
  The installation uses this string as the  definition  for  the  GTM$ROUTINES
  search list.   The directories specified do not need to exist at this  time.  
  This definition can be easily changed later.   Because GTM$ROUTINES provides
  a string to GT.M and does not act as a VMS search list,  you  should enclose 
  the items in the list with quotes (") when the list has more than one item.


$ EXIT
$HELP_DEF_GLD:
$ TYPE SYS$INPUT
  GTM$GBLDIR is a logical name for your global directory.  If you answer  YES,
  the installation adds a default  definition  for  the  global  directory  to
  GTMLOGICALS.COM.

$ EXIT
$HELP_GBL_DIR:
$ TYPE SYS$INPUT
  The installation uses this string  as  the  definition  for  the  GTM$GBLDIR
  global directory name.  This file specification may be a partial or complete
  file specification.  The file does not need to exist at this time. A partial
  file specification is usually used for development, and a complete  one  for
  production.  This definition can be easily changed later.

$ EXIT
