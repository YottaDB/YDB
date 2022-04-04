/****************************************************************
 *								*
<<<<<<< HEAD:sr_port/merrors_ctl.c
 * Copyright (c) 2001-2024 Fidelity National Information	*
=======
 * Copyright (c) 2001-2022 Fidelity National Information 	*
>>>>>>> eb3ea98c (GT.M V7.0-002):sr_i386/merrors_ctl.c
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

LITDEF	err_msg merrors[] = {
	{ "ACK", "", 0, 0 },
	{ "BREAKZST", "Break instruction encountered during ZSTEP action", 0, 0 },
	{ "BADACCMTHD", "Invalid access method was specified, file not created", 0, 0 },
	{ "BADJPIPARAM", "!AD is not a legal parameter for $ZGETJPI()", 2, 0 },
	{ "BADSYIPARAM", "!AD is not a legal parameter for $ZGETSYI()", 2, 0 },
	{ "BITMAPSBAD", "Database bit maps are incorrect", 0, 0 },
	{ "BREAK", "Break instruction encountered", 0, 0 },
	{ "BREAKDEA", "Break instruction encountered during Device error action", 0, 0 },
	{ "BREAKZBA", "Break instruction encountered during ZBREAK action", 0, 0 },
	{ "STATCNT", "!AD:!_  Key cnt: !UL  max subsc len: !UL  max data len: !UL", 5, 0 },
	{ "BTFAIL", "The database block table is corrupt; error type !UL", 1, 0 },
	{ "MUPRECFLLCK", "Database file !AD is locked by MUPIP RECOVER.  Could not secure access.", 2, 0 },
	{ "CMD", "Command expected but not found", 0, 0 },
	{ "COLON", "Colon (:) expected in this context", 0, 0 },
	{ "COMMA", "Comma expected in this context", 0, 0 },
	{ "COMMAORRPAREXP", "Comma or right parenthesis expected but not found", 0, 0 },
	{ "COMMENT", "Comment line.  Placed zbreak at next executable line.", 0, 0 },
	{ "CTRAP", "Character trap $C(!UL) encountered", 1, 0 },
	{ "CTRLC", "CTRL_C encountered", 0, 0 },
	{ "CTRLY", "User interrupt encountered", 0, 0 },
	{ "DBCCERR", "Interlock instruction failure in critical mechanism for region !AD", 2, 0 },
	{ "DUPTOKEN", "Token 0x!16@XQ is duplicate in the journal file !AD for database !AD", 5, 0 },
	{ "DBJNLNOTMATCH", "Database !AD points to journal file name !AD but the journal file points to database file !AD", 6, 0 },
	{ "DBFILERR", "Error with database file !AD", 2, 0 },
	{ "DBNOTGDS", "!AD - Unrecognized database file format", 2, 1 },
	{ "DBOPNERR", "Error opening database file !AD", 2, 0 },
	{ "DBRDERR", "Cannot read database file !AD after opening", 2, 0 },
	{ "CCEDUMPNOW", "", 0, 0 },
	{ "DEVPARINAP", "Device parameter inappropriate to this command", 0, 0 },
	{ "RECORDSTAT", "!AD:!_  Key cnt: !@ZQ  max subsc len: !UL  max rec len: !UL  max node len: !UL", 6, 0 },
	{ "NOTGBL", "Expected a global variable name starting with an up-arrow (^): !AD", 2, 0 },
	{ "DEVPARPROT", "The protection specification is invalid", 0, 0 },
	{ "PREMATEOF", "Premature end of file detected", 0, 0 },
	{ "GVINVALID", "!_!AD!/!_!_!_Invalid global name", 2, 0 },
	{ "DEVPARTOOBIG", "String deviceparameter exceeds 255 character limit", 0, 0 },
	{ "DEVPARUNK", "Deviceparameter unknown", 0, 0 },
	{ "DEVPARVALREQ", "A value is required for this device parameter", 0, 0 },
	{ "DEVPARMNEG", "Deviceparameter must be a positive value", 0, 0 },
	{ "DSEBLKRDFAIL", "Failed attempt to read block", 0, 0 },
	{ "DSEFAIL", "DSE failed.  Failure code: !AD.", 2, 0 },
	{ "NOTALLREPLON", "Replication off for !AD regions", 2, 0 },
	{ "BADLKIPARAM", "!AD is not a legal parameter for $ZGETLKI()", 2, 0 },
	{ "JNLREADBOF", "Beginning of journal file encountered for !AD", 2, 0 },
	{ "DVIKEYBAD", "$ZGETDVI(\"!AD\",\"!AD\") contains an illegal keyword", 4, 0 },
	{ "ENQ", "", 0, 0 },
	{ "EQUAL", "Equal sign expected but not found", 0, 0 },
	{ "ERRORSUMMARY", "Errors occurred during compilation", 0, 0 },
	{ "ERRWEXC", "Error while processing exception string", 0, 0 },
	{ "ERRWIOEXC", "Error while processing I/O exception string", 0, 0 },
	{ "ERRWZBRK", "Error while processing ZBREAK action string", 0, 0 },
	{ "ERRWZTRAP", "Error while processing $ZTRAP", 0, 0 },
	{ "NUMUNXEOR", "!_!AD!/!_!_!_unexpected end of record in numeric subscript", 2, 0 },
	{ "EXPR", "Expression expected but not found", 0, 0 },
	{ "STRUNXEOR", "!_!AD!/!_!_!_unexpected end of record in string subscript", 2, 0 },
	{ "JNLEXTEND", "Journal file extension error for file !AD", 2, 0 },
	{ "FCHARMAXARGS", "Argument count of $CHAR function exceeded the maximum of 255", 0, 0 },
	{ "FCNSVNEXPECTED", "Function or special variable expected in this context", 0, 0 },
	{ "FNARGINC", "Format specifiers to $FNUMBER are incompatible: \"!AD\"", 2, 0 },
	{ "JNLACCESS", "Error accessing journal file !AD", 2, 0 },
	{ "TRANSNOSTART", "ZTCOMMIT(s) issued without corresponding ZTSTART(s)", 0, 0 },
	{ "FNUMARG", "$FNUMBER format specifier \"!AD\" contains an illegal character: \"!AD\"", 4, 0 },
	{ "FOROFLOW", "FOR commands nested more than !UL deep on a line", 1, 0 },
	{ "YDIRTSZ", "Improper size of YDIRT data: !UL", 1, 0 },
	{ "JNLSUCCESS", "!AD successful", 2, 0 },
	{ "GBLNAME", "Either an identifier or a left parenthesis is expected after a ^ in this context", 0, 0 },
	{ "GBLOFLOW", "Database file !AD is full", 2, 0 },
	{ "CORRUPT", "Corrupt input in Blk # !UL, Key #!UL; resuming with next global block", 2, 0 },
	{ "GTMCHECK", "Internal YottaDB error--Report to your YottaDB Support Channel", 0, 0 },
	{ "GVDATAFAIL", "Global variable $DATA function failed.  Failure code: !AD.", 2, 0 },
	{ "EORNOTFND", "!_!AD!/!_!_!_End of record not found", 2, 0 },
	{ "GVGETFAIL", "Global variable retrieval failed.  Failure code: !AD.", 2, 0 },
	{ "GVIS", "!_!_Global variable: !AD", 2, 0 },
	{ "GVKILLFAIL", "Global variable kill failed.  Failure code: !AD.", 2, 0 },
	{ "GVNAKED", "Illegal naked global reference", 0, 0 },
	{ "BACKUPDBFILE", "DB file !AD backed up in file !AD", 4, 0 },
	{ "GVORDERFAIL", "Global variable $ORDER or $NEXT function failed.  Failure code: !AD.", 2, 0 },
	{ "GVPUTFAIL", "Global variable put failed.  Failure code: !AD.", 2, 0 },
	{ "PATTABSYNTAX", "Error in !AD at line !UL", 3, 0 },
	{ "GVSUBOFLOW", "Maximum combined length of subscripts exceeded", 0, 0 },
	{ "GVUNDEF", "Global variable undefined: !AD", 2, 0 },
	{ "TRANSNEST", "Maximum transaction nesting levels exceeded", 0, 0 },
	{ "INDEXTRACHARS", "Indirection string contains extra trailing characters", 0, 0 },
	{ "CORRUPTNODE", "Corrupt input in Record #!UL, Key #!UL; resuming with next global node", 2, 0 },
	{ "INDRMAXLEN", "Maximum length !UL of an indirection argument was exceeded", 1, 0 },
	{ "UNUSEDMSG268", "INSFFBCNT nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "INTEGERRS", "Database integrity errors", 0, 0 },
	{ "INVCMD", "Invalid command keyword encountered", 0, 0 },
	{ "INVFCN", "Invalid function name", 0, 0 },
	{ "INVOBJ", "Cannot ZLINK object file due to unexpected format", 0, 0 },
	{ "INVSVN", "Invalid special variable name", 0, 0 },
	{ "IOEOF", "Attempt to read past an end-of-file", 0, 0 },
	{ "IONOTOPEN", "Attempt to USE an I/O device which has not been opened", 0, 0 },
	{ "MUPIPINFO", "!AD", 2, 0 },
	{ "UNUSEDMSG277", "IVTIME nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "JOBFAIL", "JOB command failure", 0, 0 },
	{ "JOBLABOFF", "Label and offset not found in created process", 0, 0 },
	{ "JOBPARNOVAL", "This job parameter cannot take a value", 0, 0 },
	{ "JOBPARNUM", "The value of this job parameter must be an integer", 0, 0 },
	{ "JOBPARSTR", "The value of this job parameter must be a string", 0, 0 },
	{ "JOBPARUNK", "Job parameter unknown", 0, 0 },
	{ "JOBPARVALREQ", "A value is required for this job parameter", 0, 0 },
	{ "JUSTFRACT", "Fraction specifier to $JUSTIFY cannot be negative", 0, 0 },
	{ "KEY2BIG", "Key size (!UL) is greater than maximum (!UL) for region: !AD", 4, 0 },
	{ "LABELEXPECTED", "Label expected in this context", 0, 0 },
	{ "LABELMISSING", "Label referenced but not defined: !AD", 2, 0 },
	{ "LABELUNKNOWN", "Label referenced but not defined", 0, 0 },
	{ "DIVZERO", "Attempt to divide by zero", 0, 0 },
	{ "LKNAMEXPECTED", "An identifier is expected after a ^ in this context", 0, 0 },
	{ "JNLRDERR", "Error reading journal file !AD.  Unable to initialize.", 2, 0 },
	{ "LOADRUNNING", "Cannot ZLINK an active routine !AD", 2, 0 },
	{ "LPARENMISSING", "Left parenthesis expected", 0, 0 },
	{ "LSEXPECTED", "A line separator is expected here", 0, 0 },
	{ "LVORDERARG", "Argument to local variable $NEXT must be subscripted", 0, 0 },
	{ "MAXFORARGS", "Maximum number of arguments to a single FOR command exceeded", 0, 0 },
	{ "TRANSMINUS", "Negative numbers not allowed with ZTCOMMIT", 0, 0 },
	{ "MAXNRSUBSCRIPTS", "Maximum number of subscripts exceeded", 0, 0 },
	{ "MAXSTRLEN", "Maximum string length exceeded", 0, 0 },
	{ "ENCRYPTCONFLT2", "A concurrent MUPIP REORG -ENCRYPT changed the encryption key for !AD before the process could initialize it. !AD", 4, 0 },
	{ "JNLFILOPN", "Error opening journal file !AD for database file !AD", 4, 0 },
	{ "MBXRDONLY", "Mailbox is read only, cannot write to it", 0, 0 },
	{ "JNLINVALID", "!AD is not a valid journal file !/ for database file: !AD", 4, 0 },
	{ "MBXWRTONLY", "Mailbox is write only, cannot read from it", 0, 0 },
	{ "MEMORY", "Central memory exhausted during request for !UJ bytes from 0x!XJ", 2, 0 },
	{ "DONOBLOCK", "Argumentless DO not followed by a block", 0, 0 },
	{ "ZATRANSCOL", "The collation requested has no implementation for the requested operation", 0, 0 },
	{ "VIEWREGLIST", "$VIEW() only handles the first region subparameter", 0, 0 },
	{ "NUMERR", "Error: cannot convert !AD value to decimal or hexadecimal number", 2, 0 },
	{ "NUM64ERR", "Error: cannot convert !AD value to 64 bit decimal or hexadecimal number", 2, 0 },
	{ "UNUM64ERR", "Error: cannot convert !AD value to 64 bit unsigned decimal or hexadecimal number", 2, 0 },
	{ "HEXERR", "Error: cannot convert !AD value to hexadecimal number", 2, 0 },
	{ "HEX64ERR", "Error: cannot convert !AD value to 64 bit hexadecimal number", 2, 0 },
	{ "CMDERR", "Error running command : !AD", 2, 0 },
	{ "BACKUPSUCCESS", "Backup completed successfully", 0, 0 },
	{ "JNLTMQUAL3", "Time qualifier BEFORE_TIME=\"!AZ\" is less than the journal file(s) minimum timestamp=\"!AZ\"", 2, 0 },
	{ "MULTLAB", "This label has been previously defined", 0, 0 },
	{ "GTMCURUNSUPP", "The requested operation is unsupported in this version of YottaDB", 0, 0 },
	{ "CCEDUMPOFF", "", 0, 0 },
	{ "NOPLACE", "Line specified in a ZBREAK cannot be found", 0, 0 },
	{ "JNLCLOSE", "Error closing journal file !AD", 2, 0 },
	{ "NOTPRINCIO", "Output currently directed to device !AD", 2, 0 },
	{ "NOTTOEOFONPUT", "Not positioned to EOF on write (sequential organization only)", 0, 0 },
	{ "NOZBRK", "No zbreak at that location", 0, 0 },
	{ "NULSUBSC", "!AD Null subscripts are not allowed for database file: !AD", 4, 4 },
	{ "NUMOFLOW", "Numeric overflow", 0, 0 },
	{ "PARFILSPC", "Parameter: !AD  file specification: !AD", 4, 0 },
	{ "PATCLASS", "Illegal character class for pattern code", 0, 0 },
	{ "PATCODE", "Illegal syntax for pattern", 0, 0 },
	{ "PATLIT", "Illegal character or unbalanced quotes for pattern literal", 0, 0 },
	{ "PATMAXLEN", "Pattern code exceeds maximum length", 0, 0 },
	{ "LPARENREQD", "!_!AD!/!_!_!_Left parenthesis expected", 2, 0 },
	{ "PATUPPERLIM", "Pattern code upper limit is less than lower limit", 0, 0 },
	{ "PCONDEXPECTED", "Post-conditional expression expected but not found", 0, 0 },
	{ "PRCNAMLEN", "Process name !AD length is greater than !SL", 3, 0 },
	{ "RANDARGNEG", "Random number generator argument must be greater than or equal to one", 0, 0 },
	{ "DBPRIVERR", "No privilege for attempted update operation for file: !AD", 2, 0 },
	{ "REC2BIG", "Record size (!UL) is greater than maximum (!UL) for region: !AD", 4, 0 },
	{ "RHMISSING", "Right-hand side of expression expected", 0, 0 },
	{ "DEVICEREADONLY", "Cannot write to read-only device", 0, 0 },
	{ "COLLDATAEXISTS", "Collation type cannot be changed while !AD!AD data exists", 4, 0 },
	{ "ROUTINEUNKNOWN", "Routine could not be found", 0, 0 },
	{ "RPARENMISSING", "Right parenthesis expected", 0, 0 },
	{ "RTNNAME", "Routine name expected here", 0, 0 },
	{ "VIEWGVN", "Invalid global key name used with VIEW/$VIEW(): !AD", 2, 0 },
	{ "RTSLOC", "!_!_At M source location !AD", 2, 0 },
	{ "RWARG", "This is not a legal argument for a READ command", 0, 0 },
	{ "RWFORMAT", "A valid format expression (!!, #, or ?expr) expected here", 0, 0 },
	{ "JNLWRTDEFER", "Journal write start deferred", 0, 0 },
	{ "SELECTFALSE", "No argument to $SELECT was true", 0, 0 },
	{ "SPOREOL", "Either a space or an end-of-line was expected but not found", 0, 0 },
	{ "SRCLIN", "!_!AD!/!_!AD", 4, 0 },
	{ "SRCLOC", "!_!_At column !UL, line !UL, source module !AD", 4, 0 },
	{ "RLNKRECNFL", "Conflict on relinkctl file !AZ for $ZROUTINES directory !AD, running an integrity check", 3, 0 },
	{ "STACKCRIT", "Stack space critical", 0, 0 },
	{ "STACKOFLOW", "Stack overflow", 0, 0 },
	{ "STACKUNDERFLO", "Stack underflow", 0, 0 },
	{ "STRINGOFLOW", "String pool overflow", 0, 0 },
	{ "SVNOSET", "Cannot SET this special variable", 0, 0 },
	{ "VIEWFN", "View parameter !AD is not valid with the $VIEW() function", 2, 0 },
	{ "TERMASTQUOTA", "Process AST quota exceeded, cannot open terminal", 0, 0 },
	{ "TEXTARG", "Invalid argument to $TEXT function", 0, 0 },
	{ "TMPSTOREMAX", "Maximum space for temporary values exceeded", 0, 0 },
	{ "VIEWCMD", "View parameter !AD is not valid with the VIEW command", 2, 0 },
	{ "JNI", "!AD", 2, 0 },
	{ "TXTSRCFMT", "$TEXT encountered an invalid source program file format", 0, 0 },
	{ "UIDMSG", "Unidentified message received", 0, 0 },
	{ "UIDSND", "Unidentified sender PID", 0, 0 },
	{ "LVUNDEF", "Undefined local variable: !AD", 2, 0 },
	{ "UNIMPLOP", "Unimplemented construct encountered", 0, 0 },
	{ "VAREXPECTED", "Variable expected in this context", 0, 0 },
	{ "BACKUPFAIL", "MUPIP cannot start backup with the above errors", 0, 0 },
	{ "MAXARGCNT", "Maximum number of arguments !UL exceeded", 1, 0 },
	{ "GTMSECSHRSEMGET", "semget error errno = !UL", 1, 0 },
	{ "VIEWARGCNT", "View parameter !AD has inappropriate number of subparameters", 2, 0 },
	{ "GTMSECSHRDMNSTARTED", "gtmsecshr daemon started (key: 0x!XL) for version !AD from !AD", 5, 0 },
	{ "ZATTACHERR", "Error attaching to \"!AD\"", 2, 0 },
	{ "ZDATEFMT", "$ZDATE format string contains invalid character", 0, 0 },
	{ "ZEDFILSPEC", "Illegal ZEDIT file specification: !AD", 2, 0 },
	{ "ZFILENMTOOLONG", "!AD is longer than 255 characters", 2, 0 },
	{ "ZFILKEYBAD", "!AD is not a legal keyword for $ZFILE()", 2, 0 },
	{ "ZFILNMBAD", "!AD is not a legal file name", 2, 0 },
	{ "ZGOTOLTZERO", "Cannot ZGOTO a level less than zero", 0, 0 },
	{ "ZGOTOTOOBIG", "Cannot ZGOTO a level greater than present level", 0, 0 },
	{ "ZLINKFILE", "Error while zlinking \"!AD\"", 2, 0 },
	{ "ZPARSETYPE", "Illegal TYPE argument to $ZPARSE(): !AD", 2, 0 },
	{ "ZPARSFLDBAD", "Illegal $ZPARSE() field parameter: !AD", 2, 0 },
	{ "ZPIDBADARG", "The tvexpr must be FALSE if last $ZPID() not found", 0, 0 },
	{ "UNUSEDMSG390", "ZPRIVARGBAD nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "UNUSEDMSG391", "ZPRIVSYNTAXERR nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "ZPRTLABNOTFND", "Label not found in routine", 0, 0 },
	{ "VIEWAMBIG", "View parameter !AD is ambiguous", 2, 0 },
	{ "VIEWNOTFOUND", "View parameter !AD not valid", 2, 0 },
	{ "UNUSEDMSG395", "ZSETPRVARGBAD nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "INVSPECREC", "!AD Invalid global modifier record", 2, 0 },
	{ "UNUSEDMSG397", "ZSETPRVSYNTAX nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "ZSRCHSTRMCT", "Search stream identifier out of range", 0, 0 },
	{ "VERSION", "Version mismatch - This program must be recompiled", 0, 0 },
	{ "MUNOTALLSEC", "WARNING: not all global sections accessed were successfully rundown", 0, 0 },
	{ "MUSECDEL", "Section !AD deleted", 2, 0 },
	{ "MUSECNOTDEL", "Section !AD not deleted", 2, 0 },
	{ "RPARENREQD", "!_!AD!/!_!_!_Right parenthesis expected", 2, 0 },
	{ "ZGBLDIRACC", "Cannot access global directory !AD!AD!AD.", 6, 0 },
	{ "GVNAKEDEXTNM", "Cannot reference different global directory in a naked reference", 0, 0 },
	{ "EXTGBLDEL", "Invalid delimiter for extended global syntax", 0, 0 },
	{ "DSEWCINITCON", "No action taken, enter YES at CONFIRMATION prompt to initialize global buffers", 0, 0 },
	{ "LASTFILCMPLD", "The file currently being compiled is !AD", 2, 0 },
	{ "NOEXCNOZTRAP", "Neither an exception nor a Ztrap is specified", 0, 0 },
	{ "UNSDCLASS", "Unsupported descriptor class", 0, 0 },
	{ "UNSDDTYPE", "Unsupported descriptor data type", 0, 0 },
	{ "ZCUNKTYPE", "External call: Unknown argument type", 0, 0 },
	{ "ZCUNKMECH", "External call: Unknown parameter-passing mechanism", 0, 0 },
	{ "ZCUNKQUAL", "External call: Unknown input qualifier", 0, 0 },
	{ "JNLDBTNNOMATCH", "Journal file !AD has !AD transaction number [0x!16@XQ], but database !AD has current transaction number [0x!16@XQ] and journal end transaction number [0x!16@XQ]", 9, 0 },
	{ "ZCALLTABLE", "External call Table format error", 0, 0 },
	{ "ZCARGMSMTCH", "External call: Actual argument count, !UL, is greater than formal argument count, !UL", 2, 0 },
	{ "ZCCONMSMTCH", "External call: Too many input arguments", 0, 0 },
	{ "ZCOPT0", "External call: Qualifier OPTIONAL_0 can be used only with mechanisms REFERENCE or DESCRIPTOR", 0, 0 },
	{ "UNUSEDMSG420", "ZCSTATUS nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "UNUSEDMSG421", "ZCUSRRTN nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "ZCPOSOVR", "External call: Invalid overlapping of arguments in table position !UL", 1, 0 },
	{ "ZCINPUTREQ", "External call: Required input argument missing", 0, 0 },
	{ "JNLTNOUTOFSEQ", "End transaction [0x!16@XQ] of journal !AD different from Begin transaction [0x!16@XQ] of next generation journal !AD", 6, 0 },
	{ "ACTRANGE", "Alternate Collating Type !UL is out of range", 1, 0 },
	{ "ZCCONVERT", "External call: error converting output argument from external call !AD", 2, 0 },
	{ "ZCRTENOTF", "External call routine !AD not found", 2, 0 },
	{ "GVRUNDOWN", "Error during global database rundown", 0, 0 },
	{ "LKRUNDOWN", "Error during LOCK database rundown", 0, 0 },
	{ "IORUNDOWN", "Error during image rundown", 0, 0 },
	{ "FILENOTFND", "File !AD not found", 2, 0 },
	{ "MUFILRNDWNFL", "File !AD rundown failed", 2, 0 },
	{ "JNLTMQUAL1", "Time qualifier BEFORE_TIME=\"!AZ\" is less than SINCE_TIME=\"!AZ\"", 2, 0 },
	{ "FORCEDHALT", "Image HALTed by MUPIP STOP", 0, 0 },
	{ "LOADEOF", "Load error: EOF reached prior to BEGIN record !@UQ.  No records loaded.", 1, 0 },
	{ "WILLEXPIRE", "This copy of YottaDB will expire within one week", 0, 0 },
	{ "LOADEDBG", "Load error: END smaller than BEGIN.  No records loaded.", 0, 0 },
	{ "LABELONLY", "Routine !AD was compiled for label-only entry", 2, 0 },
	{ "MUREORGFAIL", "MUPIP REORG failed.  Failure code: !AD.", 2, 0 },
	{ "GVZPREVFAIL", "Global variable $ZPREVIOUS function failed.  Failure code: !AD.", 2, 0 },
	{ "MULTFORMPARM", "This formal parameter is multiply defined", 0, 0 },
	{ "QUITARGUSE", "Quit cannot take an argument in this context", 0, 0 },
	{ "NAMEEXPECTED", "A local variable name is expected in this context", 0, 0 },
	{ "FALLINTOFLST", "Fall-through to a label with formallist is not allowed", 0, 0 },
	{ "NOTEXTRINSIC", "QUIT/ZHALT does not return to an extrinsic function: argument not allowed", 0, 0 },
	{ "GTMSECSHRREMSEMFAIL", "error removing semaphore errno = !UL", 1, 0 },
	{ "FMLLSTMISSING", "The formal list is absent from a label called with an actual list: !AD", 2, 0 },
	{ "ACTLSTTOOLONG", "More actual parameters than formal parameters: !AD", 2, 0 },
	{ "ACTOFFSET", "Actuallist not allowed with offset", 0, 0 },
	{ "MAXACTARG", "Maximum number of actual arguments exceeded", 0, 0 },
	{ "GTMSECSHRREMSEM", "[client pid !UL] Semaphore (!UL) removed", 2, 0 },
	{ "JNLTMQUAL2", "Time qualifier LOOKBACK_TIME=\"!AZ\" is later than SINCE_TIME=\"!AZ\"", 2, 0 },
	{ "GDINVALID", "Unrecognized Global Directory file format: !AD, expected label: !AD, found: !AD", 6, 0 },
	{ "ASSERT", "Assert failed in !AD line !UL for expression (!AD)", 5, 0 },
	{ "MUFILRNDWNSUC", "File !AD successfully rundown", 2, 0 },
	{ "LOADEDSZ", "Load error: END too small.  No records loaded.", 0, 0 },
	{ "QUITARGLST", "Quit cannot take a list of arguments", 0, 0 },
	{ "QUITARGREQD", "Quit from an extrinsic must have an argument", 0, 0 },
	{ "CRITRESET", "The critical section crash count for region !AD has been incremented", 2, 0 },
	{ "UNKNOWNFOREX", "Process halted by a forced exit from a source other than MUPIP", 0, 0 },
	{ "FSEXP", "File specification expected but not found", 0, 0 },
	{ "WILDCARD", "Wild cards are prohibited: !AD", 2, 0 },
	{ "DIRONLY", "Directories only are allowed in file specs: !AD", 2, 0 },
	{ "FILEPARSE", "Error parsing file specification: !AD", 2, 0 },
	{ "QUALEXP", "Qualifier expected but not found", 0, 0 },
	{ "BADQUAL", "Unrecognized qualifier: !AD", 2, 0 },
	{ "QUALVAL", "Qualifier value required but not found", 0, 0 },
	{ "ZROSYNTAX", "$ZROUTINES syntax error: !AD", 2, 0 },
	{ "COMPILEQUALS", "Error in compiler qualifiers: !AD", 2, 0 },
	{ "ZLNOOBJECT", "No object module was produced", 0, 0 },
	{ "ZLMODULE", "Object file name does not match module name: !AD", 2, 0 },
	{ "DBBLEVMX", "!AD Block level higher than maximum", 2, 0 },
	{ "DBBLEVMN", "!AD Block level less than zero", 2, 0 },
	{ "DBBSIZMN", "!AD Block too small", 2, 4 },
	{ "DBBSIZMX", "!AD Block larger than file block size", 2, 4 },
	{ "DBRSIZMN", "!AD Physical record too small", 2, 4 },
	{ "DBRSIZMX", "!AD Physical record too large", 2, 4 },
	{ "DBCMPNZRO", "!AD First record of block has nonzero compression count", 2, 4 },
	{ "DBSTARSIZ", "!AD Star record has wrong size", 2, 0 },
	{ "DBSTARCMP", "!AD Star record has nonzero compression count", 2, 3 },
	{ "DBCMPMX", "!AD Record compression count is too large", 2, 0 },
	{ "DBKEYMX", "!AD Key too long", 2, 4 },
	{ "DBKEYMN", "!AD Key too short", 2, 4 },
	{ "DBCMPBAD", "!AD Compression count not maximal", 2, 4 },
	{ "DBKEYORD", "!AD Keys out of order", 2, 4 },
	{ "DBPTRNOTPOS", "!AD Block pointer negative", 2, 3 },
	{ "DBPTRMX", "!AD Block pointer larger than file maximum", 2, 3 },
	{ "DBPTRMAP", "!AD Block pointer is a bit map block number", 2, 0 },
	{ "IFBADPARM", "External Interface Bad Parameter", 0, 0 },
	{ "IFNOTINIT", "External Interface must first call GTM$INIT or M routine", 0, 0 },
	{ "GTMSECSHRSOCKET", "!AD - !UL : Error initializing gtmsecshr socket", 3, 0 },
	{ "LOADBGSZ", "Load error: BEGIN too small.  No records loaded.", 0, 0 },
	{ "LOADFMT", "Load error: invalid format type.  Must be ZWR, GO, BINARY, or GOQ.", 0, 0 },
	{ "LOADFILERR", "Error with load file !AD", 2, 0 },
	{ "NOREGION", "REGION not found: !AD", 2, 0 },
	{ "PATLOAD", "Error loading pattern file !AD", 2, 0 },
	{ "EXTRACTFILERR", "Error with extract file !AD", 2, 0 },
	{ "FREEZE", "Region: !AD is already frozen", 2, 5 },
	{ "NOSELECT", "None of the selected variables exist -- halting", 0, 0 },
	{ "EXTRFAIL", "Extract failed for the global ^!AD. MUPIP INTEG should be run.", 2, 0 },
	{ "LDBINFMT", "Unrecognized header for load file", 0, 0 },
	{ "NOPREVLINK", "Journal file !AD has a null previous link", 2, 0 },
	{ "CCEDUMPON", "", 0, 0 },
	{ "CCEDMPQUALREQ", "A qualifier (DB,[NO]ON, or NOW) is required with the DUMP command", 0, 0 },
	{ "CCEDBDUMP", "Section !AD dumped", 2, 0 },
	{ "CCEDBNODUMP", "Section !AD not dumped; status = ", 2, 0 },
	{ "CCPMBX", "Error accessing Cluster Control Program Mailbox", 0, 0 },
	{ "REQRUNDOWN", "Error accessing database !AD.  Must be rundown on cluster node !AD.", 4, 0 },
	{ "CCPINTQUE", "Interlock failure accessing Cluster Control Program queue", 0, 0 },
	{ "CCPBADMSG", "Invalid message code received by Cluster Control Program", 0, 0 },
	{ "CNOTONSYS", "Command is not supported by this operating system", 0, 0 },
	{ "CCPNAME", "Error setting the Cluster Control Program process name", 0, 0 },
	{ "CCPNOTFND", "The Cluster Control Program is not responding", 0, 0 },
	{ "OPRCCPSTOP", "The Cluster Control Program has been halted by an operator stop request", 0, 0 },
	{ "SELECTSYNTAX", "Argument to !AD clause is not valid", 2, 0 },
	{ "LOADABORT", "Aborting load at record !UL", 1, 0 },
	{ "FNOTONSYS", "Function or special variable is not supported by this operating system", 0, 0 },
	{ "AMBISYIPARAM", "Parameter !AD is ambiguous to $ZGETSYI()", 2, 0 },
	{ "PREVJNLNOEOF", "A previous generation journal file !AD does not have valid EOF", 2, 0 },
	{ "LKSECINIT", "Error creating LOCK section for database !AD", 2, 0 },
	{ "BACKUPREPL", "Replication Instance file !AD backed up in file !AD", 4, 0 },
	{ "BACKUPSEQNO", "Journal Seqnos up to 0x!16@XQ are backed up", 1, 0 },
	{ "DIRACCESS", "Do not have full access to directory for temporary files: !AD", 2, 0 },
	{ "TXTSRCMAT", "M object module and source file do not match", 0, 0 },
	{ "CCENOGROUP", "CCE does not have GROUP privilege.  Information may be incomplete.", 0, 0 },
	{ "BADDBVER", "Incorrect database version: !AD", 2, 0 },
	{ "LINKVERSION", "This image must be relinked with the current version of YottaDB", 0, 0 },
	{ "TOTALBLKMAX", "Extension exceeds maximum total blocks.  Not extending.", 0, 0 },
	{ "LOADCTRLY", "User interrupt encountered during load.  Load halting.", 0, 0 },
	{ "CLSTCONFLICT", "Cluster conflict opening database file !AD; could not secure access.  Already open on node !AD.", 4, 0 },
	{ "SRCNAM", "in source module !AD", 2, 0 },
	{ "LCKGONE", "Lock removed: !AD", 2, 0 },
	{ "SUB2LONG", "Subscript invalid, too long", 0, 0 },
	{ "EXTRACTCTRLY", "User interrupt encountered during extract -- halting", 0, 0 },
	{ "CCENOWORLD", "CCE does not have WORLD privilege.  Information may be incomplete.", 0, 0 },
	{ "GVQUERYFAIL", "Global variable $QUERY function failed.  Failure code: !AD.", 2, 0 },
	{ "LCKSCANCELLED", "Error on remote node holding LOCKs or ZALLOCATEs.  All LOCKs and ZALLOCATEs cancelled.", 0, 0 },
	{ "INVNETFILNM", "Invalid file name following node designation in global directory", 0, 0 },
	{ "NETDBOPNERR", "Error while attempting to open database across net", 0, 0 },
	{ "BADSRVRNETMSG", "Invalid message received from GT.CM server", 0, 0 },
	{ "BADGTMNETMSG", "Invalid message sent to GT.CM server, type: 0x!XL", 1, 0 },
	{ "SERVERERR", "Severe error on server: !AD", 2, 0 },
	{ "NETFAIL", "Failure of Net operation", 0, 0 },
	{ "NETLCKFAIL", "Lock operation across Net failed", 0, 0 },
	{ "TTINVFILTER", "Invalid FILTER argument", 0, 0 },
	{ "BACKUPTN", "Transactions from 0x!16@XQ to 0x!16@XQ are backed up", 2, 0 },
	{ "WCSFLUFAIL", "Error flushing buffers -- called from module !AD at line !UL", 3, 0 },
	{ "BADTRNPARAM", "!AD is not a legal parameter to $ZTRNLNM", 2, 0 },
	{ "DSEONLYBGMM", "!AD is supported only for BG/MM access methods", 2, 0 },
	{ "DSEINVLCLUSFN", "Specified function is invalid for clustered databases", 0, 0 },
	{ "RDFLTOOSHORT", "Length specified for fixed length read less than or equal to zero", 0, 0 },
	{ "TIMRBADVAL", "Bad value specified.  Timer not changed.", 0, 0 },
	{ "CCENOSYSLCK", "CCE does not have SYSLCK privilege.  Information may be incomplete.", 0, 0 },
	{ "CCPGRP", "Error with the Cluster Control Program's group number", 0, 0 },
	{ "UNSOLCNTERR", "An unsolicited error message has been received from the network", 0, 0 },
	{ "BACKUPCTRL", "Control Y or control C encountered during backup, aborting backup", 0, 0 },
	{ "NOCCPPID", "Cannot find CCP process id", 0, 0 },
	{ "CCPJNLOPNERR", "Error opening journal file.  Database not opened.", 0, 0 },
	{ "LCKSGONE", "Locks selected for deletion removed", 0, 0 },
	{ "UNUSEDMSG560", "ZLKIDBADARG nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "DBFILOPERR", "Error doing database I/O to database file !AD", 2, 0 },
	{ "CCERDERR", "Error reading from database file !AD", 2, 0 },
	{ "CCEDBCL", "Database file !AD is clustered", 2, 0 },
	{ "CCEDBNTCL", "Database file !AD is not clustered", 2, 0 },
	{ "CCEWRTERR", "Error writing to database file !AD", 2, 0 },
	{ "CCEBADFN", "Filename error", 0, 0 },
	{ "CCERDTIMOUT", "Read timeout, CCP has not responded to request", 0, 0 },
	{ "CCPSIGCONT", "CCP non fatal error at pc 0x!XJ.  Continuing operation.", 1, 0 },
	{ "CCEBGONLY", "Only BG databases can be clustered", 0, 0 },
	{ "CCENOCCP", "The cluster control program is not running on this node", 0, 0 },
	{ "CCECCPPID", "The cluster control program has PID 0x!XL", 1, 0 },
	{ "CCECLSTPRCS", "!UL processes are accessing clustered database files", 1, 0 },
	{ "ZSHOWBADFUNC", "An invalid information code was specified with ZSHOW or $ZJOBEXAM()", 0, 0 },
	{ "NOTALLJNLEN", "Journaling disabled/off for !AD regions", 2, 0 },
	{ "BADLOCKNEST", "Unsupported nesting of LOCK commands", 0, 0 },
	{ "NOLBRSRC", "Object libraries cannot have SRC paths associated", 0, 0 },
	{ "INVZSTEP", "Invalid ZSTEP qualifier", 0, 0 },
	{ "ZSTEPARG", "ZSTEP argument expected", 0, 0 },
	{ "INVSTRLEN", "Invalid string length !UL: max !UL", 2, 0 },
	{ "RECCNT", "Last LOAD record number: !UL", 1, 0 },
	{ "TEXT", "!AD", 2, 0 },
	{ "ZWRSPONE", "Subscript patterns in ZWRITE are atomic; Invalid delimiter", 0, 0 },
	{ "FILEDEL", "File !AD successfully deleted", 2, 0 },
	{ "JNLBADLABEL", "Journal file !AD has a bad YottaDB Journal File Label. Expected !AD. Found !AD.", 6, 0 },
	{ "JNLREADEOF", "End of journal file encountered", 0, 0 },
	{ "JNLRECFMT", "Journal file record format error encountered", 0, 0 },
	{ "BLKTOODEEP", "Block level too deep", 0, 0 },
	{ "NESTFORMP", "Formal parameter list cannot be combined with nested line", 0, 0 },
	{ "BINHDR", "!AD!/!/Date: !AD!/Time: !AD!/Extract Region Characteristics!/!_Blk Size: !AD!/!_Rec Size: !AD!/!_Key Size: !AD!/!_Std Null Coll: !AD!/!AD!/", 16, 0 },
	{ "GOQPREC", "Numeric precision in key error:  Blk #!UL, Key #!UL.  Record not loaded.", 2, 0 },
	{ "LDGOQFMT", "Corrupt GOQ format header information!/", 0, 0 },
	{ "BEGINST", "Beginning LOAD at record number: !UL", 1, 0 },
	{ "INVMVXSZ", "Invalid block size for GOQ load format", 0, 0 },
	{ "JNLWRTNOWWRTR", "Journal writer attempting another write", 0, 0 },
	{ "GTMSECSHRSHMCONCPROC", "More than one process attached to Shared memory segment (!UL) not removed (!UL)", 2, 0 },
	{ "JNLINVALLOC", "Journal file allocation !UL is not within the valid range of !UL to !UL.  Journal file not created.", 3, 0 },
	{ "JNLINVEXT", "Journal file extension !UL is greater than the maximum allowed size of !UL.  Journal file not created.", 2, 0 },
	{ "MUPCLIERR", "Action not taken due to CLI errors", 0, 0 },
	{ "JNLTMQUAL4", "Time qualifier BEFORE_TIME=\"!AZ\" is less than AFTER_TIME=\"!AZ\"", 2, 0 },
	{ "GTMSECSHRREMSHM", "[client pid !UL] Shared memory segment (!UL) removed, nattch = !UL", 3, 0 },
	{ "GTMSECSHRREMFILE", "[client pid !UL] File (!AD) removed", 3, 0 },
	{ "MUNODBNAME", "A database name or the region qualifier must be specified", 0, 0 },
	{ "FILECREATE", "!AD file !AD created", 4, 0 },
	{ "FILENOTCREATE", "!AD file !AD not created", 4, 0 },
	{ "JNLPROCSTUCK", "Journal file writes blocked by process !UL", 1, 0 },
	{ "INVGLOBALQUAL", "Error in GLOBAL qualifier : Parse error at offset !UL in !AD", 3, 0 },
	{ "COLLARGLONG", "Collation sequence !UL does not contain routines for long strings", 1, 0 },
	{ "NOPINI", "PINI journal record expected but not found in journal file !AD at offset [0x!XL]", 3, 0 },
	{ "DBNOCRE", "Not all specified database files, or their associated journal files were created", 0, 0 },
	{ "JNLSPACELOW", "Journal file !AD nearing maximum size, !UL blocks to go", 3, 0 },
	{ "DBCOMMITCLNUP", "Pid !UL [0x!XL] handled error (code = !UL) during commit of !AZ transaction in database file !AD", 6, 0 },
	{ "BFRQUALREQ", "The [NO]BEFORE qualifier is required for this command", 0, 0 },
	{ "REQDVIEWPARM", "Required View parameter is missing", 0, 0 },
	{ "COLLFNMISSING", "Routine !AD is not found for collation sequence !UL", 3, 0 },
	{ "JNLACTINCMPLT", "Mupip journal action might be incomplete", 0, 0 },
	{ "NCTCOLLDIFF", "Source and destination for MERGE cannot have different numerical collation type", 0, 0 },
	{ "DLRCUNXEOR", "!_!AD!/!_!_!_unexpected end of record in $CHAR()/$ZCHAR() subscript", 2, 0 },
	{ "DLRCTOOBIG", "!_!AD!/!_!_!_!AD value cannot be greater than 255", 4, 0 },
	{ "WCERRNOTCHG", "Not all specified database files were changed", 0, 0 },
	{ "WCWRNNOTCHG", "Not all specified database files were changed", 0, 0 },
	{ "ZCWRONGDESC", "A string longer than 65535 is passed via 32-bit descriptor", 0, 0 },
	{ "MUTNWARN", "Database file !AD has 0x!16@XQ more transactions to go before reaching the transaction number limit (0x!16@XQ). Renew database with MUPIP INTEG TN_RESET", 4, 0 },
	{ "GTMSECSHRUPDDBHDR", "[client pid !UL] database fileheader (!AD) updated !AD", 5, 0 },
	{ "LCKSTIMOUT", "DAL timed LOCK request expired", 0, 0 },
	{ "CTLMNEMAXLEN", "The maximum length of a control mnemonic has been exceeded", 0, 0 },
	{ "CTLMNEXPECTED", "Control mnemonic is expected in this context", 0, 0 },
	{ "USRIOINIT", "User-defined device driver not successfully initialized", 0, 0 },
	{ "CRITSEMFAIL", "Error with semaphores for region !AD", 2, 0 },
	{ "TERMWRITE", "Error writing to terminal", 0, 0 },
	{ "COLLTYPVERSION", "Collation type !UL, version !UL mismatch", 2, 0 },
	{ "LVNULLSUBS", "Null subscripts not allowed in local variables", 0, 0 },
	{ "GVREPLERR", "Error replicating global in region !AD", 2, 0 },
	{ "UNUSEDMSG634", "DBFILERDONLY error nixed in YottaDB r1.36 (still in use in GT.M)", 3, 0 },
	{ "RMWIDTHPOS", "File record size or width must be greater than zero", 0, 0 },
	{ "OFFSETINV", "Entry point !AD+!SL not valid", 3, 0 },
	{ "JOBPARTOOLONG", "Total parameter length is too long for job command", 0, 0 },
<<<<<<< HEAD:sr_port/merrors_ctl.c
	{ "UNUSEDMSG637", "JOBARGMISSING nixed in r1.20 because it is a VMS only error", 0, 0 },
=======
	{ "RLNKINTEGINFO", "Integrity check completed successfully: !AD -- called from module !AD at line !UL", 5, 0 },
>>>>>>> eb3ea98c (GT.M V7.0-002):sr_i386/merrors_ctl.c
	{ "RUNPARAMERR", "Error accessing parameter for run command", 0, 0 },
	{ "FNNAMENEG", "Depth argument to $NAME cannot be negative", 0, 0 },
	{ "ORDER2", "Invalid second argument to $ORDER.  Must be -1 or 1.", 0, 0 },
	{ "MUNOUPGRD", "Database not upgraded because of preceding errors", 0, 0 },
	{ "REORGCTRLY", "User interrupt encountered during database reorg -- halting", 0, 0 },
	{ "TSTRTPARM", "Error parsing TSTART qualifier", 0, 0 },
	{ "TRIGNAMENF", "Trigger name !AD not found with the current default global directory", 2, 0 },
	{ "TRIGZBREAKREM", "ZBREAK in trigger !AD removed due to trigger being reloaded", 2, 0 },
	{ "TLVLZERO", "Transaction is not in progress", 0, 0 },
	{ "TRESTNOT", "Cannot TRESTART, transaction is not restartable", 0, 0 },
	{ "TPLOCK", "Cannot release LOCK(s) held prior to current TSTART", 0, 0 },
	{ "TPQUIT", "Cannot QUIT out of a routine with an active transaction", 0, 0 },
	{ "TPFAIL", "Transaction COMMIT failed.  Failure code: !AD.", 2, 0 },
	{ "TPRETRY", "Restart transaction from non-concurrency DB failure", 0, 0 },
	{ "TPTOODEEP", "$TLEVEL cannot exceed !UL", 1, 0 },
	{ "ZDEFACTIVE", "ZDEFER already active", 0, 0 },
	{ "ZDEFOFLOW", "ZDEFER Buffer overflow to node !AD", 2, 0 },
	{ "MUPRESTERR", "MUPIP restore aborted due to preceding errors", 0, 0 },
	{ "MUBCKNODIR", "MUPIP backup aborted due to error in output directory", 0, 0 },
	{ "TRANS2BIG", "Transaction exceeded available buffer space for region !AD", 2, 0 },
	{ "INVBITLEN", "Invalid size of the bit string", 0, 0 },
	{ "INVBITSTR", "Invalid bit string", 0, 0 },
	{ "INVBITPOS", "Invalid position in the bit string", 0, 0 },
	{ "PARNORMAL", "Parse successful", 0, 0 },
	{ "FILEPATHTOOLONG", "Filename including the path cannot be longer than 255 characters", 0, 0 },
	{ "RMWIDTHTOOBIG", "File record size or width too big", 0, 0 },
	{ "PATTABNOTFND", "Pattern table !AD not found", 2, 0 },
	{ "OBJFILERR", "Error with object file I/O on file !AD", 2, 0 },
	{ "SRCFILERR", "Error with source file I/O on file !AD", 2, 0 },
	{ "NEGFRACPWR", "Invalid operation: fractional power of negative number", 0, 0 },
	{ "MTNOSKIP", "SKIP operation not supported on this device", 0, 0 },
	{ "CETOOMANY", "Too many compiler escape substitutions in a single statement", 0, 0 },
	{ "CEUSRERROR", "Compiler escape user routine returned error code !UL", 1, 0 },
	{ "CEBIGSKIP", "Compiler escape user routine skip count is too large", 0, 0 },
	{ "CETOOLONG", "Compiler escape substitution exceeds maximum line size", 0, 0 },
	{ "CENOINDIR", "Indirection type information not available for compiler escape feature", 0, 0 },
	{ "COLLATIONUNDEF", "Collation type !UL is not defined", 1, 0 },
	{ "MSTACKCRIT", "User-specified M stack size critical threshold of !UL not appropriate; must be between !UL and !UL; reverting to !UL", 4, 0 },
	{ "GTMSECSHRSRVF", "!AD - !UL : Attempt to service request failed (retry = !UL)", 4, 0 },
	{ "FREEZECTRL", "Control Y or control C encountered during attempt to freeze the database. Aborting freeze.", 0, 0 },
	{ "JNLFLUSH", "Error flushing journal buffers to journal file !AD", 2, 0 },
	{ "CCPSIGDMP", "CCP non fatal dump, continuing operation. Report to your YottaDB Support Channel.", 0, 0 },
	{ "NOPRINCIO", "Unable to !AD principal device: !AD at !AD due to: !AD", 8, 0 },
	{ "INVPORTSPEC", "Invalid port specification", 0, 0 },
	{ "INVADDRSPEC", "Invalid IP address specification", 0, 0 },
	{ "MUREENCRYPTEND", "Database !AD : MUPIP REORG ENCRYPT finished by pid !UL at transaction number [0x!16@XQ]", 4, 0 },
	{ "CRYPTJNLMISMATCH", "Encryption settings mismatch between journal file !AD and corresponding database file !AD", 4, 0 },
	{ "SOCKWAIT", "Error waiting for socket connection", 0, 0 },
	{ "SOCKACPT", "Error accepting socket connection", 0, 0 },
	{ "SOCKINIT", "Error initializing socket: (errno == !UL) !AD", 3, 0 },
	{ "OPENCONN", "Error opening socket connection", 0, 0 },
	{ "DEVNOTIMP", "!AD device not implemented on in this environment", 2, 0 },
	{ "PATALTER2LARGE", "Pattern match alternation exceeded the !UL repetition limit on prospective matches", 1, 0 },
	{ "DBREMOTE", "Database region !AD is remote; perform maintenance on the server node", 2, 0 },
	{ "JNLREQUIRED", "Journaling is required for clustered operation with file !AD", 2, 0 },
	{ "TPMIXUP", "!AZ transaction cannot be started within !AZ transaction", 2, 0 },
	{ "HTOFLOW", "Hash table overflow: Failed to allocate !UL elements", 1, 0 },
	{ "RMNOBIGRECORD", "File record size requires BIGRECORD parameter", 0, 0 },
	{ "DBBMSIZE", "!AD Bit map has incorrect size", 2, 0 },
	{ "DBBMBARE", "!AD Bit map does not protect itself", 2, 0 },
	{ "DBBMINV", "!AD Bit map contains an invalid pattern", 2, 0 },
	{ "DBBMMSTR", "!AD Bit map does not match master map", 2, 0 },
	{ "DBROOTBURN", "!AD Root block has data level", 2, 0 },
	{ "REPLSTATEERR", "Replication state cannot be changed to the specified value for database file !AD", 2, 0 },
	{ "UNUSEDMSG702", "VMSMEMORY nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "DBDIRTSUBSC", "!AD Directory tree block contains non name-level entries", 2, 0 },
	{ "TIMEROVFL", "Timer overflow; interval probably too large", 0, 0 },
	{ "GTMASSERT", "!AD - Assert failed !AD line !UL", 5, 0 },
	{ "DBFHEADERR4", "Database file !AD: control problem: !AD was 0x!XL expecting 0x!XL", 6, 0 },
	{ "DBADDRANGE", "Database file !AD, element location 0x!XJ: blk = 0x!XL: control 0x!XJ was outside !AD range 0x!XJ to 0x!XJ", 9, 0 },
	{ "DBQUELINK", "Database file !AD, element location 0x!XJ: blk = 0x!16@XQ: control !AD queue problem: was 0x!XJ, expecting 0x!XJ", 8, 0 },
	{ "DBCRERR", "Database file !AD, cr location 0x!XJ blk = 0x!16@XQ error: !AD was 0x!XL, expecting 0x!XL -- called from module !AD at line !UL", 11, 0 },
	{ "MUSTANDALONE", "Could not get exclusive access to !AD", 2, 1 },
	{ "MUNOACTION", "MUPIP unable to perform requested action", 0, 0 },
	{ "RMBIGSHARE", "File with BIGRECORD specified may only be shared if READONLY", 0, 0 },
	{ "TPRESTART", "Database !AD; code: !AD; blk: 0x!16@XQ in glbl: ^!AD; pvtmods: !UL, blkmods: !UL, blklvl: !UL, type: !UL, readset: !UL, writeset: !UL, local_tn: 0x!16@XQ, zpos: !AD", 16, 0 },
	{ "SOCKWRITE", "Write to a socket failed", 0, 0 },
	{ "DBCNTRLERR", "Database file !AD: control error suspected but not found", 2, 0 },
	{ "NOTERMENV", "Environment variable TERM not set.  Assuming \"unknown.\"", 0, 0 },
	{ "NOTERMENTRY", "TERM = \"!AD\" has no \"terminfo\" entry.  Possible terminal handling problems.", 2, 0 },
	{ "NOTERMINFODB", "No \"terminfo\" database.  Terminal handling problems likely.", 0, 0 },
	{ "INVACCMETHOD", "Invalid access method", 0, 0 },
	{ "JNLOPNERR", "Error opening journal file !AD!/  for database !AD", 4, 0 },
	{ "JNLRECTYPE", "Journal record type does not match expected type", 0, 0 },
	{ "JNLTRANSGTR", "Transaction number !@UQ in journal header is greater than !@UQ in database header", 2, 0 },
	{ "JNLTRANSLSS", "Transaction number !@UQ in journal header is less than !@UQ in database header", 2, 0 },
	{ "JNLWRERR", "Error writing journal file !AD.  Unable to update header.", 2, 0 },
	{ "FILEIDMATCH", "Saved File ID does not match the current ID - the file appears to have been moved", 0, 0 },
	{ "EXTSRCLIN", "!_!AD!/!_!AD", 4, 0 },
	{ "EXTSRCLOC", "!_!_At column !UL, line !UL, source module !AD", 4, 0 },
	{ "UNUSEDMSG728", "BIGNOACL nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "ERRCALL", "Error called from !AD line !UL", 3, 0 },
	{ "ZCCTENV", "Environmental variable for external package !AD not set", 2, 0 },
	{ "ZCCTOPN", "Unable to open external call table: !AD", 2, 0 },
	{ "ZCCTNULLF", "External call table contains no records: !AD", 2, 0 },
	{ "ZCUNAVAIL", "Package, !AD unavailable", 2, 0 },
	{ "ZCENTNAME", "No entry name found in external call table", 0, 0 },
	{ "ZCCOLON", "Colon expected but not found", 0, 0 },
	{ "ZCRTNTYP", "Unknown return type", 0, 0 },
	{ "ZCRCALLNAME", "Routine name expected but not found", 0, 0 },
	{ "ZCRPARMNAME", "Parameter name expected but not found", 0, 0 },
	{ "ZCUNTYPE", "Unknown type encountered", 0, 0 },
	{ "UNUSEDMSG740", "ZCMLTSTATUS nixed in r2.00 Jan 2024", 0, 0 },
	{ "ZCSTATUSRET", "External call returned error status", 0, 0 },
	{ "ZCMAXPARAM", "Exceeded maximum number of external call parameters", 0, 0 },
	{ "ZCCSQRBR", "Closing Square bracket expected", 0, 0 },
	{ "ZCPREALLNUMEX", "Pre-allocation value should be a decimal number", 0, 0 },
	{ "ZCPREALLVALPAR", "Pre-allocation allowed only for variables passed by reference", 0, 0 },
	{ "VERMISMATCH", "Attempt to access !AD with version !AD, while already using !AD", 6, 0 },
	{ "JNLCNTRL", "Journal control unsynchronized for !AD.", 2, 0 },
	{ "TRIGNAMBAD", "Trigger initialization failed. Error while processing ^#t(\"!AD\",!AD)", 4, 0 },
	{ "BUFRDTIMEOUT", "Pid [0x!XL] timed out waiting for buffered read of blk [0x!XL] into cr [0x!XL] by process [0x!XL] to complete in database file !AD", 6, 0 },
	{ "INVALIDRIP", "Invalid read-in-progress field in Cache Record.  Resetting and continuing.  Region: !AD.", 2, 0 },
	{ "BLKSIZ512", "Block size !UL rounds to !UL", 2, 0 },
	{ "MUTEXERR", "Mutual Exclusion subsystem failure", 0, 0 },
	{ "JNLVSIZE", "Journal File !AD has incorrect virtual_filesize !UL.  Allocation : !UL, Extension : !UL, Filesize : !UL, File system block size : !UL", 7, 0 },
	{ "MUTEXLCKALERT", "Mutual Exclusion subsystem ALERT - lock attempt threshold crossed for region !AD.  Process !UL is in crit cycle !UL.", 4, 0 },
	{ "MUTEXFRCDTERM", "Mutual Exclusion subsystem detected forced termination of process !UL.  Crit salvaged from database file !AD.", 3, 0 },
	{ "GTMSECSHR", "!UL : Error during gtmsecshr operation", 1, 0 },
	{ "GTMSECSHRSRVFID", "!AD: !UL - Attempt to service request failed.!/ client id: !UL, mesg type: !UL, mesg data: !UL", 6, 0 },
	{ "GTMSECSHRSRVFIL", "!AD: !UL - Attempt to service request failed.!/ client id: !UL, mesg type: !UL!/file: !AD", 7, 0 },
	{ "FREEBLKSLOW", "Only !@UQ free blocks left out of !@UQ total blocks for !AD", 4, 0 },
	{ "PROTNOTSUP", "Protocol !AD not supported", 2, 0 },
	{ "DELIMSIZNA", "Delimiter size is not appropriate", 0, 0 },
	{ "INVCTLMNE", "Invalid control mnemonics", 0, 0 },
	{ "SOCKLISTEN", "Error listening on a socket", 0, 0 },
	{ "RESTORESUCCESS", "Restore completed successfully", 0, 0 },
	{ "ADDRTOOLONG", "Socket address !AD of length !UL is longer than the maximum permissible length !UL", 4, 0 },
	{ "GTMSECSHRGETSEMFAIL", "error getting semaphore errno = !UL", 1, 0 },
	{ "CPBEYALLOC", "Attempt to copy beyond the allocated buffer", 0, 0 },
	{ "DBRDONLY", "Database file !AD read only", 2, 5 },
	{ "DUPTN", "Duplicate transaction found [TN = 0x!16@XQ] at offset 0x!XL in journal file !AD", 4, 0 },
	{ "TRESTLOC", "Transaction start: !AD, Transaction failure: !AD", 4, 0 },
	{ "REPLPOOLINST", "Error with replication pool (id = !UL) for instance file !AD", 3, 0 },
	{ "ZCVECTORINDX", "Invalid Vector Index !UL", 1, 0 },
	{ "REPLNOTON", "Replication is not on for journal file !AD, rollback will not continue", 2, 0 },
	{ "JNLMOVED", "Journal file appears to have been moved.  Journaling activity will not be done.", 0, 0 },
	{ "EXTRFMT", "Extract error: invalid record format - no records found.", 0, 0 },
	{ "CALLERID", "Routine !AD called from 0x!XJ", 3, 0 },
	{ "KRNLKILL", "Process was terminated by SIGDANGER signal from the system -- System swap space is too low -- Report to System Administrator", 0, 0 },
	{ "MEMORYRECURSIVE", "Memory Subsystem called recursively", 0, 0 },
	{ "FREEZEID", "Cache !AD on !AD by freeze id 0x!XL with match 0x!XL from 0x!XJ", 7, 0 },
	{ "UNUSEDMSG778", "BLKWRITERR last used in V6.3-001A May 2017", 0, 0 },
	{ "DSEINVALBLKID", "Trying to edit DB with 64-bit block IDs using pre-V7 DSE", 0, 0 },
	{ "PINENTRYERR", "Custom pinentry program failure", 0, 0 },
	{ "BCKUPBUFLUSH", "Unable to flush buffer for online backup", 0, 0 },
	{ "NOFORKCORE", "Unable to fork off process to create core.  Core creation postponed.", 0, 0 },
	{ "JNLREAD", "Error reading from journal file !AD at offset [0x!XL]", 3, 0 },
	{ "JNLMINALIGN", "Journal Record Alignment !UL is less than the minimum value of !UL", 2, 0 },
	{ "JOBSTARTCMDFAIL", "JOB command STARTUP script invocation failed", 0, 0 },
	{ "JNLPOOLSETUP", "Journal Pool setup error", 0, 0 },
	{ "JNLSTATEOFF", "ROLLBACK or RECOVER BACKWARD cannot proceed as database file !AD does not have journaling ENABLED and ON", 2, 0 },
	{ "RECVPOOLSETUP", "Receive Pool setup error", 0, 0 },
	{ "REPLCOMM", "Replication subsystem communication failure", 0, 0 },
	{ "NOREPLCTDREG", "Replication subsystem found no region replicated for !AD !AZ", 3, 0 },
	{ "REPLINFO", "!AD", 2, 0 },
	{ "REPLWARN", "!AD", 2, 0 },
	{ "REPLERR", "!AD", 2, 0 },
	{ "JNLNMBKNOTPRCD", "Journal file !AD does not match the current journal file !AD of database file !AD", 6, 0 },
	{ "REPLFILIOERR", "Replication subsystem file I/O error !AD", 2, 0 },
	{ "REPLBRKNTRANS", "Replication subsystem found seqno !16@XQ broken or missing in the journal files", 1, 0 },
	{ "TTWIDTHTOOBIG", "Terminal WIDTH exceeds the maximum allowed limit", 0, 0 },
	{ "REPLLOGOPN", "Replication subsystem could not open log file !AD: !AD.  Logging done to !AD.", 6, 0 },
	{ "REPLFILTER", "Replication filter subsystem failure", 0, 0 },
	{ "GBLMODFAIL", "Global variable Conflict Test failed.  Failure code: !AD.", 2, 0 },
	{ "TTLENGTHTOOBIG", "Terminal LENGTH exceeds the maximum allowed limit", 0, 0 },
	{ "TPTIMEOUT", "Transaction timeout", 0, 0 },
	{ "NORTN", "Routine name missing", 0, 0 },
	{ "JNLFILNOTCHG", "Journal file not changed", 0, 0 },
	{ "EVENTLOGERR", "Error in event logging subsystem", 0, 0 },
	{ "UPDATEFILEOPEN", "Update file open error", 0, 0 },
	{ "JNLBADRECFMT", "Journal File Record Format Error encountered for file !AD at disk address 0x!XL", 3, 0 },
	{ "NULLCOLLDIFF", "Null collation order must be the same for all regions", 0, 0 },
	{ "MUKILLIP", "Kill in progress indicator is set for file !AD - this !AD operation is likely to result in incorrectly marked busy errors", 4, 0 },
	{ "JNLRDONLY", "Journal file !AD read only", 2, 0 },
	{ "ANCOMPTINC", "Deviceparameter !AD is not compatible with any other deviceparameters in the !AD command", 4, 0 },
	{ "ABNCOMPTINC", "Deviceparameter !AD and deviceparameter !AD are not compatible in the !AD command", 6, 0 },
	{ "RECLOAD", "Error loading record number: !AD", 2, 0 },
	{ "SOCKNOTFND", "Socket !AD not found", 2, 0 },
	{ "CURRSOCKOFR", "Current socket of index !UL is out of range.  There are only !UL sockets.", 2, 0 },
	{ "SOCKETEXIST", "Socket !AD already exists", 2, 0 },
	{ "LISTENPASSBND", "Controlmnemonic LISTEN can be applied to PASSIVE socket in the state BOUND only", 0, 0 },
	{ "DBCLNUPINFO", "Database file !AD !/!AD", 4, 0 },
	{ "MUNODWNGRD", "Database not downgraded because of preceding errors", 0, 0 },
	{ "REPLTRANS2BIG", "Transaction !16@XQ of size !@ZQ (pre-filter size !@ZQ) too large to be accommodated in the !AD pool", 5, 0 },
	{ "RDFLTOOLONG", "Length specified for fixed length read exceeds the maximum string size", 0, 0 },
	{ "MUNOFINISH", "MUPIP unable to finish all requested actions", 0, 0 },
	{ "DBFILEXT", "Database file !AD extended from 0x!16@XQ blocks to 0x!16@XQ at transaction 0x!16@XQ", 5, 0 },
	{ "JNLFSYNCERR", "Error synchronizing journal file !AD to disk", 2, 0 },
	{ "ICUNOTENABLED", "ICU libraries not loaded", 0, 0 },
	{ "ZCPREALLVALINV", "The pre-allocation value exceeded the maximum string length", 0, 0 },
	{ "NEWJNLFILECREAT", "Journal file !AD nearing maximum size.  New journal file created.", 2, 0 },
	{ "DSKSPACEFLOW", "Disk Space for file !AD nearing maximum size.  !@UQ blocks available.", 3, 0 },
	{ "GVINCRFAIL", "Global variable $INCR failed.  Failure code: !AD.", 2, 0 },
	{ "ISOLATIONSTSCHN", "Error changing NOISOLATION status for global ^!AD within a TP transaction from !UL to !UL", 4, 0 },
	{ "UNUSEDMSG833", "REPLGBL2LONG nixed in r1.24", 0, 0 },
	{ "TRACEON", "Missing global name (with optional subscripts) for recording M-tracing information", 0, 0 },
	{ "TOOMANYCLIENTS", "GT.CM is serving the maximum number of clients.  Try again later.", 0, 0 },
	{ "NOEXCLUDE", "None of the excluded variables exist", 0, 0 },
	{ "UNUSEDMSG837", "GVINCRISOLATION nixed in r1.24", 0, 0 },
	{ "EXCLUDEREORG", "Global: !AD is present in the EXCLUDE option.  REORG will skip the global.", 2, 0 },
	{ "REORGINC", "Reorg was incomplete.  Not all globals were reorged.", 0, 0 },
	{ "ASC2EBCDICCONV", "ASCII/EBCDIC conversion failed when calling !AD", 2, 0 },
	{ "GTMSECSHRSTART", "!AD - !UL : gtmsecshr failed to startup", 3, 0 },
	{ "DBVERPERFWARN1", "Performance warning: Database !AD is running in compatibility mode which degrades performance. Run MUPIP REORG UPGRADE for best overall performance", 2, 0 },
	{ "FILEIDGBLSEC", "File ID in global section does not match with the database file !AD", 2, 0 },
	{ "GBLSECNOTGDS", "Global Section !AD is not a YottaDB global section", 2, 0 },
	{ "BADGBLSECVER", "Global Section !AD does not match the current database version", 2, 0 },
	{ "RECSIZENOTEVEN", "RECORDSIZE [!UL] needs to be a multiple of 2 if ICHSET or OCHSET is UTF-16, UTF-16LE or UTF-16BE", 1, 0 },
	{ "BUFFLUFAILED", "Error flushing buffers from !AD for database file !AD", 4, 5 },
	{ "MUQUALINCOMP", "Incompatible qualifiers - FILE and REGION", 0, 0 },
	{ "DISTPATHMAX", "Executable path length is greater than maximum (!UL)", 1, 0 },
	{ "FILEOPENFAIL", "Failed to open file !AD", 2, 0 },
	{ "UNUSEDMSG851", "IMAGENAME nixed in r1.20", 0, 0 },
	{ "GTMSECSHRPERM", "The gtmsecshr module in $ydb_dist (!AD) does not have the correct permission: !AD, and UID: !UL", 5, 0 },
	{ "YDBDISTUNDEF", "Environment variable $ydb_dist is not defined", 0, 0 },
	{ "SYSCALL", "Error received from system call !AD -- called from module !AD at line !UL", 5, 0 },
	{ "MAXGTMPATH", "The executing module path is greater than the maximum !UL", 1, 0 },
	{ "TROLLBK2DEEP", "Intended rollback(!SL) deeper than the current $tlevel(!UL)", 2, 0 },
	{ "INVROLLBKLVL", "Rollback level (!UL) not less than current $TLEVEL(!UL).  Can't rollback.", 2, 0 },
	{ "OLDBINEXTRACT", "Loading an older version(!UL) of binary extract. !/Database or global collation changes since the extract, if any, will result in database corruption.", 1, 0 },
	{ "ACOMPTBINC", "Deviceparameter !AD is compatible with only !AD in the command !AD", 6, 0 },
	{ "NOTREPLICATED", "Transaction number !16@XQ generated by the !AD process (PID = !UL) is not replicated to the secondary", 4, 0 },
	{ "DBPREMATEOF", "Premature end of file with database file !AD", 2, 0 },
	{ "KILLBYSIG", "!AD process !UL has been killed by a signal !UL", 4, 0 },
	{ "KILLBYSIGUINFO", "!AD process !UL has been killed by a signal !UL from process !UL with userid number !UL", 6, 0 },
	{ "KILLBYSIGSINFO1", "!AD process !UL has been killed by a signal !UL at address 0x!XJ (vaddr 0x!XJ)", 6, 0 },
	{ "KILLBYSIGSINFO2", "!AD process !UL has been killed by a signal !UL at address 0x!XJ", 5, 0 },
	{ "SIGILLOPC", "Signal was caused by an illegal opcode", 0, 0 },
	{ "SIGILLOPN", "Signal was caused by an illegal operand", 0, 0 },
	{ "SIGILLADR", "Signal was caused by illegal addressing mode", 0, 0 },
	{ "SIGILLTRP", "Signal was caused by an illegal trap", 0, 0 },
	{ "SIGPRVOPC", "Signal was caused by a privileged opcode", 0, 0 },
	{ "SIGPRVREG", "Signal was caused by a privileged register", 0, 0 },
	{ "SIGCOPROC", "Signal was caused by a coprocessor error", 0, 0 },
	{ "SIGBADSTK", "Signal was caused by an internal stack error", 0, 0 },
	{ "SIGADRALN", "Signal was caused by invalid address alignment", 0, 0 },
	{ "SIGADRERR", "Signal was caused by a non-existent physical address", 0, 0 },
	{ "SIGOBJERR", "Signal was caused by an object specific hardware error", 0, 0 },
	{ "SIGINTDIV", "Signal was caused by an integer divided by zero", 0, 0 },
	{ "SIGINTOVF", "Signal was caused by an integer overflow", 0, 0 },
	{ "SIGFLTDIV", "Signal was caused by a floating point divide by zero", 0, 0 },
	{ "SIGFLTOVF", "Signal was caused by a floating point overflow", 0, 0 },
	{ "SIGFLTUND", "Signal was caused by a floating point underflow", 0, 0 },
	{ "SIGFLTRES", "Signal was caused by a floating point inexact result", 0, 0 },
	{ "SIGFLTINV", "Signal was caused by an invalid floating point operation", 0, 0 },
	{ "SIGMAPERR", "Signal was caused by an address not mapped to an object", 0, 0 },
	{ "SIGACCERR", "Signal was caused by invalid permissions for mapped object", 0, 0 },
	{ "TRNLOGFAIL", "Translation of (VMS) logical name or (UNIX) environment variable !AD failed", 2, 0 },
	{ "INVDBGLVL", "Invalid non-numeric debug level specified !AD in (VMS) logical name or (UNIX) environment variable !AD", 4, 0 },
	{ "DBMAXNRSUBS", "!AD Maximum number of subscripts exceeded", 2, 4 },
	{ "GTMSECSHRSCKSEL", "gtmsecshr select on socket failed", 0, 0 },
	{ "GTMSECSHRTMOUT", "gtmsecshr exiting due to idle timeout", 0, 0 },
	{ "GTMSECSHRRECVF", "gtmsecshr receive on server socket failed", 0, 0 },
	{ "GTMSECSHRSENDF", "gtmsecshr send on server socket failed", 0, 0 },
	{ "SIZENOTVALID8", "Size (in bytes) must be either 1, 2, 4, or 8", 0, 0 },
	{ "GTMSECSHROPCMP", "gtmsecshr operation may be compromised", 0, 0 },
	{ "GTMSECSHRSUIDF", "gtmsecshr server setuid to root failed", 0, 0 },
	{ "GTMSECSHRSGIDF", "gtmsecshr server setgid to root failed", 0, 0 },
	{ "GTMSECSHRSSIDF", "gtmsecshr server setsid failed", 0, 0 },
	{ "GTMSECSHRFORKF", "gtmsecshr server unable to fork off a child process", 0, 0 },
	{ "DBFSYNCERR", "Error synchronizing database file !AD to disk", 2, 0 },
	{ "UNUSEDMSG898", "SECONDAHEAD last used in V7.0-000 Jan 2021", 0, 0 },
	{ "SCNDDBNOUPD", "Database Updates not allowed on the secondary", 0, 0 },
	{ "MUINFOUINT4", "!AD : !UL [0x!XL]", 4, 0 },
	{ "NLMISMATCHCALC", "Location of !AD expected at 0x!XL, but found at 0x!XL", 4, 0 },
	{ "RELINKCTLFULL", "Relinkctl file for directory !AD is full (maximum entries !UL)", 3, 0 },
	{ "MUPIPSET2BIG", "!UL too large, maximum !AD allowed is !UL", 4, 0 },
	{ "DBBADNSUB", "!AD Bad numeric subscript", 2, 4 },
	{ "DBBADKYNM", "!AD Bad key name", 2, 4 },
	{ "DBBADPNTR", "!AD Bad pointer value in directory", 2, 3 },
	{ "DBBNPNTR", "!AD Bit map block number as pointer", 2, 3 },
	{ "DBINCLVL", "!AD Block at incorrect level", 2, 3 },
	{ "DBBFSTAT", "!AD Block busy/free status unknown (local bitmap corrupted)", 2, 3 },
	{ "DBBDBALLOC", "!AD Block doubly allocated", 2, 4 },
	{ "DBMRKFREE", "!AD Block incorrectly marked free", 2, 3 },
	{ "DBMRKBUSY", "!AD Block incorrectly marked busy", 2, 2 },
	{ "DBBSIZZRO", "!AD Block size equals zero", 2, 1 },
	{ "DBSZGT64K", "!AD Block size is greater than 64K", 2, 1 },
	{ "DBNOTMLTP", "!AD Block size not a multiple of 512 bytes", 2, 1 },
	{ "DBTNTOOLG", "!AD Block transaction number too large", 2, 5 },
	{ "DBBPLMLT512", "!AD Blocks per local map is less than 512", 2, 3 },
	{ "DBBPLMGT2K", "!AD Blocks per local map is greater than 2K", 2, 3 },
	{ "MUINFOUINT8", "!AD : !@ZQ [0x!16@XQ]", 4, 0 },
	{ "DBBPLNOT512", "!AD Blocks per local map is not 512", 2, 3 },
	{ "MUINFOSTR", "!AD : !AD", 4, 0 },
	{ "DBUNDACCMT", "!AD Cannot determine access method; trying with BG", 2, 2 },
	{ "DBTNNEQ", "!AD Current tn and early tn are not equal", 2, 5 },
	{ "MUPGRDSUCC", "Database file !AD successfully !AD to !AD", 6, 0 },
	{ "DBDSRDFMTCHNG", "Database file !AD, Desired DB Format set to !AD by !AD with pid !UL [0x!XL] at transaction number [0x!16@XQ]", 9, 0 },
	{ "DBFGTBC", "!AD File size larger than block count would indicate", 2, 5 },
	{ "DBFSTBC", "!AD File size smaller than block count would indicate", 2, 3 },
	{ "DBFSTHEAD", "!AD File smaller than database header", 2, 1 },
	{ "DBCREINCOMP", "!AD Header indicates database file creation was interrupted before completion", 2, 1 },
	{ "DBFLCORRP", "!AD Header indicates database file is corrupt", 2, 1 },
	{ "DBHEADINV", "!AD Header size not valid for database", 2, 1 },
	{ "DBINCRVER", "!AD Incorrect version of YottaDB database", 2, 1 },
	{ "DBINVGBL", "!AD Invalid mixing of global names", 2, 4 },
	{ "DBKEYGTIND", "!AD Key greater than index key", 2, 4 },
	{ "DBGTDBMAX", "!AD Key larger than database maximum", 2, 4 },
	{ "DBKGTALLW", "!AD Key larger than maximum allowed length", 2, 4 },
	{ "DBLTSIBL", "!AD Keys less than sibling's index key", 2, 4 },
	{ "DBLRCINVSZ", "!AD Last record of block has invalid size", 2, 4 },
	{ "MUREUPDWNGRDEND", "Region !AD : MUPIP REORG UPGRADE/DOWNGRADE finished by pid !UL [0x!XL] at transaction number [0x!16@XQ]", 5, 0 },
	{ "DBLOCMBINC", "!AD Local bit map incorrect", 2, 2 },
	{ "DBLVLINC", "!AD Local bitmap block level incorrect", 2, 2 },
	{ "DBMBSIZMX", "!AD Map block too large", 2, 2 },
	{ "DBMBSIZMN", "!AD Map block too small", 2, 2 },
	{ "DBMBTNSIZMX", "!AD Map block transaction number too large", 2, 5 },
	{ "DBMBMINCFRE", "!AD Master bit map incorrectly asserts this local map has free space", 2, 2 },
	{ "DBMBPINCFL", "!AD Master bit map incorrectly marks this local map full", 2, 2 },
	{ "DBMBPFLDLBM", "!AD Master bit map shows this map full, agreeing with disk local map", 2, 2 },
	{ "DBMBPFLINT", "!AD Master bit map shows this map full, agreeing with MUPIP INTEG", 2, 2 },
	{ "DBMBPFLDIS", "!AD Master bit map shows this map full, in disagreement with both disk and INTEG result", 2, 2 },
	{ "DBMBPFRDLBM", "!AD Master bit map shows this map has space, agreeing with disk local map", 2, 2 },
	{ "DBMBPFRINT", "!AD Master bit map shows this map has space, agreeing with MUPIP INTEG", 2, 2 },
	{ "DBMAXKEYEXC", "!AD Maximum key size for database exceeds design maximum", 2, 0 },
	{ "REPLAHEAD", "Replicating instance is ahead of the originating instance.!AD", 2, 0 },
	{ "MUPIPSET2SML", "!UL too small, minimum !AD allowed is !UL", 4, 0 },
	{ "DBREADBM", "!AD Read error on bit map", 2, 3 },
	{ "DBCOMPTOOLRG", "!AD Record has too large compression count", 2, 4 },
	{ "DBVERPERFWARN2", "Peformance warning: Database !AD is not fully upgraded. Run MUPIP REORG UPGRADE for best overall performance", 2, 0 },
	{ "DBRBNTOOLRG", "!AD Root block number greater than last block number in file", 2, 3 },
	{ "DBRBNLBMN", "!AD Root block number is a local bit map number", 2, 3 },
	{ "DBRBNNEG", "!AD Root block number negative", 2, 3 },
	{ "DBRLEVTOOHI", "!AD Root level higher than maximum", 2, 3 },
	{ "DBRLEVLTONE", "!AD Root level less than one", 2, 3 },
	{ "DBSVBNMIN", "!AD Start VBN smaller than possible", 2, 1 },
	{ "DBTTLBLK0", "!AD Total blocks equal zero", 2, 1 },
	{ "DBNOTDB", "!AD File does not have a valid GDS file header", 2, 0 },
	{ "DBTOTBLK", "File header indicates total blocks is 0x!16@XQ but file size indicates total blocks would be 0x!16@XQ", 2, 0 },
	{ "DBTN", "Block TN is 0x!16@XQ", 1, 0 },
	{ "DBNOREGION", "None of the database regions accessible", 0, 5 },
	{ "DBTNRESETINC", "WARNING: tn_reset for database is incomplete due to integrity errors", 0, 0 },
	{ "DBTNLTCTN", "Transaction numbers greater than or equal to the current transaction were found", 0, 5 },
	{ "DBTNRESET", "Cannot reset transaction number for this region", 0, 0 },
	{ "MUTEXRSRCCLNUP", "Mutex subsystem leftover resource !AD removed", 2, 0 },
	{ "SEMWT2LONG", "Process !UL waited !UL second(s) for the !AD lock for region !AD, lock held by pid !UL", 7, 0 },
	{ "REPLINSTOPEN", "Error opening replication instance file !AD", 2, 0 },
	{ "REPLINSTCLOSE", "Error closing replication instance file !AD", 2, 0 },
	{ "JOBSETUP", "Error receiving !AD from parent process", 2, 0 },
	{ "DBCRERR8", "Database file !AD, cr location 0x!XJ blk = 0x!16@XQ error: !AD was 0x!16@XQ, expecting 0x!16@XQ -- called from module !AD at line !UL", 11, 0 },
	{ "NUMPROCESSORS", "Could not determine number of processors", 0, 0 },
	{ "DBADDRANGE8", "Database file !AD, element location 0x!XJ: blk = 0x!16@XQ: control 0x!16@XQ was outside !AD range 0x!16@XQ to 0x!16@XQ", 9, 0 },
	{ "RNDWNSEMFAIL", "Attempting to acquire gds_rundown semaphore when it is already owned", 0, 0 },
	{ "GTMSECSHRSHUTDN", "gtmsecshr process has received a shutdown request -- shutting down", 0, 0 },
	{ "NOSPACECRE", "Not enough space to create database file !AD.  !@ZQ blocks are needed, only !@ZQ available.", 4, 0 },
	{ "LOWSPACECRE", "Disk space for database file !AD is not enough for !UL future extensions.  !@ZQ !UL-byte blocks are needed, only !@ZQ available.", 6, 0 },
	{ "WAITDSKSPACE", "Process 0x!XL will wait !UL seconds for necessary disk space to become available for !AD ", 4, 0 },
	{ "OUTOFSPACE", "Database file !AD ran out of disk space.  Detected by process !UL.  !/Exit without clearing shared memory due to the disk space constraints.  !/Make space and then perform mupip rundown to ensure database integrity.", 3, 0 },
	{ "JNLPVTINFO", "Pid 0x!XL cycle 0x!XL fd_mismatch 0x!XL channel 0x!XL sync_io 0x!XL pini_addr 0x!XL qio_active 0x!XL old_channel 0x!XL", 8, 0 },
	{ "NOSPACEEXT", "Not enough disk space for file !AD to extend.  !@UQ blocks needed.  !@UQ blocks available.", 4, 0 },
	{ "WCBLOCKED", "Field !AD is set by process !UL at transaction number 0x!16@XQ for database file !AD", 6, 0 },
	{ "REPLJNLCLOSED", "Replication in jeopardy as journaling got closed for database file !AD. Current region seqno is !@ZQ [0x!16@XQ] and system seqno is !@ZQ [0x!16@XQ]", 6, 0 },
	{ "RENAMEFAIL", "Rename of file !AD to !AD failed", 4, 0 },
	{ "FILERENAME", "File !AD is renamed to !AD", 4, 0 },
	{ "JNLBUFINFO", "Pid 0x!XL dsk 0x!XL free 0x!XL bytcnt 0x!XL io_in_prog 0x!XL fsync_in_prog 0x!XL dskaddr 0x!XL freeaddr 0x!XL qiocnt 0x!XL now_writer 0x!XL fsync_pid 0x!XL filesize 0x!XL cycle 0x!XL errcnt 0x!XL wrtsize 0x!XL fsync_dskaddr 0x!XL rsrv_free 0x!XL rsrv_freeaddr 0x!XL phase2_commit_index1 0x!XL phase2_commit_index2 0x!XL next_align_addr 0x!XL size 0x!XL", 22, 0 },
	{ "SDSEEKERR", "Sequential device seek error - !AD", 2, 0 },
	{ "LOCALSOCKREQ", "LOCAL socket required", 0, 0 },
	{ "TPNOTACID", "!AD at !AD violates ACID properties of a TRANSACTION and could exceed !AD seconds; $TRESTART = !UL and indefinite RESTARTs may occur", 7, 0 },
	{ "JNLSETDATA2LONG", "SET journal record has data of length !UL.  Target system cannot handle data more than !UL bytes.", 2, 0 },
	{ "JNLNEWREC", "Target system cannot recognize journal record of type !UL, last recognized type is !UL", 2, 0 },
	{ "REPLFTOKSEM", "Error with replication semaphores for instance file !AD", 2, 0 },
	{ "SOCKNOTPASSED", "Socket message contained no passed socket descriptors", 0, 0 },
	{ "UNUSEDMSG1002", "EXTRIOERR nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "UNUSEDMSG1003", "EXTRCLOSEERR nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "CONNSOCKREQ", "Socket not connected", 0, 0 },
	{ "REPLEXITERR", "Replication process encountered an error while exiting", 0, 0 },
	{ "MUDESTROYSUC", "Global section (!AD) corresponding to file !AD successfully destroyed", 4, 0 },
	{ "DBRNDWN", "Error during global database rundown for region !AD.!/Notify those responsible for proper database operation.", 2, 0 },
	{ "MUDESTROYFAIL", "Global section (!AD) corresponding to file !AD failed to be destroyed", 4, 0 },
	{ "NOTALLDBOPN", "Not all required database files were opened", 0, 0 },
	{ "MUSELFBKUP", "Database file !AD can not be backed upon itself", 2, 0 },
	{ "DBDANGER", "Process !UL [0x!XL] killed while committing update for database file !AD.  Possibility of damage to block 0x!16@XQ.", 5, 0 },
	{ "UNUSEDMSG1012", "TRUNCATEFAIL nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "TCGETATTR", "Error while getting terminal attributes on file descriptor !UL", 1, 0 },
	{ "TCSETATTR", "Error while setting terminal attributes on file descriptor !UL", 1, 0 },
	{ "IOWRITERR", "IO Write by pid 0x!XL to blk 0x!XL of database file !AD failed.  Pid 0x!XL retrying the IO.", 5, 0 },
	{ "REPLINSTWRITE", "Error writing [0x!XL] bytes at offset [0x!16@XQ] in replication instance file !AD", 4, 0 },
	{ "DBBADFREEBLKCTR", "Database !AD free blocks counter in file header: 0x!16@XQ appears incorrect, should be 0x!16@XQ.  Auto-corrected.", 4, 2 },
	{ "REQ2RESUME", "Request to resume suspended processing received from process !UL owned by userid !UL", 2, 0 },
	{ "TIMERHANDLER", "Incorrect SIGALRM handler (0x!XJ) found by !AD", 3, 0 },
	{ "FREEMEMORY", "Error occurred freeing memory from 0x!XJ", 1, 0 },
	{ "MUREPLSECDEL", "Replication section !AD deleted", 2, 0 },
	{ "MUREPLSECNOTDEL", "Replication section !AD not deleted", 2, 0 },
	{ "MUJPOOLRNDWNSUC", "Jnlpool section (id = !AD) belonging to the replication instance !AD successfully rundown", 4, 0 },
	{ "MURPOOLRNDWNSUC", "Recvpool section (id = !AD) belonging to the replication instance !AD successfully rundown", 4, 0 },
	{ "MUJPOOLRNDWNFL", "Jnlpool section (id = !AD) belonging to the replication instance !AD rundown failed", 4, 0 },
	{ "MURPOOLRNDWNFL", "Recvpool section (id = !AD) belonging to the replication instance !AD rundown failed", 4, 0 },
	{ "MUREPLPOOL", "Error with replpool section !AD", 2, 0 },
	{ "REPLACCSEM", "Error with replication access semaphore (id = !UL) for instance file !AD", 3, 0 },
	{ "JNLFLUSHNOPROG", "No progress while attempting to flush journal file !AD", 2, 0 },
	{ "REPLINSTCREATE", "Error creating replication instance file !AD", 2, 0 },
	{ "SUSPENDING", "Process Received Signal !UL. Suspending processing on user request or attempt to do terminal I/O while running in the background", 1, 0 },
	{ "SOCKBFNOTEMPTY", "Socket buffer size cannot be set to 0x!XL due to 0x!XL bytes of buffered data.  Read first.", 2, 0 },
	{ "ILLESOCKBFSIZE", "The specified socket buffer size is 0x!XL, which is either 0 or too big", 1, 0 },
	{ "NOSOCKETINDEV", "There is no socket in the current socket device", 0, 0 },
	{ "SETSOCKOPTERR", "Setting the socket attribute !AD failed: (errno == !UL) !AD", 5, 0 },
	{ "GETSOCKOPTERR", "Getting the socket attribute !AD failed: (errno == !UL) !AD", 5, 0 },
	{ "NOSUCHPROC", "Process !UL does not exist - no need to !AD it", 3, 0 },
	{ "DSENOFINISH", "DSE unable to finish all requested actions", 0, 0 },
	{ "LKENOFINISH", "LKE unable to finish all requested actions", 0, 0 },
	{ "NOCHLEFT", "Unhandled condition exception (all handlers exhausted) - process terminating", 0, 0 },
	{ "MULOGNAMEDEF", "Logical name !AD, needed to start replication server is already defined for this job.  !/Check for an existing or improperly terminated server.", 2, 0 },
	{ "BUFOWNERSTUCK", "Pid !UL waiting for Pid !UL to finish disk-read of block !@UQ [0x!16@XQ].!/Been waiting for !UL minutes.  read_in_progress=!UL : rip_latch = !UL.", 7, 0 },
	{ "ACTIVATEFAIL", "Cannot activate passive source server on instance !AD while a receiver server and/or update process is running", 2, 0 },
	{ "DBRNDWNWRN", "Global section of database file !AD not rundown successfully by pid !UL [0x!XL].  Global section was not removed.", 4, 0 },
	{ "DLLNOOPEN", "Failed to load external dynamic library !AD", 2, 0 },
	{ "DLLNORTN", "Failed to look up the location of the symbol !AD", 2, 0 },
	{ "DLLNOCLOSE", "Failed to unload external dynamic library", 0, 0 },
	{ "FILTERNOTALIVE", "Replication server detected that the filter is not alive while attempting to send seqno !16@XQ", 1, 0 },
	{ "FILTERCOMM", "Error communicating seqno !16@XQ with the filter", 1, 0 },
	{ "FILTERBADCONV", "Bad conversion of seqno !16@XQ by filter", 1, 0 },
	{ "PRIMARYISROOT", "Attempted operation not valid on root primary instance !AD", 2, 0 },
	{ "GVQUERYGETFAIL", "Global variable QUERY and GET failed.  Failure code: !AD.", 2, 0 },
	{ "UNUSEDMSG1051", "DBCREC2BIGINBLK removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "MERGEDESC", "Merge operation not possible.  !AD is descendent of !AD.", 4, 0 },
	{ "MERGEINCOMPL", "Error encountered during MERGE; operation may be incomplete", 0, 0 },
	{ "DBNAMEMISMATCH", "Database file !AD points to shared memory (id = !UL) which in turn points to an inaccessible database file !AZ", 4, 0 },
	{ "DBIDMISMATCH", "Database file !AZ (region !AD) id does not match file id in shared memory (id = !UL).", 4, 0 },
	{ "DEVOPENFAIL", "Error opening !AD", 2, 0 },
	{ "IPCNOTDEL", "!AD : !AD did not delete IPC resources for region !AD", 6, 0 },
	{ "XCVOIDRET", "Attempt to return a value from function !AD, which is declared void in external call table !AD", 4, 0 },
	{ "MURAIMGFAIL", "Mupip recover or rollback failed while processing an after-image journal record.  Failure code: !AD.", 2, 0 },
	{ "REPLINSTUNDEF", "Replication instance environment variable $ydb_repl_instance/$gtm_repl_instance is undefined", 0, 0 },
	{ "REPLINSTACC", "Error accessing replication instance file !AD", 2, 0 },
	{ "NOJNLPOOL", "No journal pool info found in the replication instance of !AD", 2, 0 },
	{ "NORECVPOOL", "No receiver pool info found in the replication instance of !AD", 2, 0 },
	{ "FTOKERR", "Error getting ftok of the file !AD", 2, 0 },
	{ "REPLREQRUNDOWN", "Error accessing replication instance !AD.  Must be rundown on cluster node !AD.", 4, 0 },
	{ "BLKCNTEDITFAIL", "Mupip recover or rollback failed to correct the block count field in the file header for file !AD", 2, 0 },
	{ "SEMREMOVED", "Semaphore id !UL removed from the system", 1, 0 },
	{ "REPLINSTFMT", "Format error encountered while reading replication instance file !AD. Expected !AD. Found !AD.", 6, 0 },
	{ "SEMKEYINUSE", "Semaphore key 0x!XL is already in use (possibly by an older version)", 1, 0 },
	{ "XTRNTRANSERR", "Error attempting to generate an environment using an external algorithm", 0, 0 },
	{ "XTRNTRANSDLL", "Error during extended reference environment translation.  Check the above message.", 0, 0 },
	{ "XTRNRETVAL", "Length of return value (!SL) from extended reference translation algorithm is not in the range [0,!UL]", 2, 0 },
	{ "XTRNRETSTR", "Return string from extended reference translation algorithm is NULL", 0, 0 },
	{ "INVECODEVAL", "Invalid value for $ECODE (!AD)", 2, 0 },
	{ "SETECODE", "Non-empty value assigned to $ECODE (user-defined error trap)", 0, 0 },
	{ "INVSTACODE", "Invalid value for second parameter of $STACK (!AD)", 2, 0 },
	{ "REPEATERROR", "Repeat previous error", 0, 0 },
	{ "NOCANONICNAME", "Value is not a canonic name (!AD)", 2, 0 },
	{ "NOSUBSCRIPT", "No such subscript found (!SL)", 1, 0 },
	{ "SYSTEMVALUE", "Invalid value for $SYSTEM (!AD)", 2, 0 },
	{ "SIZENOTVALID4", "Size (in bytes) must be either 1, 2, or 4", 0, 0 },
	{ "STRNOTVALID", "Error: cannot convert !AD value to valid value", 2, 0 },
	{ "CREDNOTPASSED", "Socket message contained no passed credentials", 0, 0 },
	{ "ERRWETRAP", "Error while processing $ETRAP", 0, 0 },
	{ "TRACINGON", "Tracing already turned on", 0, 0 },
	{ "CITABENV", "Environment variable for call-in table !AD not set", 2, 0 },
	{ "CITABOPN", "Unable to open call-in table: !AD", 2, 0 },
	{ "CIENTNAME", "No label reference found for this entry in call-in table", 0, 0 },
	{ "CIRTNTYP", "Invalid return type", 0, 0 },
	{ "CIRCALLNAME", "Call-in routine name expected but not found", 0, 0 },
	{ "CIRPARMNAME", "Invalid parameter specification for call-in table", 0, 0 },
	{ "CIDIRECTIVE", "Invalid directive parameter passing.  Expected I, O or IO.", 0, 0 },
	{ "CIPARTYPE", "Invalid type specification for O/IO directive - expected pointer type", 0, 0 },
	{ "CIUNTYPE", "Unknown parameter type encountered", 0, 0 },
	{ "CINOENTRY", "No entry specified for !AD in the call-in table !AZ", 3, 0 },
	{ "JNLINVSWITCHLMT", "Journal AUTOSWITCHLIMIT [!UL] falls outside of allowed limits [!UL] and [!UL]", 3, 0 },
	{ "SETZDIR", "Cannot change working directory to !AD", 2, 0 },
	{ "JOBACTREF", "Actual parameter in job command passed by reference", 0, 0 },
	{ "ECLOSTMID", "$ECODE overflow, the first and last ecodes are retained, but some intervening ecodes have been lost", 0, 0 },
	{ "ZFF2MANY", "Number of characters specified for ZFF deviceparameter (!UL) is more than the maximum allowed (!UL)", 2, 0 },
	{ "JNLFSYNCLSTCK", "Journaling fsync lock is stuck in journal file !AD", 2, 0 },
	{ "DELIMWIDTH", "Delimiter length !UL exceeds device width !UL", 2, 0 },
	{ "DBBMLCORRUPT", "Database !AD : Bitmap blk [0x!16@XQ] is corrupt (size = [0x!XL], levl = [0x!XL], tn = [0x!16@XQ]) : Dbtn = [0x!16@XQ] : Database integrity errors likely", 7, 0 },
	{ "DLCKAVOIDANCE", "Possible deadlock detected: Database !AD : Dbtn [0x!16@XQ] : t_tries [0x!XL] : dollar_trestart [0x!XL] : now_crit [0x!XL] : TP transaction restarted", 6, 0 },
	{ "WRITERSTUCK", "Buffer flush stuck waiting for [0x!XL] concurrent writers to finish writing to database file !AD", 3, 0 },
	{ "PATNOTFOUND", "Current pattern table has no characters with pattern code !AD", 2, 0 },
	{ "INVZDIRFORM", "Invalid value (!UL) specified for ZDIR_FORM", 1, 0 },
	{ "ZDIROUTOFSYNC", "$ZDIRECTORY !AD is not the same as its cached value !AD", 4, 0 },
	{ "GBLNOEXIST", "Global !AD no longer exists", 2, 0 },
	{ "MAXBTLEVEL", "Global ^!AD in region !AD reached maximum level", 4, 0 },
	{ "INVMNEMCSPC", "Unsupported mnemonicspace !AD", 2, 0 },
	{ "JNLALIGNSZCHG", "Journal ALIGNSIZE is rounded up to !UL blocks (closest next higher power of two)", 1, 0 },
	{ "SEFCTNEEDSFULLB", "Current side effect setting does not permit full Boolean to be turned off", 0, 0 },
	{ "GVFAILCORE", "A core file is being created for later analysis if necessary", 0, 0 },
	{ "UNUSEDMSG1115", "DBCDBNOCERTIFY removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "DBFRZRESETSUC", "Freeze released successfully on database file !AD", 2, 0 },
	{ "JNLFILEXTERR", "Error during extension of journal file !AD", 2, 0 },
<<<<<<< HEAD:sr_port/merrors_ctl.c
	{ "JOBEXAMDONE", "YottaDB process !UL completed job examine to !AD", 3, 0 },
	{ "JOBEXAMFAIL", "YottaDB process !UL executing $ZJOBEXAM function failed with the preceding error message", 1, 0 },
=======
	{ "JOBEXAMDONE", "GT.M process !UL successfully executed $ZJOBEXAM() into !AD", 3, 0 },
	{ "JOBEXAMFAIL", "GT.M process !UL failed while executing $ZJOBEXAM(). Check the preceding error message for more information.", 1, 0 },
>>>>>>> eb3ea98c (GT.M V7.0-002):sr_i386/merrors_ctl.c
	{ "JOBINTRRQST", "Job interrupt requested", 0, 0 },
	{ "ERRWZINTR", "Error while processing $ZINTERRUPT", 0, 0 },
	{ "CLIERR", "!AD", 2, 0 },
	{ "REPLNOBEFORE", "NOBEFORE option cannot be used when the current replication state is ON for database file !AD", 2, 0 },
	{ "REPLJNLCNFLCT", "Journaling cannot be turned !AD on database file !AD as the replication state is !AD and must also be turned !AD in the same command", 8, 0 },
	{ "JNLDISABLE", "Specified journal option(s) cannot take effect as journaling is DISABLED on database file !AD", 2, 0 },
	{ "FILEEXISTS", "File !AD already exists", 2, 0 },
	{ "JNLSTATE", "Journaling state for !AD !AD is now !AD", 6, 0 },
	{ "REPLSTATE", "Replication state for !AD !AD is now !AD", 6, 0 },
	{ "JNLCREATE", "Journal file !AD created for !AD !AD with !AD", 8, 0 },
	{ "JNLNOCREATE", "Journal file !AD not created", 2, 0 },
	{ "JNLFNF", "Journal file !AD not found", 2, 0 },
	{ "PREVJNLLINKCUT", "Previous journal file name link set to NULL in new journal file !AD created for database file !AD", 4, 0 },
	{ "PREVJNLLINKSET", "Previous generation journal file name is changed from !AD to !AD", 4, 0 },
	{ "FILENAMETOOLONG", "File name too long", 0, 0 },
	{ "REQRECOV", "Error accessing database !AD.  Must be recovered on cluster node !AD.", 4, 0 },
	{ "JNLTRANS2BIG", "Transaction needs an estimated [!@ZQ blocks] in journal file !AD which exceeds the AUTOSWITCHLIMIT of !UL blocks", 4, 0 },
	{ "JNLSWITCHTOOSM", "Journal AUTOSWITCHLIMIT [!UL blocks] is less than Journal ALLOCATION [!UL blocks] for database file !AD", 4, 0 },
	{ "JNLSWITCHSZCHG", "Journal AUTOSWITCHLIMIT [!UL blocks] is rounded down to [!UL blocks] to equal the sum of Journal ALLOCATION [!UL blocks] and a multiple of Journal EXTENSION [!UL blocks] for database file !AD", 6, 0 },
	{ "NOTRNDMACC", "Only random access files are supported as backup files for non-incremental backup", 0, 0 },
	{ "TMPFILENOCRE", "Error in MUPIP BACKUP while trying to create temporary file !AD", 2, 0 },
	{ "UNUSEDMSG1141", "SHRMEMEXHAUSTED last used in OpenVMS", 0, 0 },
	{ "JNLSENDOPER", "pid = 0x!XL : status = 0x!XL : jpc_status = 0x!XL : jpc_status2 = 0x!XL : iosb.cond = 0x!XW", 5, 0 },
	{ "DDPSUBSNUL", "NUL characters in subscripts are not supported by DDP", 0, 0 },
	{ "DDPNOCONNECT", "Named volume set, !AD, is not connected", 2, 0 },
	{ "DDPCONGEST", "Agent congestion", 0, 0 },
	{ "DDPSHUTDOWN", "Server has shut down", 0, 0 },
	{ "DDPTOOMANYPROCS", "Maximum process limit of !UL exceeded", 1, 0 },
	{ "DDPBADRESPONSE", "DDP invalid response code: !XB; message text follows", 1, 0 },
	{ "DDPINVCKT", "Invalid format for CIRCUIT", 0, 0 },
	{ "DDPVOLSETCONFIG", "Volume Set Configuration file error", 0, 0 },
	{ "DDPCONFGOOD", "Volume Set Configuration entry accepted", 0, 0 },
	{ "DDPCONFIGNORE", "Volume Set Configuration line ignored", 0, 0 },
	{ "DDPCONFINCOMPL", "Volume Set Configuration entry incomplete", 0, 0 },
	{ "DDPCONFBADVOL", "Volume Set Configuration entry : invalid volume", 0, 0 },
	{ "DDPCONFBADUCI", "Volume Set Configuration entry : invalid uci", 0, 0 },
	{ "DDPCONFBADGLD", "Volume Set Configuration entry : invalid global directory", 0, 0 },
	{ "DDPRECSIZNOTNUM", "Maximum record size is not numeric", 0, 0 },
	{ "DDPOUTMSG2BIG", "DDP message too big to be accommodated in outbound buffer", 0, 0 },
	{ "DDPNOSERVER", "DDP Server not running on local node", 0, 0 },
	{ "MUTEXRELEASED", "Process !UL [0x!XL] has released the critical section for database !AD to avoid deadlock. $TLEVEL: !UL  t_tries: !UL", 6, 0 },
	{ "JNLCRESTATUS", "!AD at line !UL for journal file !AD, database file !AD encountered error", 7, 0 },
	{ "ZBREAKFAIL", "Could not set breakpoint at !AD due to insufficient memory", 2, 0 },
	{ "DLLVERSION", "Routine !AD in library !AD was compiled with an incompatible version of GT.M/YottaDB.  Recompile with the current version of YottaDB and re-link", 4, 0 },
	{ "INVZROENT", "!AD is neither a directory nor an object library(DLL)", 2, 0 },
	{ "DDPLOGERR", "!AD: !AD", 4, 0 },
	{ "GETSOCKNAMERR", "Getting the socket name failed from getsockname(): (errno==!UL) !AD", 3, 0 },
	{ "INVYDBEXIT", "Inappropriate invocation of ydb_exit(). Calls to ydb_exit() cannot be made from external calls.", 0, 0 },
	{ "CIMAXPARAM", "Exceeded maximum number of parameters in the call-in table entry. An M routine cannot accept more than 32 parameters.", 0, 0 },
	{ "UNUSEDMSG1171", "CITPNESTED nixed in r1.20 as part of #188", 0, 0 },
	{ "CIMAXLEVELS", "Too many nested Call-ins. Nested resources exhausted at level !UL.", 1, 0 },
	{ "JOBINTRRETHROW", "Job interrupt redelivered", 0, 0 },
	{ "STARFILE", "Star(*) argument cannot be specified with !AD", 2, 0 },
	{ "NOSTARFILE", "Only star(*) argument can be specified with !AD", 2, 0 },
	{ "MUJNLSTAT", "!AD at !AD", 4, 0 },
	{ "JNLTPNEST", "Mupip journal command found nested TP transactions for journal file !AD at offset 0x!XL at transaction number 0x!16@XQ", 4, 0 },
	{ "REPLOFFJNLON", "Replication state for database file !AD is OFF but journaling state is enabled", 2, 0 },
	{ "FILEDELFAIL", "Deletion of file !AD failed", 2, 0 },
	{ "INVQUALTIME", "Invalid time qualifier value.  Specify as !AD=delta_or_absolute_time.", 2, 0 },
	{ "NOTPOSITIVE", "!AD qualifier must be given a value greater than zero", 2, 0 },
	{ "INVREDIRQUAL", "Invalid REDIRECT qualifier value.  !AD", 2, 0 },
	{ "INVERRORLIM", "Invalid ERROR_LIMIT qualifier value.  Must be at least zero", 0, 0 },
	{ "INVIDQUAL", "Invalid ID qualifier value !AD", 2, 0 },
	{ "INVTRNSQUAL", "Invalid TRANSACTION qualifier.  Specify only one of TRANSACTION=[NO]SET or TRANSACTION=[NO]KILL.", 0, 0 },
	{ "JNLNOBIJBACK", "MUPIP JOURNAL BACKWARD cannot continue as journal file !AD does not have before image journaling", 2, 0 },
	{ "SETREG2RESYNC", "Setting resync sequence number 0x!16@XQ to region sequence number 0x!16@XQ for database !AD", 4, 0 },
	{ "JNLALIGNTOOSM", "Alignsize !UL (bytes) is too small for a block size of !UL (bytes) for !AD !AD.  Using alignsize of !UL (bytes) instead.", 7, 0 },
	{ "JNLFILEOPNERR", "Error opening journal file !AD", 2, 0 },
	{ "JNLFILECLOSERR", "Error closing journal file !AD", 2, 0 },
	{ "REPLSTATEOFF", "MUPIP JOURNAL -ROLLBACK -BACKWARD cannot proceed as database !AD does not have replication ON", 2, 0 },
	{ "MUJNLPREVGEN", "Previous generation journal file !AD included for database file !AD", 4, 0 },
	{ "MUPJNLINTERRUPT", "Database file !AD indicates interrupted MUPIP JOURNAL command.  Restore from backup for forward recover/rollback.", 2, 0 },
	{ "ROLLBKINTERRUPT", "Database file !AD indicates interrupted ROLLBACK.  Reissue the MUPIP JOURNAL ROLLBACK command.", 2, 0 },
	{ "RLBKJNSEQ", "Journal seqno of the instance after rollback is !@ZQ [0x!16@XQ]", 2, 0 },
	{ "REPLRECFMT", "Replication journal record format error encountered", 0, 0 },
	{ "PRIMARYNOTROOT", "Attempted operation not valid on non-root primary instance !AD", 2, 0 },
	{ "DBFRZRESETFL", "Freeze release failed on database file !AD", 2, 0 },
	{ "JNLCYCLE", "Journal file !AD causes cycle in the journal file generations of database file !AD", 4, 0 },
	{ "JNLPREVRECOV", "Journal file has nonzero value in prev_recov_end_of_data field", 0, 0 },
	{ "RESOLVESEQNO", "Resolving until sequence number !@ZQ [0x!16@XQ]", 2, 0 },
	{ "BOVTNGTEOVTN", "Journal file !AD has beginning transaction [0x!16@XQ] which is greater than end transaction [0x!16@XQ]", 4, 0 },
	{ "BOVTMGTEOVTM", "Journal file !AD has beginning timestamp [0x!16@XQ] greater than end timestamp [0x!16@XQ]", 4, 0 },
	{ "BEGSEQGTENDSEQ", "Journal file !AD has beginning sequence number [0x!16@XQ] greater than end sequence number [0x!16@XQ]", 4, 0 },
	{ "DBADDRALIGN", "Database file !AD, element location 0x!XJ: blk = 0x!16@XQ: [!AD] control 0x!XJ was unaligned relative to base 0x!XJ and element size 0x!XL", 9, 0 },
	{ "DBWCVERIFYSTART", "Database file !AD, write cache verification started by pid !UL [0x!XL] at transaction number 0x!16@XQ", 5, 0 },
	{ "DBWCVERIFYEND", "Database file !AD, write cache verification finished by pid !UL [0x!XL] at transaction number 0x!16@XQ", 5, 0 },
	{ "MUPIPSIG", "!AD (signal !UL) issued from process !UL [0x!XL] to process !UL [0x!XL]", 7, 0 },
	{ "HTSHRINKFAIL", "Hash table compaction failed to allocate new smaller table due to lack of memory", 0, 0 },
	{ "STPEXPFAIL", "Stringpool expansion failed. It could not expand to !UL bytes.", 1, 0 },
	{ "DBBTUWRNG", "The blocks-to-upgrade file-header field is incorrect. Expected 0x!16@XQ, found 0x!16@XQ", 2, 5 },
	{ "DBBTUFIXED", "The blocks-to-upgrade file-header field has been changed to the correct value", 0, 0 },
	{ "DBMAXREC2BIG", "Maximum record size (!UL) is too large for this block size (!UL) - Maximum is !UL", 3, 0 },
	{ "UNUSEDMSG1212", "DBCSCNNOTCMPLT removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "UNUSEDMSG1213", "DBCBADFILE removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "UNUSEDMSG1214", "DBCNOEXTND removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "UNUSEDMSG1215", "DBCINTEGERR removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "DBMINRESBYTES", "Minimum RESERVED BYTES value required for certification/upgrade is !UL - Currently is !UL", 2, 0 },
	{ "UNUSEDMSG1217", "DBCNOTSAMEDB removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "UNUSEDMSG1218", "DBCDBCERTIFIED removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "UNUSEDMSG1219", "DBCMODBLK2BIG removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "UNUSEDMSG1220", "DBCREC2BIG removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "UNUSEDMSG1221", "DBCCMDFAIL removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "UNUSEDMSG1222", "DBCKILLIP removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "UNUSEDMSG1223", "DBCNOFINISH removed from code in V7.0-000 Nov 2020", 0, 0 },
	{ "DYNUPGRDFAIL", "Unable to dynamically upgrade block 0x!16@XQ in database !AD due to lack of free space in block", 3, 0 },
	{ "MMNODYNDWNGRD", "Unable to use dynamic downgrade with MM access method for region !AD. Use BG access method for downgrade", 2, 0 },
	{ "MMNODYNUPGRD", "Unable to use MM access method for region !AD until all database blocks are upgraded", 2, 0 },
	{ "MUDWNGRDNRDY", "Database !AD is not ready to downgrade - still !@UQ database blocks to downgrade", 3, 0 },
	{ "MUDWNGRDTN", "Transaction number 0x!16@XQ in database !AD is too big for MUPIP [REORG] DOWNGRADE. Renew database with MUPIP INTEG TN_RESET", 3, 0 },
	{ "MUDWNGRDNOTPOS", "Start VBN value is [!UL] while downgraded YottaDB version can support only [!UL]. Downgrade not possible", 2, 0 },
	{ "MUUPGRDNRDY", "Database !AD has not been certified as being ready to upgrade to !AD format", 4, 0 },
	{ "TNWARN", "Database file !AD has 0x!16@XQ more transactions to go before reaching the transaction number limit (0x!16@XQ). Renew database with MUPIP INTEG TN_RESET", 4, 0 },
	{ "TNTOOLARGE", "Database file !AD has reached the transaction number limit (0x!16@XQ). Renew database with MUPIP INTEG TN_RESET", 3, 0 },
	{ "SHMPLRECOV", "Shared memory pool block recovery invoked for region !AD", 2, 0 },
	{ "MUNOSTRMBKUP", "Database !AD has a block size larger than !UL and thus cannot use stream (incremental) backup", 3, 0 },
	{ "EPOCHTNHI", "At the EPOCH record at offset !UL of !AD transaction number [0x!16@XQ] is higher than database transaction number [0x!16@XQ]", 5, 0 },
	{ "CHNGTPRSLVTM", "Mupip will change tp_resolve_time from !UL to !UL because expected EPOCH or EOF record was not found in Journal File !AD", 4, 0 },
	{ "JNLUNXPCTERR", "Unexpected error encountered for Journal !AD at disk address 0x!XL", 3, 0 },
	{ "OMISERVHANG", "GTCM OMI server is hung", 0, 0 },
	{ "RSVDBYTE2HIGH", "Record size (!UL) is greater than the maximum allowed for region !AD with Block size (!UL) and Reserved bytes (!UL)", 5, 0 },
	{ "BKUPTMPFILOPEN", "Open of backup temporary file !AD failed", 2, 0 },
	{ "BKUPTMPFILWRITE", "Write to backup temporary file !AD failed", 2, 0 },
<<<<<<< HEAD:sr_port/merrors_ctl.c
	{ "UNUSEDMSG1244", "VMSMEMORY2 nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "UNUSEDMSG1245", "LOADBGSZ2 last used in V6.3-009", 0, 0 },
	{ "UNUSEDMSG1246", "LOADEDSZ2 last used in V6.3-009", 0, 0 },
=======
	{ "UNUSEDMSG1242", "VMSMEMORY2 last used in OpenVMS", 0, 0 },
	{ "UNUSEDMSG1243", "LOADBGSZ2 last used in V6.3-009", 0, 0 },
	{ "UNUSEDMSG1244", "LOADEDSZ2 last used in V6.3-009", 0, 0 },
>>>>>>> eb3ea98c (GT.M V7.0-002):sr_i386/merrors_ctl.c
	{ "REPLINSTMISMTCH", "Process has replication instance file !AD (jnlpool shmid = !UL) open but database !AD is bound to instance file !AD (jnlpool shmid = !UL)", 8, 0 },
	{ "REPLINSTREAD", "Error reading [0x!XL] bytes at offset [0x!16@XQ] from replication instance file !AD", 4, 0 },
	{ "REPLINSTDBMATCH", "Replication instance file !AD has seqno [0x!16@XQ] while database has a different seqno [0x!16@XQ]", 4, 0 },
	{ "REPLINSTNMSAME", "Primary and Secondary instances have the same replication instance name !AD", 2, 0 },
	{ "REPLINSTNMUNDEF", "Replication instance name not defined", 0, 0 },
	{ "REPLINSTNMLEN", "Replication instance name !AD should be 1 to 15 characters long", 2, 0 },
	{ "REPLINSTNOHIST", "History information for !AD not found in replication instance file !AD", 4, 0 },
	{ "REPLINSTSECLEN", "Secondary replication instance name !AD should be 1 to 15 characters long", 2, 0 },
	{ "REPLINSTSECMTCH", "Secondary replication instance name !AD sent by receiver does not match !AD specified at source server startup", 4, 0 },
	{ "REPLINSTSECNONE", "No information found for secondary instance !AD in instance file !AD", 4, 0 },
	{ "REPLINSTSECUNDF", "Secondary replication instance name not defined", 0, 0 },
	{ "REPLINSTSEQORD", "!AD has seqno [0x!16@XQ] which is less than last record seqno [0x!16@XQ] in replication instance file !AD", 6, 0 },
	{ "REPLINSTSTNDALN", "Could not get exclusive access to replication instance file !AD", 2, 0 },
	{ "REPLREQROLLBACK", "Replication instance file !AD indicates abnormal shutdown or an incomplete ROLLBACK. Run MUPIP JOURNAL ROLLBACK first", 2, 0 },
	{ "REQROLLBACK", "Error accessing database !AD.  Run MUPIP JOURNAL -ROLLBACK -NOONLINE on cluster node !AD.", 4, 0 },
	{ "INVOBJFILE", "Cannot ZLINK object file !AD due to unexpected format", 2, 0 },
	{ "SRCSRVEXISTS", "Source server for secondary instance !AD is already running with pid !UL", 3, 0 },
	{ "SRCSRVNOTEXIST", "Source server for secondary instance !AD is not alive", 2, 0 },
	{ "SRCSRVTOOMANY", "Cannot start more than !UL source servers in replication instance !AD", 3, 0 },
	{ "JNLPOOLBADSLOT", "Source server slot for secondary instance !AD is in an inconsistent state. Pid = [!UL], State = [!UL], SlotIndex = [!UL]", 5, 0 },
	{ "NOENDIANCVT", "Unable to convert the endian format of file !AD due to !AD", 4, 0 },
	{ "ENDIANCVT", "Converted database file !AD from !AZ endian to !AZ endian on a !AZ endian system", 5, 0 },
	{ "DBENDIAN", "Database file !AD is !AZ endian on a !AZ endian system", 4, 0 },
	{ "BADCHSET", "!AD is not a valid character mapping in this context", 2, 0 },
	{ "BADCASECODE", "!AD is not a valid case conversion code", 2, 0 },
	{ "BADCHAR", "$ZCHAR(!AD) is not a valid character in the !AD encoding form", 4, 0 },
	{ "DLRCILLEGAL", "!_!AD!/!_!_!_Illegal $CHAR() value !UL", 3, 0 },
	{ "NONUTF8LOCALE", "Locale has character encoding (!AD) which is not compatible with UTF-8 character set", 2, 0 },
	{ "INVDLRCVAL", "Invalid $CHAR() value !UL", 1, 0 },
	{ "DBMISALIGN", "File header indicates total blocks is 0x!16@XQ but file size indicates total blocks would be between 0x!16@XQ and 0x!16@XQ. Reconstruct the database from a backup or extend it by at least !@UQ blocks.", 4, 0 },
	{ "LOADINVCHSET", "Extract file CHSET (!AD) is incompatible with ydb_chset/gtm_chset", 2, 0 },
	{ "DLLCHSETM", "Routine !AD in library !AD was compiled with CHSET=M which is different from $ZCHSET", 4, 0 },
	{ "DLLCHSETUTF8", "Routine !AD in library !AD was compiled with CHSET=UTF-8 which is different from $ZCHSET", 4, 0 },
	{ "BOMMISMATCH", "!AD Byte Order Marker found when !AD character set specified", 4, 0 },
	{ "WIDTHTOOSMALL", "WIDTH should be at least 2 when device ICHSET or OCHSET is UTF-8 or UTF-16", 0, 0 },
	{ "SOCKMAX", "Attempt to exceed maximum sockets (!UL) for the SOCKET device", 1, 0 },
	{ "PADCHARINVALID", "PAD deviceparameter cannot be greater than 127", 0, 0 },
	{ "ZCNOPREALLOUTPAR", "Parameter !UL in external call !AD.!AD is an output only parameter requiring pre-allocation", 5, 0 },
	{ "SVNEXPECTED", "Special variable expected in this context", 0, 0 },
	{ "SVNONEW", "Cannot NEW this special variable", 0, 0 },
	{ "ZINTDIRECT", "Attempt to enter direct mode from $ZINTERRUPT", 0, 0 },
	{ "ZINTRECURSEIO", "Attempt to do IO to the active device in $ZINTERRUPT", 0, 0 },
	{ "MRTMAXEXCEEDED", "Maximum value of !UL for SOCKET device parameter MOREREADTIME exceeded", 1, 0 },
	{ "JNLCLOSED", "Journaling closed for database file !AD at transaction number 0x!16@XQ", 3, 0 },
	{ "RLBKNOBIMG", "ROLLBACK cannot proceed as database !AD has NOBEFORE_IMAGE journaling", 2, 0 },
	{ "RLBKJNLNOBIMG", "Journal file !AD has NOBEFORE_IMAGE journaling", 2, 0 },
	{ "RLBKLOSTTNONLY", "ROLLBACK will only create a lost transaction file (database and journal files will not be modified)", 0, 0 },
	{ "KILLBYSIGSINFO3", "!AD process !UL has been killed by a signal !UL accessing vaddress 0x!XJ", 5, 0 },
	{ "GTMSECSHRTMPPATH", "gtmsecshr path is !AD", 2, 0 },
	{ "UNUSEDMSG1296", "GTMERREXIT nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "INVMEMRESRV", "Could not allocate YottaDB memory reserve (!AD)", 2, 0 },
	{ "OPCOMMISSED", "!UL errors and !UL MBFULLs sending prior operator messages", 2, 0 },
	{ "COMMITWAITSTUCK", "Pid !UL waited !UL minute(s) for !UL concurrent YottaDB process(es) to finish commits in database file !AD", 5, 0 },
	{ "COMMITWAITPID", "Pid !UL waited !UL minute(s) for pid !UL to finish commits to block 0x!16@XQ in database file !AD", 6, 0 },
	{ "UPDREPLSTATEOFF", "Error replicating global ^!AD as it maps to database !AD which has replication turned OFF", 4, 0 },
	{ "LITNONGRAPH", "M standard requires graphics in string literals; found non-printable: $ZCHAR(!AD)", 2, 0 },
	{ "DBFHEADERR8", "Database file !AD: control problem: !AD was 0x!16@XQ expecting 0x!16@XQ", 6, 0 },
	{ "MMBEFOREJNL", "BEFORE image journaling cannot be set with MM access method in database file !AD", 2, 0 },
	{ "MMNOBFORRPL", "Replication cannot be used in database file !AD which uses MM access method and NOBEFORE image journaling", 2, 0 },
	{ "KILLABANDONED", "Abandoned kills counter is greater than zero for file !AD, !AD", 4, 0 },
	{ "BACKUPKILLIP", "Kill in progress indicator is set for file !AD, backup database could have incorrectly marked busy integrity errors", 2, 0 },
	{ "LOGTOOLONG", "Environment variable !AD is too long. Maximum length allowed is !UL bytes.", 3, 0 },
	{ "NOALIASLIST", "Parenthetical lists of multiple arguments cannot have a preceding alias introducer or include alias (*) forms", 0, 0 },
	{ "ALIASEXPECTED", "Alias or alias container variable expected in this context", 0, 0 },
	{ "VIEWLVN", "Invalid local variable name used with VIEW/$VIEW(): !AD", 2, 0 },
	{ "DZWRNOPAREN", "$ZWRTACxxx is not allowed inside a parenthesized SET target", 0, 0 },
	{ "DZWRNOALIAS", "$ZWRTAC cannot be aliased", 0, 0 },
	{ "FREEZEERR", "Error while trying to !AD region !AD", 4, 0 },
	{ "CLOSEFAIL", "Error while closing file descriptor !SL", 1, 0 },
	{ "CRYPTINIT", "Could not initialize encryption library while opening encrypted file !AD. !AD", 4, 0 },
	{ "CRYPTOPFAILED", "Encrypt/Decrypt operation failed for file !AD. !AD", 4, 0 },
	{ "CRYPTDLNOOPEN", "Could not load encryption library while opening encrypted file !AD. !AD", 4, 0 },
	{ "CRYPTNOV4", "!AD is an encrypted database. Cannot downgrade(to V4) with Encryption option enabled.", 2, 0 },
	{ "CRYPTNOMM", "!AD is an encrypted database. Cannot support MM access method.", 2, 0 },
	{ "READONLYNOBG", "Read-only cannot be enabled on non-MM databases", 0, 0 },
	{ "CRYPTKEYFETCHFAILED", "Could not retrieve encryption key corresponding to file !AD. !AD", 4, 0 },
	{ "CRYPTKEYFETCHFAILEDNF", "Could not retrieve encryption key during !AD operation key. !AD", 4, 0 },
	{ "CRYPTHASHGENFAILED", "Could not generate cryptographic hash for symmetric key corresponding to file !AD. !AD", 4, 0 },
	{ "CRYPTNOKEY", "No encryption key specified", 0, 0 },
	{ "BADTAG", "Unable to use file !AD (CCSID !UL) with CCSID !UL", 4, 0 },
	{ "ICUVERLT36", "!AD !UL.!UL. ICU version greater than or equal to 3.6 should be used", 4, 0 },
	{ "ICUSYMNOTFOUND", "Symbol !AD not found in the ICU libraries. ICU needs to be built with symbol-renaming disabled or !AD environment variable needs to be properly specified", 4, 0 },
	{ "STUCKACT", "Process stuck script invoked: !AD : !AD", 4, 0 },
	{ "CALLINAFTERXIT", "After a ydb_exit(), a process cannot create a valid YottaDB context", 0, 0 },
	{ "LOCKSPACEFULL", "No more room for LOCK slots on database file !AD", 2, 0 },
	{ "IOERROR", "Error occured while doing !AD in !AD operation -- called from module !AD at line !UL", 7, 0 },
	{ "MAXSSREACHED", "Maximum snapshots - !UL - for region !AD reached. Please wait for the existing snapshots to complete before starting a new one.", 3, 0 },
	{ "SNAPSHOTNOV4", "Cannot downgrade (to V4) while snapshots are in progress. Currently !UL snapshots are in progress for region !AD.", 3, 0 },
	{ "SSV4NOALLOW", "Database snapshots are supported only on fully upgraded V5 databases. !AD has V4 format blocks.", 2, 0 },
	{ "SSTMPDIRSTAT", "Cannot access temporary directory !AD", 2, 0 },
	{ "SSTMPCREATE", "Cannot create the temporary file in directory !AD for the requested snapshot", 2, 0 },
	{ "JNLFILEDUP", "Journal files !AD and !AD are the same", 4, 0 },
	{ "SSPREMATEOF", "Premature end of file while reading block !@UQ of size: !UL bytes at offset: !UL from !AD", 5, 0 },
	{ "SSFILOPERR", "Error while doing !AD operation on file !AD", 4, 0 },
	{ "REGSSFAIL", "Process !UL encountered error !UL contributing to the snapshot for region !AD - the snapshot is no longer valid", 4, 0 },
	{ "SSSHMCLNUPFAIL", "Error while doing snapshot shared memory cleanup. Operation -- !AD. Identifier -- !UL", 3, 0 },
	{ "SSFILCLNUPFAIL", "Error while unlinking snapshot file -- !AD", 2, 0 },
	{ "SETINTRIGONLY", "ISV !AD cannot be modified outside of the trigger environment", 2, 0 },
	{ "MAXTRIGNEST", "Maximum trigger nesting level (!UL) exceeded", 1, 0 },
	{ "TRIGCOMPFAIL", "Compilation of database trigger named !AD failed", 2, 0 },
	{ "NOZTRAPINTRIG", "Use of $ZTRAP in a database trigger environment ($ZTLEVEL greater than 0) is not supported", 0, 0 },
	{ "ZTWORMHOLE2BIG", "String length of !UL bytes exceeds maximum length of !UL bytes for $ZTWORMHOLE", 2, 0 },
	{ "JNLENDIANLITTLE", "Journal file !AD is LITTLE endian on a BIG endian system", 2, 0 },
	{ "JNLENDIANBIG", "Journal file !AD is BIG endian on a LITTLE endian system", 2, 0 },
	{ "TRIGINVCHSET", "Trigger !AD for global ^!AD was created with CHSET=!AD which is different from the current $ZCHSET of this process", 6, 0 },
	{ "TRIGREPLSTATE", "Trigger cannot update replicated database file !AD since triggering update was not replicated", 2, 0 },
	{ "GVDATAGETFAIL", "Global variable DATAGET sub-operation (in KILL function) failed. Failure code: !AD.", 2, 0 },
	{ "TRIG2NOTRIG", "Sending transaction sequence number 0x!16@XQ which used triggers to a replicator that does not support triggers", 1, 0 },
	{ "ZGOTOINVLVL", "ZGOTO in a trigger running in !AD cannot ZGOTO level !UL", 3, 0 },
	{ "TRIGTCOMMIT", "TCOMMIT at $ZTLEVEL=!UL not allowed as corresponding TSTART was done at lower $ZTLEVEL=!UL", 2, 0 },
	{ "TRIGTLVLCHNG", "Detected a net transaction level ($TLEVEL) change during trigger !AD. Transaction level must be the same at exit as when the trigger started", 2, 0 },
	{ "TRIGNAMEUNIQ", "Unable to make trigger name !AD unique beyond !UL versions already loaded", 3, 0 },
	{ "ZTRIGINVACT", "Missing or invalid parameter in position !UL given to $ZTRIGGER()", 1, 0 },
	{ "INDRCOMPFAIL", "Compilation of indirection failed", 0, 0 },
	{ "QUITALSINV", "QUIT * return when the extrinsic was not invoked with SET *", 0, 0 },
	{ "PROCTERM", "!AD process termination due to !AZ (return code !UL) from !AD", 6, 0 },
	{ "SRCLNNTDSP", "Source lines exceeding !UL character width are not displayed", 1, 0 },
	{ "ARROWNTDSP", "Unable to display ^----- due to length of source line", 0, 0 },
	{ "TRIGDEFBAD", "Trigger initialization failed for global ^!AD. Error while processing ^#t(\"!AD\",!AD)", 6, 0 },
	{ "TRIGSUBSCRANGE", "Trigger definition for global ^!AD has one or more invalid subscript range(s) : !AD", 4, 0 },
	{ "TRIGDATAIGNORE", "Ignoring trigger data !AD. Use MUPIP TRIGGER to load trigger definitions", 2, 0 },
	{ "TRIGIS", "!_!_Trigger name: !AD", 2, 0 },
	{ "TCOMMITDISALLOW", "TROLLBACK required after an unhandled error in trigger context", 0, 0 },
	{ "SSATTACHSHM", "Error while attaching to shared memory identifier !UL", 1, 0 },
	{ "TRIGDEFNOSYNC", "Global ^!AD has triggers defined on the !AD instance but none on the !AD instance. Current journal sequence number is 0x!16@XQ", 7, 0 },
	{ "TRESTMAX", "TRESTART not allowed in a final TP retry more than once", 0, 0 },
	{ "ZLINKBYPASS", "ZLINK of !AD bypassed - Identical routine already linked", 2, 0 },
	{ "GBLEXPECTED", "Global variable reference expected in this context", 0, 0 },
	{ "GVZTRIGFAIL", "ZTRIGGER of a global variable failed.  Failure code: !AD.", 2, 0 },
	{ "MUUSERLBK", "Abnormal shutdown of replication-enabled database !AD detected", 2, 0 },
	{ "SETINSETTRIGONLY", "ISV !AD can only be modified in a 'SET' type trigger", 2, 0 },
	{ "DZTRIGINTRIG", "$ZTRIGGER() is not allowed inside trigger context. Trigger name: !AD", 2, 0 },
	{ "LSINSERTED", "Line !UL, source module !AD exceeds maximum source line length; line seperator inserted, terminating scope of any prior IF, ELSE, or FOR", 3, 0 },
	{ "BOOLSIDEFFECT", "Extrinsic ($$), External call ($&) or $INCREMENT() with potential side effects in Boolean expression", 0, 0 },
	{ "DBBADUPGRDSTATE", "Correcting conflicting values for fields describing database version upgrade state in the file header for region !AD (!AD) - make fresh backups with new journal files immediately.", 4, 0 },
	{ "WRITEWAITPID", "PID !UL waited !UL minute(s) for PID !UL to finish writing block 0x!16@XQ in database file !AD", 6, 0 },
	{ "ZGOCALLOUTIN", "ZGOTO level 0 with entry ref not valid when using call-ins", 0, 0 },
	{ "UNUSEDMSG1384", "REPLNOXENDIAN nixed in r1.24 because oldest supported version is V6.0-000 which supports cross-endian replication", 0, 0 },
	{ "REPLXENDIANFAIL", "!AD side encountered error while doing endian conversion at journal sequence number 0x!16@XQ", 3, 0 },
	{ "UNUSEDMSG1386", "ZGOTOINVLVL2 nixed in r1.20 because it is a VMS only error", 0, 0 },
	{ "GTMSECSHRCHDIRF", "gtmsecshr unable to chdir to its temporary directory (!AD)", 2, 0 },
	{ "JNLORDBFLU", "Error flushing database blocks to !AD. See related messages in the operator log", 2, 0 },
	{ "ZCCLNUPRTNMISNG", "External call: Cleanup routine name missing. Cannot continue", 0, 0 },
	{ "ZCINVALIDKEYWORD", "External call: Invalid keyword found. Cannot continue", 0, 0 },
	{ "REPLMULTINSTUPDATE", "Previous updates in the current transaction are to !AD so updates to !AD (in !AD) not allowed", 6, 0 },
	{ "DBSHMNAMEDIFF", "Database file !AD points to shared memory (id = !UL) which points to a different database file !AZ", 4, 0 },
	{ "SHMREMOVED", "Removed Shared Memory id !UL corresponding to file !AD", 3, 0 },
	{ "DEVICEWRITEONLY", "Cannot read from a write-only device", 0, 0 },
	{ "ICUERROR", "ICU returned status !UL which is either unrecognized or inconsistent with the operating context", 1, 0 },
	{ "ZDATEBADDATE", "$ZDATE() date argument !AD is less than -365 (the $HOROLOG value for 01-JAN-1840) or greater than 364570088 (the $HOROLOG value for 31-DEC-999999)", 2, 0 },
	{ "ZDATEBADTIME", "$ZDATE() time argument !AD is less than 0 or greater than 86399 (the $HOROLOG value for a second before midnight)", 2, 0 },
	{ "COREINPROGRESS", "Previous core attempt failed; core generation bypassed", 0, 0 },
	{ "MAXSEMGETRETRY", "Failed to get ftok semaphore after !UL tries because it is being continually deleted", 1, 0 },
	{ "JNLNOREPL", "Replication not enabled for journal file !AD (database file !AD)", 4, 0 },
	{ "JNLRECINCMPL", "Incomplete journal record at disk address 0x!XL for file !AD while attempting to read seqno 0x!16@XQ", 4, 0 },
	{ "JNLALLOCGROW", "Increased Journal ALLOCATION from [!UL blocks] to [!UL blocks] to match AUTOSWITCHLIMIT for !AZ !AD", 5, 0 },
	{ "INVTRCGRP", "Invalid trace group specified in $ydb_trace_groups/$gtm_trace_groups: !AD", 2, 0 },
	{ "MUINFOUINT6", "!AD : !UL [0x!XL] ; $H=!UL,!UL", 6, 0 },
	{ "NOLOCKMATCH", "No matching locks were found in !AD", 2, 0 },
	{ "BADREGION", "Region is not BG, MM, or CM", 0, 0 },
	{ "LOCKSPACEUSE", "Estimated free lock space: !UL% of !UL pages", 2, 0 },
	{ "JIUNHNDINT", "An error during $ZINTERRUPT processing was not handled: !AD", 2, 0 },
	{ "GTMASSERT2", "!AD - Assert failed !AD line !UL for expression (!AD)", 7, 0 },
	{ "ZTRIGNOTRW", "ZTRIGGER cannot operate on read-only region !AD", 2, 0 },
	{ "TRIGMODREGNOTRW", "Trigger(s) cannot be added/changed/deleted/upgraded because region !AD is read-only", 2, 0 },
	{ "INSNOTJOINED", "Replicating Instance !AD is not a member of the same Group as Instance !AD", 4, 0 },
	{ "INSROLECHANGE", "Supplementary Instance !AD and non-Supplementary Instance !AD belong to the same Group", 4, 0 },
	{ "INSUNKNOWN", "Supplementary Instance !AD has no instance definition for non-Supplementary Instance !AD", 4, 0 },
	{ "NORESYNCSUPPLONLY", "NORESYNC only supported for Supplementary Instances", 0, 0 },
	{ "NORESYNCUPDATERONLY", "NORESYNC qualifier only allowed on a Supplementary Instance which allows local updates", 0, 0 },
	{ "NOSUPPLSUPPL", "Instance !AD is configured to perform local updates so it cannot receive from Supplementary Instance !AD", 4, 0 },
	{ "REPL2OLD", "Instance !AD uses a GT.M/YottaDB version that does not support connection with the current version on instance !AD", 4, 0 },
	{ "EXTRFILEXISTS", "Error opening output file: !AD -- File exists", 2, 0 },
	{ "MUUSERECOV", "Abnormal shutdown of journaled database !AD detected", 2, 0 },
	{ "SECNOTSUPPLEMENTARY", "!AD is a Supplementary Instance and so cannot act as a source to non-Supplementary Instance !AD ", 4, 0 },
	{ "SUPRCVRNEEDSSUPSRC", "Instance !AD is not configured to perform local updates so it cannot act as a receiver for non-Supplementary Instance !AD", 4, 0 },
	{ "PEERPIDMISMATCH", "Local socket peer with PID=!UL does not match specified PID=!UL", 2, 0 },
	{ "SETITIMERFAILED", "A setitimer() call returned an error status of !UL", 1, 0 },
	{ "UPDSYNC2MTINS", "Can only UPDATERESYNC with an empty instance file", 0, 0 },
	{ "UPDSYNCINSTFILE", "Error with instance file name specified in UPDATERESYNC qualifier", 0, 0 },
	{ "REUSEINSTNAME", "Error with instance name specified in REUSE qualifier", 0, 0 },
	{ "RCVRMANYSTRMS", "Receiver server now connecting to source stream [!2UL] but had previously connected to a different stream [!2UL]", 2, 0 },
	{ "RSYNCSTRMVAL", "RSYNC_STRM qualifier can only take on a value from 0 to 15", 0, 0 },
	{ "RLBKSTRMSEQ", "Stream journal seqno of the instance after rollback is Stream !2UL : Seqno !@ZQ [0x!16@XQ]", 3, 0 },
	{ "RESOLVESEQSTRM", "Resolving until stream sequence number Stream !2UL : Seqno !@ZQ [0x!16@XQ]", 3, 0 },
	{ "REPLINSTDBSTRM", "Replication instance file !AD has seqno [0x!16@XQ] for Stream !2UL while database has a different seqno [0x!16@XQ]", 5, 0 },
	{ "RESUMESTRMNUM", "Error with stream number specified in RESUME qualifier", 0, 0 },
	{ "ORLBKSTART", "ONLINE ROLLBACK started on instance !AD corresponding to !AD", 4, 0 },
	{ "ORLBKTERMNTD", "ONLINE ROLLBACK terminated on instance !AD corresponding to !AD with the above errors", 4, 0 },
	{ "ORLBKCMPLT", "ONLINE ROLLBACK completed successfully on instance !AD corresponding to !AD", 4, 0 },
	{ "ORLBKNOSTP", "ONLINE ROLLBACK proceeding with database updates. MUPIP STOP will no longer be allowed", 0, 0 },
	{ "ORLBKFRZPROG", "!AD : waiting for FREEZE on region !AD (!AD) to clear", 6, 0 },
	{ "ORLBKFRZOVER", "!AD : FREEZE on region !AD (!AD) cleared", 6, 0 },
	{ "ORLBKNOV4BLK", "Region !AD (!AD) has V4 format blocks. Database upgrade required. ONLINE ROLLBACK cannot continue", 4, 0 },
	{ "DBROLLEDBACK", "Concurrent ONLINE ROLLBACK detected on one or more regions. The current operation is no longer valid", 0, 0 },
	{ "DSEWCREINIT", "Database cache reinitialized by DSE for region !AD", 2, 0 },
	{ "MURNDWNOVRD", "OVERRIDE qualifier used with MUPIP RUNDOWN on database file !AD", 2, 0 },
	{ "REPLONLNRLBK", "ONLINE ROLLBACK detected. Starting afresh", 0, 0 },
	{ "SRVLCKWT2LNG", "PID !UL is holding the source server lock. Waited for !UL seconds. Now exiting", 2, 0 },
	{ "IGNBMPMRKFREE", "Ignoring bitmap free-up operation for region !AD (!AD) due to concurrent ONLINE ROLLBACK", 4, 0 },
	{ "PERMGENFAIL", "Failed to determine access permissions to use for creation of !AD for file !AD", 4, 0 },
	{ "PERMGENDIAG", "Permissions: Proc(uid:!UL,gid:!UL!AD), DB File(uid:!UL,gid:!UL,perm:!AD), Lib File(gid:!UL,perm:!AD)", 11, 0 },
	{ "MUTRUNC1ATIME", "Process with PID !UL already performing truncate in region !AD", 3, 0 },
	{ "MUTRUNCBACKINPROG", "Truncate detected concurrent backup in progress for region !AD", 2, 0 },
	{ "MUTRUNCERROR", "Truncate of region !AD encountered service error !AD", 4, 0 },
	{ "MUTRUNCFAIL", "Truncate failed after reorg", 0, 0 },
	{ "MUTRUNCNOSPACE", "Region !AD has insufficient space to meet truncate target percentage of !UL", 3, 0 },
	{ "MUTRUNCNOTBG", "Region !AD does not have access method BG ", 2, 0 },
	{ "MUTRUNCNOV4", "Region !AD is not fully upgraded from V4 format.", 2, 0 },
	{ "MUTRUNCPERCENT", "Truncate threshold percentage should be from 0 to 99", 0, 0 },
	{ "MUTRUNCSSINPROG", "Truncate detected concurrent snapshot in progress for region !AD", 2, 0 },
	{ "MUTRUNCSUCCESS", "Database file !AD truncated from 0x!16@XQ blocks to 0x!16@XQ at transaction 0x!16@XQ", 5, 0 },
	{ "RSYNCSTRMSUPPLONLY", "RSYNC_STRM qualifier only supported for Supplementary Instances", 0, 0 },
	{ "STRMNUMIS", "Stream # is !2UL", 1, 0 },
	{ "STRMNUMMISMTCH1", "Stream !2UL exists on the receiver instance file but is unknown on the source instance", 1, 0 },
	{ "STRMNUMMISMTCH2", "Stream !2UL exists on the source instance file but is unknown on the receiver instance", 1, 0 },
	{ "STRMSEQMISMTCH", "Unable to play update on Stream !2UL with seqno [0x!16@XQ] as receiving instance has a different stream seqno [0x!16@XQ]", 3, 0 },
	{ "LOCKSPACEINFO", "Region: !AD: processes on queue: !UL/!UL; LOCK slots in use: !UL/!UL; SUBSCRIPT slot bytes in use: !UL/!UL", 8, 0 },
	{ "JRTNULLFAIL", "Applying NULL journal record failed.  Failure code: !AD.", 2, 0 },
	{ "LOCKSUB2LONG", "Following subscript is !UL bytes long which exceeds 255 byte limit.", 1, 0 },
	{ "RESRCWAIT", "Waiting briefly for the !AD semaphore for region !AD (!AD) was held by PID !UL (Sem. ID: !UL).", 8, 0 },
	{ "RESRCINTRLCKBYPAS", "!AD with PID !UL bypassing the !AD semaphore for region !AD (!AD) was held by PID !UL.", 10, 0 },
	{ "DBFHEADERRANY", "Database file !AD: control problem: !AD was 0x!XJ expecting 0x!XJ", 6, 0 },
	{ "REPLINSTFROZEN", "Instance !AZ is now Frozen", 1, 0 },
	{ "REPLINSTFREEZECOMMENT", "Freeze Comment: !AZ", 1, 0 },
	{ "REPLINSTUNFROZEN", "Instance !AZ is now Unfrozen", 1, 0 },
	{ "DSKNOSPCAVAIL", "Attempted write to file !AD failed due to lack of disk space. Retrying indefinitely.", 2, 0 },
	{ "DSKNOSPCBLOCKED", "Retry of write to file !AD suspended due to new instance freeze. Waiting for instance to be unfrozen.", 2, 0 },
	{ "DSKSPCAVAILABLE", "Write to file !AD succeeded after out-of-space condition cleared", 2, 0 },
	{ "ENOSPCQIODEFER", "Write to file !AD deferred due to lack of disk space", 2, 0 },
	{ "CUSTOMFILOPERR", "Error while doing !AD operation on file !AD", 4, 0 },
	{ "CUSTERRNOTFND", "Error mnemonic !AD specified in custom errors file is not valid for this version of YottaDB", 2, 0 },
	{ "CUSTERRSYNTAX", "Syntax error in file !AD at line number !UL", 3, 0 },
	{ "ORLBKINPROG", "Online ROLLBACK in progress by PID !UL in region !AD", 3, 0 },
	{ "DBSPANGLOINCMP", "!AD Spanning node is missing. Block no !UL of spanning node is missing", 3, 4 },
	{ "DBSPANCHUNKORD", "!AD Chunk of !UL blocks is out of order", 3, 4 },
	{ "DBDATAMX", "!AD Record too large", 2, 4 },
	{ "DBIOERR", "Error while doing write operation on region !AD (!AD)", 4, 0 },
	{ "INITORRESUME", "UPDATERESYNC on a Supplementary Instance must additionally specify INITIALIZE or RESUME", 0, 0 },
	{ "GTMSECSHRNOARG0", "gtmsecshr cannot identify its origin - argv[0] is null", 0, 0 },
	{ "GTMSECSHRISNOT", "gtmsecshr is not running as gtmsecshr but !AD - must be gtmsecshr", 2, 0 },
	{ "GTMSECSHRBADDIR", "gtmsecshr is not running from $ydb_dist/gtmsecshrdir or $ydb_dist cannot be determined", 0, 0 },
	{ "JNLBUFFREGUPD", "Journal file buffer size for region !AD has been adjusted from !UL to !UL.", 4, 0 },
	{ "JNLBUFFDBUPD", "Journal file buffer size for database file !AD has been adjusted from !UL to !UL.", 4, 0 },
	{ "LOCKINCR2HIGH", "Attempt to increment a LOCK more than !UL times", 1, 0 },
	{ "LOCKIS", "!_!_Resource name: !AD", 2, 0 },
	{ "LDSPANGLOINCMP", "Incomplete spanning node found during load!/!_!_at File offset : [0x!16@XQ]", 1, 0 },
	{ "MUFILRNDWNFL2", "Database section (id = !UL) belonging to database file !AD rundown failed", 3, 0 },
	{ "MUINSTFROZEN", "!AD : Instance !AZ is frozen. Waiting for instance to be unfrozen before proceeding with writes to database file !AD", 5, 0 },
	{ "MUINSTUNFROZEN", "!AD : Instance !AZ is now Unfrozen. Continuing with writes to database file !AD", 5, 0 },
	{ "GTMEISDIR", "!AD : Is a directory", 2, 0 },
	{ "SPCLZMSG", "The following error message cannot be driven through ZMESSAGE", 0, 0 },
	{ "MUNOTALLINTEG", "At least one region skipped. See the earlier messages", 0, 0 },
	{ "BKUPRUNNING", "Process !UL is currently backing up region !AD. Cannot start another backup.", 3, 0 },
	{ "MUSIZEINVARG", "MUPIP SIZE : Invalid parameter value for: !AD", 2, 0 },
	{ "MUSIZEFAIL", "MUPIP SIZE : failed.  Failure code: !AD.", 2, 0 },
	{ "SIDEEFFECTEVAL", "Extrinsic ($$), External call ($&) or $INCREMENT() with potential side effects in actuallist, function arguments, non-Boolean binary operands or subscripts", 0, 0 },
	{ "CRYPTINIT2", "Could not initialize encryption library !AD. !AD", 4, 0 },
	{ "CRYPTDLNOOPEN2", "Could not load encryption library !AD. !AD", 4, 0 },
	{ "CRYPTBADCONFIG", "Could not retrieve data from encrypted file !AD due to bad encryption configuration. !AD", 4, 0 },
	{ "DBCOLLREQ", "JOURNAL EXTRACT proceeding without collation information for globals in database. !AD !AD", 4, 0 },
	{ "SETEXTRENV", "Database files are missing or Instance is frozen; supply the database files, wait for the freeze to lift or define ydb_extract_nocol/gtm_extract_nocol to extract possibly incorrect collation", 0, 0 },
	{ "NOTALLDBRNDWN", "Not all regions were successfully rundown", 0, 0 },
	{ "TPRESTNESTERR", "TP restart signaled while handing error - treated as nested error - Use TROLLBACK in error handler to avoid this", 0, 0 },
	{ "JNLFILRDOPN", "Error opening journal file !AD for read for database file !AD", 4, 0 },
	{ "UNUSEDMSG1514", "SEQNUMSEARCHTIMEOUT nixed in r1.24 because it was unused starting GT.M V6.3-000", 0, 0 },
	{ "FTOKKEY", "FTOK key 0x!XL", 1, 0 },
	{ "SEMID", "Semaphore id !UL", 1, 0 },
	{ "JNLQIOSALVAGE", "Journal IO lock for database file !AD salvaged from dead process !UL", 3, 0 },
	{ "FAKENOSPCLEARED", "DEBUG: All fake ENOSPC flags were cleared !UL heartbeats ago", 1, 0 },
	{ "MMFILETOOLARGE", "Size of !AD region (!AD) is larger than maximum size supported for memory mapped I/O on this platform", 4, 0 },
	{ "BADZPEEKARG", "Missing, invalid or surplus !AD parameter for $ZPEEK()", 2, 0 },
	{ "BADZPEEKRANGE", "Access exception raised in memory range given to $ZPEEK()", 0, 0 },
	{ "BADZPEEKFMT", "$ZPEEK() value length inappropriate for selected format", 0, 0 },
	{ "DBMBMINCFREFIXED", "Master bitmap incorrectly marks local bitmap 0x!XL as free. Auto-corrected", 1, 0 },
	{ "NULLENTRYREF", "JOB command did not specify entryref", 0, 0 },
	{ "ZPEEKNORPLINFO", "$ZPEEK() unable to access requested replication structure", 0, 0 },
	{ "MMREGNOACCESS", "Region !AD (!AD) is no longer accessible. See prior error messages in the operator and application error logs", 4, 0 },
<<<<<<< HEAD:sr_port/merrors_ctl.c
	{ "MALLOCMAXUNIX", "Exceeded maximum allocation defined by $ydb_max_storalloc/gtm_max_storalloc", 0, 0 },
	{ "UNUSEDMSG1528", "MALLOCMAXVMS nixed in r1.20 because it is a VMS only error", 0, 0 },
=======
	{ "UNUSEDMSG1525", "MALLOCMAXUNIX last used in OpenVMS", 0, 0 },
	{ "MALLOCCRIT", "Memory allocation critical due to request for !UJ bytes from 0x!XJ", 2, 0 },
>>>>>>> eb3ea98c (GT.M V7.0-002):sr_i386/merrors_ctl.c
	{ "HOSTCONFLICT", "Host !AD could not open database file !AD because it is marked as already open on node !AD", 6, 0 },
	{ "GETADDRINFO", "Error in getting address info", 0, 0 },
	{ "GETNAMEINFO", "Error in getting name info", 0, 0 },
	{ "SOCKBIND", "Error in binding socket", 0, 0 },
	{ "INSTFRZDEFER", "Instance Freeze initiated by !AD error on region !AD deferred due to critical resource conflict", 4, 0 },
<<<<<<< HEAD:sr_port/merrors_ctl.c
	{ "UNUSEDMSG1534", "REGOPENRETRY last used in V6.3-000A", 0, 0 },
=======
	{ "VIEWARGTOOLONG", "The argument length (!UL) to VIEW command !AD exceeds the maximum !UL", 4, 0 },
>>>>>>> eb3ea98c (GT.M V7.0-002):sr_i386/merrors_ctl.c
	{ "REGOPENFAIL", "Failed to open region !AD (!AD) due to conflicting database shutdown activity", 4, 0 },
	{ "REPLINSTNOSHM", "Database !AD has no active connection to a replication journal pool", 2, 0 },
	{ "DEVPARMTOOSMALL", "Deviceparameter must be greater than zero (0)", 0, 0 },
	{ "REMOTEDBNOSPGBL", "Database region !AD contains portion of a spanning global and so cannot point to a remote file", 2, 0 },
	{ "NCTCOLLSPGBL", "Database region !AD contains portion of spanning global ^!AD and so cannot support non-zero numeric collation type", 4, 0 },
	{ "ACTCOLLMISMTCH", "Global ^!AD inherits alternative collation sequence #!UL from global directory but database file !AD contains different collation sequence #!UL for this global", 6, 0 },
	{ "GBLNOMAPTOREG", "Global !AD does not map to region !AD in current global directory", 4, 0 },
	{ "ISSPANGBL", "Operation cannot be performed on global ^!AD as it spans multiple regions in current global directory", 2, 0 },
	{ "TPNOSUPPORT", "Operation cannot be performed while inside of a TP transaction", 0, 0 },
	{ "EXITSTATUS", "Unexpected process exit (!AD), exit status !UL -- called from module !AD at line !UL", 6, 0 },
	{ "ZATRANSERR", "The input string is too long to convert", 0, 0 },
	{ "FILTERTIMEDOUT", "Replication server timed out attempting to read seqno !16@XQ from external filter", 1, 0 },
	{ "TLSDLLNOOPEN", "Failed to load YottaDB TLS/SSL library for secure communication", 0, 0 },
	{ "TLSINIT", "Failed to initialize YottaDB TLS/SSL library for secure communication", 0, 0 },
	{ "TLSCONVSOCK", "Failed to convert Unix TCP/IP socket to TLS/SSL aware socket", 0, 0 },
	{ "TLSHANDSHAKE", "Connection to remote side using TLS/SSL protocol failed", 0, 0 },
	{ "TLSCONNINFO", "Failed to obtain information on the TLS/SSL connection", 0, 0 },
	{ "TLSIOERROR", "Error during TLS/SSL !AD operation", 2, 0 },
	{ "TLSRENEGOTIATE", "Failed to renegotiate TLS/SSL connection", 0, 0 },
	{ "REPLNOTLS", "!AD requested TLS/SSL communication but the !AD was either not started with TLSID qualifier or does not support TLS/SSL protocol", 4, 0 },
	{ "COLTRANSSTR2LONG", "Output string after collation transformation is too long", 0, 0 },
	{ "SOCKPASS", "Socket pass failed", 0, 0 },
	{ "SOCKACCEPT", "Socket accept failed", 0, 0 },
	{ "NOSOCKHANDLE", "No socket handle specified in WRITE /PASS", 0, 0 },
	{ "TRIGLOADFAIL", "MUPIP TRIGGER or $ZTRIGGER operation failed. Failure code: !AD.", 2, 0 },
	{ "SOCKPASSDATAMIX", "Attempt to use a LOCAL socket for both READ/WRITE and PASS/ACCEPT", 0, 0 },
	{ "NOGTCMDB", "!AD does not support operation on GT.CM database region: !AD", 4, 2 },
	{ "NOUSERDB", "!AD does not support operation on non-GDS format region: !AD", 4, 0 },
	{ "DSENOTOPEN", "DSE could not open region !AD - see DSE startup error message for cause", 2, 0 },
	{ "ZSOCKETATTR", "Attribute \"!AD\" invalid for $ZSOCKET function", 2, 0 },
	{ "ZSOCKETNOTSOCK", "$ZSOCKET function called but device is not a socket", 0, 0 },
	{ "CHSETALREADY", "Socket device already contains sockets with iCHSET=!AD, oCHSET=!AD", 4, 0 },
	{ "DSEMAXBLKSAV", "DSE cannot SAVE another block as it already has the maximum of !UL", 1, 0 },
	{ "BLKINVALID", "0x!16@XQ is not a valid block as database file !AD has 0x!16@XQ total blocks", 4, 0 },
	{ "CANTBITMAP", "Can't perform this operation on a bit map (block at a 200 hexadecimal boundary)", 0, 0 },
	{ "AIMGBLKFAIL", "After image build for block 0x!16@XQ in region !AD failed in DSE or MUPIP", 3, 0 },
	{ "YDBDISTUNVERIF", "Environment variable $ydb_dist (!AD) could not be verified against the executables path (!AD)", 4, 0 },
	{ "CRYPTNOAPPEND", "APPEND disallowed on the encrypted file !AD", 2, 0 },
	{ "CRYPTNOSEEK", "SEEK disallowed on the encrypted file !AD", 2, 0 },
	{ "CRYPTNOTRUNC", "Not positioned at file start or EOF. TRUNCATE disallowed on the encrypted file !AD", 2, 0 },
	{ "CRYPTNOKEYSPEC", "Key name needs to be specified with KEY, IKEY, or OKEY device parameter for encrypted I/O", 0, 0 },
	{ "CRYPTNOOVERRIDE", "Cannot override IVEC and/or key without compromising integrity", 0, 0 },
	{ "CRYPTKEYTOOBIG", "Specified key has length !UL, which is greater than the maximum allowed key length !UL", 2, 0 },
	{ "CRYPTBADWRTPOS", "Encrypted WRITE disallowed from a position different than where the last WRITE completed", 0, 0 },
	{ "LABELNOTFND", "GOTO referenced a label that does not exist", 0, 0 },
	{ "RELINKCTLERR", "Error with relink control structure for $ZROUTINES directory !AD", 2, 0 },
	{ "INVLINKTMPDIR", "Value for $ydb_linktmpdir/$gtm_linktmpdir is either not found or not a directory(!AD) - Reverting to default value", 2, 0 },
	{ "NOEDITOR", "Can't find an executable editor: !AD", 2, 0 },
	{ "UPDPROC", "Update Process error", 0, 0 },
	{ "HLPPROC", "Helper Process error", 0, 0 },
	{ "REPLNOHASHTREC", "Sequence number 0x!16@XQ contains trigger definition updates. !AD side must be at least V6.2-000 for replication to continue", 3, 0 },
	{ "REMOTEDBNOTRIG", "Trigger operations are not supported on global ^!AD as it maps to database region !AD that points to a remote file", 4, 0 },
	{ "NEEDTRIGUPGRD", "Cannot do trigger operation on database file !AD until it is upgraded; Run MUPIP TRIGGER -UPGRADE first", 2, 0 },
	{ "REQRLNKCTLRNDWN", "Error accessing relinkctl file !AZ for $ZROUTINES directory !AD. Must be rundown", 3, 0 },
	{ "RLNKCTLRNDWNSUC", "Relinkctl file for $ZROUTINES directory !AD successfully rundown", 2, 0 },
	{ "RLNKCTLRNDWNFL", "Relinkctl file for $ZROUTINES directory !AD failed to rundown as it is open by !UL process(es)", 3, 0 },
	{ "MPROFRUNDOWN", "Error during M-profiling rundown", 0, 0 },
	{ "ZPEEKNOJNLINFO", "$ZPEEK() unable to access requested journal structure - region !AD is not currently journaled", 2, 0 },
	{ "TLSPARAM", "TLS parameter !AD !AD", 4, 0 },
	{ "RLNKRECLATCH", "Failed to get latch on relinkctl record for routine name !AZ in $ZROUTINES directory !AD", 3, 0 },
	{ "RLNKSHMLATCH", "Failed to get latch on relinkctl shared memory for $ZROUTINES directory !AD", 2, 0 },
	{ "JOBLVN2LONG", "The zwrite representation of a local variable transferred to a JOB'd process is too long. The zwrite representation cannot exceed !UL. Encountered size: !UL", 2, 0 },
	{ "NLRESTORE", "DB file header field !AD: !UL does not match the value used in original mapping - restoring to: !UL", 4, 0 },
	{ "PREALLOCATEFAIL", "Disk space reservation for !AD segment has failed", 2, 0 },
	{ "NODFRALLOCSUPP", "The NODEFER_ALLOCATE qualifier is not allowed on this operating system. Not changing the defer allocation flag", 0, 0 },
	{ "LASTWRITERBYPAS", "The last writer for database file !AD bypassed the rundown", 2, 0 },
	{ "TRIGUPBADLABEL", "Trigger upgrade cannot upgrade label !UL to !UL on ^!AD in region !AD", 6, 0 },
	{ "WEIRDSYSTIME", "Time reported by the system clock is outside the acceptable range.  Please check and correct the system clock", 0, 0 },
	{ "REPLSRCEXITERR", "Source server for secondary instance !AZ exited abnormally. See log file !AZ for details.", 2, 0 },
	{ "INVZBREAK", "Cannot set ZBREAK in direct mode routine (GTM$DMOD)", 0, 0 },
	{ "INVTMPDIR", "Value or default for $ydb_tmp/$gtm_tmp is either not found or not a directory (!AD) - Reverting to default value", 2, 0 },
	{ "ARCTLMAXHIGH", "The environment variable !AD = !UL is too high. Assuming the maximum acceptable value of !UL", 4, 0 },
	{ "ARCTLMAXLOW", "The environment variable !AD = !UL is too low. Assuming the minimum acceptable value of !UL", 4, 0 },
	{ "NONTPRESTART", "Database !AD; code: !AD; blk: 0x!16@XQ in glbl: ^!AD; blklvl: !UL, type: !UL, zpos: !AD", 11, 0 },
	{ "PBNPARMREQ", "A first parameter value !AD requires a second parameter specified containing !AD", 4, 0 },
	{ "PBNNOPARM", "First parameter !AD does not support a second parameter", 2, 0 },
	{ "PBNUNSUPSTRUCT", "$ZPEEK() does not support structure !AD", 2, 0 },
	{ "PBNINVALID", "!AD does not have a field named !AD", 4, 0 },
	{ "PBNNOFIELD", "%ZPEEKBYNAME() requires a field.item as its first parameter", 0, 0 },
	{ "JNLDBSEQNOMATCH", "Journal file !AD has beginning region sequence number [0x!16@XQ], but database !AD has region sequence number [0x!16@XQ]", 6, 0 },
	{ "MULTIPROCLATCH", "Failed to get multi-process latch at !AD", 2, 0 },
	{ "INVLOCALE", "Attempt to reset locale to supplied value of $ydb_locale/$gtm_locale (!AD) failed", 2, 0 },
	{ "NOMORESEMCNT", "!AD counter semaphore has reached its maximum and stopped counting for !AZ !AD. Run MUPIP JOURNAL -ROLLBACK -BACKWARD, MUPIP JOURNAL -RECOVER -BACKWARD or MUPIP RUNDOWN to restore the database files and shared resources to a clean state", 5, 0 },
	{ "SETQUALPROB", "Error getting !AD qualifier value", 2, 0 },
	{ "EXTRINTEGRITY", "Database !AD potentially contains spanning nodes or data encrypted with two different keys", 2, 0 },
	{ "CRYPTKEYRELEASEFAILED", "Could not safely release encryption key corresponding to file !AD. !AD", 4, 0 },
	{ "MUREENCRYPTSTART", "Database !AD : MUPIP REORG ENCRYPT started by pid !UL at transaction number [0x!16@XQ]", 4, 0 },
	{ "MUREENCRYPTV4NOALLOW", "Database (re)encryption supported only on fully upgraded V5 databases. !AD has V4 format blocks", 2, 0 },
	{ "ENCRYPTCONFLT", "MUPIP REORG -ENCRYPT and MUPIP EXTRACT -FORMAT=BIN cannot run concurrently - skipping !AD on region: !AD, file: !AD", 6, 0 },
	{ "JNLPOOLRECOVERY", "The size of the data written to the journal pool (!UL) does not match the size of the data in the journal files (!UL) at journal sequence number [0x!16@XQ] for the replication instance file !AZ. The journal pool has been recovered.", 4, 0 },
	{ "LOCKTIMINGINTP", "A LOCK at !AD within a TP transaction is waiting in a final TP retry, which may lead to a general response gap", 2, 0 },
	{ "PBNUNSUPTYPE", "$ZPEEK() does not support type !AD", 2, 0 },
	{ "DBFHEADLRU", "Database file !AD LRU pointer: 0x!16@XQ is outside of range: 0x!16@XQ to 0x!16@XQ or misaligned", 5, 0 },
	{ "ASYNCIONOV4", "!AD database has !AD; cannot !AD", 6, 0 },
	{ "AIOCANCELTIMEOUT", "Pid [0x!XL] timed out waiting for pending async io to complete/cancel in database file !AD", 3, 0 },
	{ "DBGLDMISMATCH", "Database file !AD has !AZ whereas !AZ !AD in global directory !AD has !AZ", 9, 0 },
	{ "DBBLKSIZEALIGN", "Database file !AD has AIO=ON and block_size=!UL which is not a multiple of filesystem block size !UL", 4, 0 },
	{ "ASYNCIONOMM", "Database file !AD!AD cannot !AD", 6, 0 },
	{ "RESYNCSEQLOW", "MUPIP JOURNAL -ROLLBACK -FORWARD -RESYNC=!@ZQ [0x!16@XQ] requested is lower than !@ZQ [0x!16@XQ] which is the starting sequence number of the instance", 4, 0 },
	{ "DBNULCOL", "!AD NULL collation representation differs from the database file header setting", 2, 4 },
	{ "UTF16ENDIAN", "The device previously set UTF-16 endianness to !AD and cannot change to !AD", 4, 0 },
	{ "OFRZACTIVE", "Region !AD has an Online Freeze", 2, 0 },
	{ "OFRZAUTOREL", "Online Freeze automatically released for database file !AD", 2, 0 },
	{ "OFRZCRITREL", "Proceeding with a write to region !AD after Online Freeze while holding crit", 2, 0 },
	{ "OFRZCRITSTUCK", "Unable to proceed with a write to region !AD with Online Freeze while holding crit. Region stuck until freeze is removed.", 2, 0 },
	{ "OFRZNOTHELD", "Online Freeze had been automatically released for at least one region", 0, 0 },
	{ "AIOBUFSTUCK", "Waited !UL minutes for PID: !UL to finish AIO disk write of block: !@UQ [0x!16@XQ] aio_error=!UL", 5, 0 },
	{ "DBDUPNULCOL", "Discarding !AD=!AD key due to duplicate null collation record", 4, 0 },
	{ "CHANGELOGINTERVAL", "!AD Server is now logging to !AD every !UL transactions", 5, 0 },
	{ "DBNONUMSUBS", "!AD Key contains a numeric form of subscript in a global defined to collate all subscripts as strings", 2, 2 },
	{ "AUTODBCREFAIL", "Automatic creation of database file !AD associated with region !AD failed; see associated messages for details", 4, 0 },
	{ "RNDWNSTATSDBFAIL", "Rundown of statistics database region !AD (DB !AD) failed at/in !AD with following error: !AD", 8, 0 },
	{ "STATSDBNOTSUPP", "Attempted operation is not supported on statistics database file !AD", 2, 0 },
	{ "TPNOSTATSHARE", "VIEW \"[NO]STATSHARE\" is not allowed inside a TP transaction", 0, 0 },
	{ "FNTRANSERROR", "Filename including path exceeded 255 chars while trying to resolve filename !AD", 2, 0 },
	{ "NOCRENETFILE", "Database file !AD not created; cannot create across network", 2, 0 },
	{ "DSKSPCCHK", "Error while checking for available disk space to create file !AD", 2, 0 },
	{ "NOCREMMBIJ", "MM access method not compatible with BEFORE image journaling; Database file !AD not created", 2, 0 },
	{ "FILECREERR", "Error !AD for file !AD during DB creation", 4, 0 },
	{ "RAWDEVUNSUP", "RAW device for region !AD is not supported", 2, 0 },
	{ "DBFILECREATED", "Database file !AD created", 2, 0 },
	{ "PCTYRESERVED", "Attempted operation not supported on ^%Y* namespace", 0, 0 },
	{ "REGFILENOTFOUND", "Database file !AD corresponding to region !AD cannot be found", 4, 1 },
	{ "DRVLONGJMP", "Fake internal error that drives longjmp()", 0, 0 },
	{ "INVSTATSDB", "Database file !AD associated with statistics database region !AD is not a valid statistics database", 4, 0 },
	{ "STATSDBERR", "Error in/at !AD attempting to use a statistics database: !AD", 4, 0 },
	{ "STATSDBINUSE", "Statistics database !AD is in use with database !AD so cannot also be used with database !AD", 6, 0 },
	{ "STATSDBFNERR", "This database has no accessible statistics database due to the following error: !AD", 2, 0 },
	{ "JNLSWITCHRETRY", "Retrying previously abandoned switch of journal file !AD for database !AD", 4, 0 },
	{ "JNLSWITCHFAIL", "Failed to switch journal file !AD for database file !AD", 4, 0 },
	{ "CLISTRTOOLONG", "!AZ specified is !UL bytes long which is greater than the allowed maximum of !UL bytes", 3, 0 },
	{ "LVMONBADVAL", "Value for local variable !AD changed inappropriately between two points for indexes !UL and !UL - expected value: !AD  actual value: !AD - Generating core", 8, 0 },
	{ "RESTRICTEDOP", "Attempt to perform a restricted operation: !AZ", 1, 0 },
	{ "RESTRICTSYNTAX", "Syntax error in file !AD at line number !UL. All facilities restricted for process.", 3, 0 },
	{ "MUCREFILERR", "Error in/at !AD creating database !AD (region !AD)", 6, 0 },
	{ "JNLBUFFPHS2SALVAGE", "Salvaged journal records from process !UL for database file !AD at transaction number [0x!16@XQ] and journal-sequence-number/unique-token [0x!16@XQ] with journal file starting offset [0x!XL] and length [0x!XL]", 7, 0 },
	{ "JNLPOOLPHS2SALVAGE", "Salvaged journal records from process !UL for replication instance file !AD at journal sequence number [0x!16@XQ] with journal pool starting offset [0x!16@XQ] and length [0x!XL]", 6, 0 },
	{ "MURNDWNARGLESS", "Argumentless MUPIP RUNDOWN started with process id !UL by userid !UL from directory !AD", 4, 0 },
	{ "DBFREEZEON", "Database file !AD is FROZEN (!AZOVERRIDE !AZONLINE !AZAUTOREL)", 5, 0 },
	{ "DBFREEZEOFF", "Database file !AD is UNFROZEN (!AZOVERRIDE !AZAUTOREL)", 4, 0 },
	{ "STPCRIT", "String pool space critical", 0, 0 },
	{ "STPOFLOW", "String pool space overflow", 0, 0 },
	{ "SYSUTILCONF", "Error determining the path for system utility. !AD", 2, 0 },
	{ "MSTACKSZNA", "User-specified M stack size of !UL KiB not appropriate; must be between !UL KiB and !UL KiB; reverting to !UL KiB", 4, 0 },
	{ "JNLEXTRCTSEQNO", "Journal Extracts based on sequence numbers are restricted to a single region when replication is OFF", 0, 0 },
	{ "INVSEQNOQUAL", "Invalid SEQNO qualifier value !AD", 2, 0 },
	{ "LOWSPC", "WARNING: Database !AD has !UL% or less of the total block space remaining. Blocks Used: !@UQ Total Blocks Available: !@UQ", 5, 0 },
	{ "FAILEDRECCOUNT", "LOAD unable to process !@UQ records", 1, 0 },
	{ "LOADRECCNT", "Last EXTRACT record processed by LOAD: !@UQ", 1, 0 },
	{ "COMMFILTERERR", "Error executing the command filter for !AD. !AD", 4, 0 },
	{ "NOFILTERNEST", "Filter nesting not allowed", 0, 0 },
	{ "MLKHASHTABERR", "A LOCK control structure is damaged and could not be corrected. Lock entry for !AD is invalid.", 2, 0 },
	{ "LOCKCRITOWNER", "LOCK crit is held by: !UL", 1, 0 },
	{ "MLKHASHWRONG", "A LOCK control structure has an invalid value; LOCK table failed integrity check. !AD", 2, 0 },
	{ "XCRETNULLREF", "Returned null reference from external call !AD", 2, 0 },
	{ "EXTCALLBOUNDS", "Wrote outside bounds of external call buffer. M label: !AZ", 1, 0 },
	{ "EXCEEDSPREALLOC", "Preallocated size !UL for M external call label !AZ exceeded by string of length !UL", 3, 0 },
	{ "ZTIMEOUT", "Time expired", 0, 0 },
	{ "ERRWZTIMEOUT", "Error while processing $ZTIMEOUT", 0, 0 },
	{ "MLKHASHRESIZE", "LOCK hash table increased in size from !UL to !UL and placed in shared memory (id = !UL)", 3, 0 },
	{ "MLKHASHRESIZEFAIL", "Failed to increase LOCK hash table size from !UL to !UL. Will retry with larger size.", 2, 0 },
	{ "MLKCLEANED", "LOCK garbage collection freed !UL lock slots for region !AD", 3, 0 },
	{ "NOTMNAME", "!AD is not a valid M name", 2, 0 },
	{ "DEVNAMERESERVED", "Cannot use !AD as device name. Reserved for GTM internal usage", 2, 0 },
	{ "ORLBKREL", "ONLINE ROLLBACK releasing all locking resources to allow a freeze OFF to proceed", 0, 0 },
	{ "ORLBKRESTART", "ONLINE ROLLBACK restarted on instance !AD corresponding to !AD", 4, 0 },
	{ "UNIQNAME", "Cannot provide same file name (!AD) for !AD and !AD", 6, 0 },
	{ "APDINITFAIL", "Audit Principal Device failed to initialize audit information", 0, 0 },
	{ "APDCONNFAIL", "Audit Principal Device failed to connect to audit logger", 0, 0 },
	{ "APDLOGFAIL", "Audit Principal Device failed to log activity", 0, 0 },
	{ "STATSDBMEMERR", "Process attempted to create stats block in statistics database !AD and received SIGBUS--invalid physical address. Check file system space.", 2, 0 },
	{ "BUFSPCDELAY", "Request for !UL blocks in region !AD delayed", 3, 0 },
	{ "AIOQUEUESTUCK", "Waited !UL minutes for AIO work queue to complete (cr = !XL)", 2, 0 },
	{ "INVGVPATQUAL", "Invalid Global Value Pattern file qualifier value.  !AD", 2, 0 },
	{ "NULLPATTERN", "Empty line found in the Pattern file.", 0, 0 },
	{ "MLKREHASH", "LOCK hash table rebuilt for region !AD (seed = !UJ)", 3, 0 },
	{ "MUKEEPPERCENT", "Keep threshold percentage should be from 0 to 99", 0, 0 },
	{ "MUKEEPNODEC", "Expected decimal integer input for keep", 0, 0 },
	{ "MUKEEPNOTRUNC", "Keep issued without -truncate", 0, 0 },
	{ "MUTRUNCNOSPKEEP", "Region !AD has insufficient space to meet truncate target percentage of !UL with keep at !@UQ blocks", 4, 0 },
	{ "TERMHANGUP", "Terminal has disconnected", 0, 0 },
	{ "DBFILNOFULLWRT", "Disabling fullblock writes. !AD !AD !UL", 5, 0 },
	{ "BADCONNECTPARAM", "Error parsing or invalid !AD", 2, 0 },
	{ "BADPARAMCOUNT", "-CONNECTPARAMS accepts one to six parameter values", 0, 0 },
	{ "REPLALERT", "Source Server could not connect to replicating instance [!AZ] for !UL seconds", 2, 0 },
	{ "SHUT2QUICK", "Shutdown timeout [!UL] shorter than the heartbeat period [!UL]; cannot confirm the backlog at the replicating instance [!AD]", 4, 0 },
	{ "REPLNORESP", "No sequence number confirmation from the replicating instance [!AD] after waiting for [!UL] second(s)", 3, 0 },
	{ "REPL0BACKLOG", "Total backlog for the specified replicating instance(s) is 0", 0, 0 },
	{ "REPLBACKLOG", "Timeout occurred while there was a backlog", 0, 0 },
	{ "INVSHUTDOWN", "Shutdown timeout should be from 0 to 3600 seconds", 0, 0 },
	{ "SOCKBLOCKERR", "WRITE /BLOCK error: !AD", 2, 0 },
	{ "SOCKWAITARG", "!AD argument to WRITE /WAIT !AD", 4, 0 },
	{ "LASTTRANS", "Last transaction sequence number !AD : !@UQ", 3, 0 },
	{ "SRCBACKLOGSTATUS", "Instance !AD !AD", 4, 0 },
	{ "BKUPRETRY", "Retrying MUPIP BACKUP for region: !AD (database file: !AD). Attempt: #!UL of !UL", 6, 0 },
	{ "BKUPPROGRESS", "Transfer : !AD ; Speed : !AD MiB/sec ; Transactions : !@UQ ; Estimated time left : !UL !AD", 8, 0 },
	{ "BKUPFILEPERM", "Backup file !AD does not have write permission", 2, 0 },
};

<<<<<<< HEAD:sr_port/merrors_ctl.c
=======
LITDEF	int ERR_ACK = 150372361;
LITDEF	int ERR_BREAKZST = 150372371;
LITDEF	int ERR_BADACCMTHD = 150372379;
LITDEF	int ERR_BADJPIPARAM = 150372386;
LITDEF	int ERR_BADSYIPARAM = 150372394;
LITDEF	int ERR_BITMAPSBAD = 150372402;
LITDEF	int ERR_BREAK = 150372411;
LITDEF	int ERR_BREAKDEA = 150372419;
LITDEF	int ERR_BREAKZBA = 150372427;
LITDEF	int ERR_STATCNT = 150372435;
LITDEF	int ERR_BTFAIL = 150372442;
LITDEF	int ERR_MUPRECFLLCK = 150372450;
LITDEF	int ERR_CMD = 150372458;
LITDEF	int ERR_COLON = 150372466;
LITDEF	int ERR_COMMA = 150372474;
LITDEF	int ERR_COMMAORRPAREXP = 150372482;
LITDEF	int ERR_COMMENT = 150372491;
LITDEF	int ERR_CTRAP = 150372498;
LITDEF	int ERR_CTRLC = 150372507;
LITDEF	int ERR_CTRLY = 150372515;
LITDEF	int ERR_DBCCERR = 150372522;
LITDEF	int ERR_DUPTOKEN = 150372530;
LITDEF	int ERR_DBJNLNOTMATCH = 150372538;
LITDEF	int ERR_DBFILERR = 150372546;
LITDEF	int ERR_DBNOTGDS = 150372554;
LITDEF	int ERR_DBOPNERR = 418808018;
LITDEF	int ERR_DBRDERR = 418808026;
LITDEF	int ERR_CCEDUMPNOW = 150372580;
LITDEF	int ERR_DEVPARINAP = 150372586;
LITDEF	int ERR_RECORDSTAT = 150372595;
LITDEF	int ERR_NOTGBL = 150372602;
LITDEF	int ERR_DEVPARPROT = 150372610;
LITDEF	int ERR_PREMATEOF = 150372618;
LITDEF	int ERR_GVINVALID = 150372626;
LITDEF	int ERR_DEVPARTOOBIG = 150372634;
LITDEF	int ERR_DEVPARUNK = 150372642;
LITDEF	int ERR_DEVPARVALREQ = 150372650;
LITDEF	int ERR_DEVPARMNEG = 150372658;
LITDEF	int ERR_DSEBLKRDFAIL = 150372666;
LITDEF	int ERR_DSEFAIL = 150372674;
LITDEF	int ERR_NOTALLREPLON = 150372680;
LITDEF	int ERR_BADLKIPARAM = 150372690;
LITDEF	int ERR_JNLREADBOF = 150372698;
LITDEF	int ERR_DVIKEYBAD = 150372706;
LITDEF	int ERR_ENQ = 150372713;
LITDEF	int ERR_EQUAL = 150372722;
LITDEF	int ERR_ERRORSUMMARY = 150372730;
LITDEF	int ERR_ERRWEXC = 150372738;
LITDEF	int ERR_ERRWIOEXC = 150372746;
LITDEF	int ERR_ERRWZBRK = 150372754;
LITDEF	int ERR_ERRWZTRAP = 150372762;
LITDEF	int ERR_NUMUNXEOR = 150372770;
LITDEF	int ERR_EXPR = 150372778;
LITDEF	int ERR_STRUNXEOR = 150372786;
LITDEF	int ERR_JNLEXTEND = 150372794;
LITDEF	int ERR_FCHARMAXARGS = 150372802;
LITDEF	int ERR_FCNSVNEXPECTED = 150372810;
LITDEF	int ERR_FNARGINC = 150372818;
LITDEF	int ERR_JNLACCESS = 418808282;
LITDEF	int ERR_TRANSNOSTART = 150372834;
LITDEF	int ERR_FNUMARG = 150372842;
LITDEF	int ERR_FOROFLOW = 150372850;
LITDEF	int ERR_YDIRTSZ = 150372858;
LITDEF	int ERR_JNLSUCCESS = 150372865;
LITDEF	int ERR_GBLNAME = 150372874;
LITDEF	int ERR_GBLOFLOW = 150372882;
LITDEF	int ERR_CORRUPT = 150372890;
LITDEF	int ERR_GTMCHECK = 150372900;
LITDEF	int ERR_GVDATAFAIL = 150372906;
LITDEF	int ERR_EORNOTFND = 150372914;
LITDEF	int ERR_GVGETFAIL = 150372922;
LITDEF	int ERR_GVIS = 150372931;
LITDEF	int ERR_GVKILLFAIL = 150372938;
LITDEF	int ERR_GVNAKED = 150372946;
LITDEF	int ERR_BACKUPDBFILE = 150372955;
LITDEF	int ERR_GVORDERFAIL = 150372962;
LITDEF	int ERR_GVPUTFAIL = 150372970;
LITDEF	int ERR_PATTABSYNTAX = 150372978;
LITDEF	int ERR_GVSUBOFLOW = 150372986;
LITDEF	int ERR_GVUNDEF = 150372994;
LITDEF	int ERR_TRANSNEST = 150373002;
LITDEF	int ERR_INDEXTRACHARS = 150373010;
LITDEF	int ERR_CORRUPTNODE = 150373018;
LITDEF	int ERR_INDRMAXLEN = 150373026;
LITDEF	int ERR_INSFFBCNT = 150373034;
LITDEF	int ERR_INTEGERRS = 150373042;
LITDEF	int ERR_INVCMD = 150373048;
LITDEF	int ERR_INVFCN = 150373058;
LITDEF	int ERR_INVOBJ = 150373066;
LITDEF	int ERR_INVSVN = 150373074;
LITDEF	int ERR_IOEOF = 150373082;
LITDEF	int ERR_IONOTOPEN = 150373090;
LITDEF	int ERR_MUPIPINFO = 150373099;
LITDEF	int ERR_IVTIME = 150373106;
LITDEF	int ERR_JOBFAIL = 150373114;
LITDEF	int ERR_JOBLABOFF = 150373122;
LITDEF	int ERR_JOBPARNOVAL = 150373130;
LITDEF	int ERR_JOBPARNUM = 150373138;
LITDEF	int ERR_JOBPARSTR = 150373146;
LITDEF	int ERR_JOBPARUNK = 150373154;
LITDEF	int ERR_JOBPARVALREQ = 150373162;
LITDEF	int ERR_JUSTFRACT = 150373170;
LITDEF	int ERR_KEY2BIG = 150373178;
LITDEF	int ERR_LABELEXPECTED = 150373186;
LITDEF	int ERR_LABELMISSING = 150373194;
LITDEF	int ERR_LABELUNKNOWN = 150373202;
LITDEF	int ERR_DIVZERO = 150373210;
LITDEF	int ERR_LKNAMEXPECTED = 150373218;
LITDEF	int ERR_JNLRDERR = 418808682;
LITDEF	int ERR_LOADRUNNING = 150373234;
LITDEF	int ERR_LPARENMISSING = 150373242;
LITDEF	int ERR_LSEXPECTED = 150373250;
LITDEF	int ERR_LVORDERARG = 150373258;
LITDEF	int ERR_MAXFORARGS = 150373266;
LITDEF	int ERR_TRANSMINUS = 150373274;
LITDEF	int ERR_MAXNRSUBSCRIPTS = 150373282;
LITDEF	int ERR_MAXSTRLEN = 150373290;
LITDEF	int ERR_ENCRYPTCONFLT2 = 150373296;
LITDEF	int ERR_JNLFILOPN = 150373306;
LITDEF	int ERR_MBXRDONLY = 418808770;
LITDEF	int ERR_JNLINVALID = 150373322;
LITDEF	int ERR_MBXWRTONLY = 418808786;
LITDEF	int ERR_MEMORY = 150373340;
LITDEF	int ERR_DONOBLOCK = 150373344;
LITDEF	int ERR_ZATRANSCOL = 150373354;
LITDEF	int ERR_VIEWREGLIST = 150373360;
LITDEF	int ERR_NUMERR = 150373370;
LITDEF	int ERR_NUM64ERR = 150373378;
LITDEF	int ERR_UNUM64ERR = 150373386;
LITDEF	int ERR_HEXERR = 150373394;
LITDEF	int ERR_HEX64ERR = 150373402;
LITDEF	int ERR_CMDERR = 150373410;
LITDEF	int ERR_BACKUPSUCCESS = 150373419;
LITDEF	int ERR_JNLTMQUAL3 = 150373426;
LITDEF	int ERR_MULTLAB = 150373434;
LITDEF	int ERR_GTMCURUNSUPP = 150373442;
LITDEF	int ERR_CCEDUMPOFF = 150373452;
LITDEF	int ERR_NOPLACE = 150373458;
LITDEF	int ERR_JNLCLOSE = 150373466;
LITDEF	int ERR_NOTPRINCIO = 150373472;
LITDEF	int ERR_NOTTOEOFONPUT = 150373482;
LITDEF	int ERR_NOZBRK = 150373491;
LITDEF	int ERR_NULSUBSC = 150373498;
LITDEF	int ERR_NUMOFLOW = 150373506;
LITDEF	int ERR_PARFILSPC = 150373514;
LITDEF	int ERR_PATCLASS = 150373522;
LITDEF	int ERR_PATCODE = 150373530;
LITDEF	int ERR_PATLIT = 150373538;
LITDEF	int ERR_PATMAXLEN = 150373546;
LITDEF	int ERR_LPARENREQD = 150373554;
LITDEF	int ERR_PATUPPERLIM = 150373562;
LITDEF	int ERR_PCONDEXPECTED = 150373570;
LITDEF	int ERR_PRCNAMLEN = 150373578;
LITDEF	int ERR_RANDARGNEG = 150373586;
LITDEF	int ERR_DBPRIVERR = 418809050;
LITDEF	int ERR_REC2BIG = 150373602;
LITDEF	int ERR_RHMISSING = 150373610;
LITDEF	int ERR_DEVICEREADONLY = 150373618;
LITDEF	int ERR_COLLDATAEXISTS = 150373626;
LITDEF	int ERR_ROUTINEUNKNOWN = 150373634;
LITDEF	int ERR_RPARENMISSING = 150373642;
LITDEF	int ERR_RTNNAME = 150373650;
LITDEF	int ERR_VIEWGVN = 150373658;
LITDEF	int ERR_RTSLOC = 150373667;
LITDEF	int ERR_RWARG = 150373674;
LITDEF	int ERR_RWFORMAT = 150373682;
LITDEF	int ERR_JNLWRTDEFER = 150373691;
LITDEF	int ERR_SELECTFALSE = 150373698;
LITDEF	int ERR_SPOREOL = 150373706;
LITDEF	int ERR_SRCLIN = 150373715;
LITDEF	int ERR_SRCLOC = 150373723;
LITDEF	int ERR_RLNKRECNFL = 150373728;
LITDEF	int ERR_STACKCRIT = 150373738;
LITDEF	int ERR_STACKOFLOW = 150373748;
LITDEF	int ERR_STACKUNDERFLO = 150373754;
LITDEF	int ERR_STRINGOFLOW = 150373762;
LITDEF	int ERR_SVNOSET = 150373770;
LITDEF	int ERR_VIEWFN = 150373778;
LITDEF	int ERR_TERMASTQUOTA = 150373786;
LITDEF	int ERR_TEXTARG = 150373794;
LITDEF	int ERR_TMPSTOREMAX = 150373802;
LITDEF	int ERR_VIEWCMD = 150373810;
LITDEF	int ERR_JNI = 150373818;
LITDEF	int ERR_TXTSRCFMT = 150373826;
LITDEF	int ERR_UIDMSG = 150373834;
LITDEF	int ERR_UIDSND = 150373842;
LITDEF	int ERR_UNDEF = 150373850;
LITDEF	int ERR_UNIMPLOP = 150373858;
LITDEF	int ERR_VAREXPECTED = 150373866;
LITDEF	int ERR_BACKUPFAIL = 150373874;
LITDEF	int ERR_MAXARGCNT = 150373882;
LITDEF	int ERR_GTMSECSHRSEMGET = 150373892;
LITDEF	int ERR_VIEWARGCNT = 150373898;
LITDEF	int ERR_GTMSECSHRDMNSTARTED = 150373907;
LITDEF	int ERR_ZATTACHERR = 150373914;
LITDEF	int ERR_ZDATEFMT = 150373922;
LITDEF	int ERR_ZEDFILSPEC = 150373930;
LITDEF	int ERR_ZFILENMTOOLONG = 150373938;
LITDEF	int ERR_ZFILKEYBAD = 150373946;
LITDEF	int ERR_ZFILNMBAD = 150373954;
LITDEF	int ERR_ZGOTOLTZERO = 150373962;
LITDEF	int ERR_ZGOTOTOOBIG = 150373970;
LITDEF	int ERR_ZLINKFILE = 150373978;
LITDEF	int ERR_ZPARSETYPE = 150373986;
LITDEF	int ERR_ZPARSFLDBAD = 150373994;
LITDEF	int ERR_ZPIDBADARG = 150374002;
LITDEF	int ERR_ZPRIVARGBAD = 150374010;
LITDEF	int ERR_ZPRIVSYNTAXERR = 150374018;
LITDEF	int ERR_ZPRTLABNOTFND = 150374026;
LITDEF	int ERR_VIEWAMBIG = 150374034;
LITDEF	int ERR_VIEWNOTFOUND = 150374042;
LITDEF	int ERR_ZSETPRVARGBAD = 150374050;
LITDEF	int ERR_INVSPECREC = 150374058;
LITDEF	int ERR_ZSETPRVSYNTAX = 150374066;
LITDEF	int ERR_ZSRCHSTRMCT = 150374074;
LITDEF	int ERR_VERSION = 150374082;
LITDEF	int ERR_MUNOTALLSEC = 150374088;
LITDEF	int ERR_MUSECDEL = 150374099;
LITDEF	int ERR_MUSECNOTDEL = 150374107;
LITDEF	int ERR_RPARENREQD = 150374114;
LITDEF	int ERR_ZGBLDIRACC = 418809578;
LITDEF	int ERR_GVNAKEDEXTNM = 150374130;
LITDEF	int ERR_EXTGBLDEL = 150374138;
LITDEF	int ERR_DSEWCINITCON = 150374147;
LITDEF	int ERR_LASTFILCMPLD = 150374155;
LITDEF	int ERR_NOEXCNOZTRAP = 150374163;
LITDEF	int ERR_UNSDCLASS = 150374170;
LITDEF	int ERR_UNSDDTYPE = 150374178;
LITDEF	int ERR_ZCUNKTYPE = 150374186;
LITDEF	int ERR_ZCUNKMECH = 150374194;
LITDEF	int ERR_ZCUNKQUAL = 150374202;
LITDEF	int ERR_JNLDBTNNOMATCH = 150374210;
LITDEF	int ERR_ZCALLTABLE = 150374218;
LITDEF	int ERR_ZCARGMSMTCH = 150374226;
LITDEF	int ERR_ZCCONMSMTCH = 150374234;
LITDEF	int ERR_ZCOPT0 = 150374242;
LITDEF	int ERR_ZCSTATUS = 150374250;
LITDEF	int ERR_ZCUSRRTN = 150374258;
LITDEF	int ERR_ZCPOSOVR = 150374266;
LITDEF	int ERR_ZCINPUTREQ = 150374274;
LITDEF	int ERR_JNLTNOUTOFSEQ = 150374282;
LITDEF	int ERR_ACTRANGE = 150374290;
LITDEF	int ERR_ZCCONVERT = 150374298;
LITDEF	int ERR_ZCRTENOTF = 150374306;
LITDEF	int ERR_GVRUNDOWN = 150374314;
LITDEF	int ERR_LKRUNDOWN = 150374322;
LITDEF	int ERR_IORUNDOWN = 150374330;
LITDEF	int ERR_FILENOTFND = 150374338;
LITDEF	int ERR_MUFILRNDWNFL = 150374346;
LITDEF	int ERR_JNLTMQUAL1 = 150374354;
LITDEF	int ERR_FORCEDHALT = 150374364;
LITDEF	int ERR_LOADEOF = 150374370;
LITDEF	int ERR_WILLEXPIRE = 150374379;
LITDEF	int ERR_LOADEDBG = 150374386;
LITDEF	int ERR_LABELONLY = 150374394;
LITDEF	int ERR_MUREORGFAIL = 150374402;
LITDEF	int ERR_GVZPREVFAIL = 150374410;
LITDEF	int ERR_MULTFORMPARM = 150374418;
LITDEF	int ERR_QUITARGUSE = 150374426;
LITDEF	int ERR_NAMEEXPECTED = 150374434;
LITDEF	int ERR_FALLINTOFLST = 150374442;
LITDEF	int ERR_NOTEXTRINSIC = 150374450;
LITDEF	int ERR_GTMSECSHRREMSEMFAIL = 150374456;
LITDEF	int ERR_FMLLSTMISSING = 150374466;
LITDEF	int ERR_ACTLSTTOOLONG = 150374474;
LITDEF	int ERR_ACTOFFSET = 150374482;
LITDEF	int ERR_MAXACTARG = 150374490;
LITDEF	int ERR_GTMSECSHRREMSEM = 150374499;
LITDEF	int ERR_JNLTMQUAL2 = 150374506;
LITDEF	int ERR_GDINVALID = 150374514;
LITDEF	int ERR_ASSERT = 150374524;
LITDEF	int ERR_MUFILRNDWNSUC = 150374531;
LITDEF	int ERR_LOADEDSZ = 150374538;
LITDEF	int ERR_QUITARGLST = 150374546;
LITDEF	int ERR_QUITARGREQD = 150374554;
LITDEF	int ERR_CRITRESET = 150374562;
LITDEF	int ERR_UNKNOWNFOREX = 150374572;
LITDEF	int ERR_FSEXP = 150374578;
LITDEF	int ERR_WILDCARD = 150374586;
LITDEF	int ERR_DIRONLY = 150374594;
LITDEF	int ERR_FILEPARSE = 150374602;
LITDEF	int ERR_QUALEXP = 150374610;
LITDEF	int ERR_BADQUAL = 150374618;
LITDEF	int ERR_QUALVAL = 150374626;
LITDEF	int ERR_ZROSYNTAX = 150374634;
LITDEF	int ERR_COMPILEQUALS = 150374642;
LITDEF	int ERR_ZLNOOBJECT = 150374650;
LITDEF	int ERR_ZLMODULE = 150374658;
LITDEF	int ERR_DBBLEVMX = 150374667;
LITDEF	int ERR_DBBLEVMN = 150374675;
LITDEF	int ERR_DBBSIZMN = 150374682;
LITDEF	int ERR_DBBSIZMX = 150374690;
LITDEF	int ERR_DBRSIZMN = 150374698;
LITDEF	int ERR_DBRSIZMX = 150374706;
LITDEF	int ERR_DBCMPNZRO = 150374714;
LITDEF	int ERR_DBSTARSIZ = 150374723;
LITDEF	int ERR_DBSTARCMP = 150374731;
LITDEF	int ERR_DBCMPMX = 150374739;
LITDEF	int ERR_DBKEYMX = 150374746;
LITDEF	int ERR_DBKEYMN = 150374754;
LITDEF	int ERR_DBCMPBAD = 150374760;
LITDEF	int ERR_DBKEYORD = 150374770;
LITDEF	int ERR_DBPTRNOTPOS = 150374778;
LITDEF	int ERR_DBPTRMX = 150374786;
LITDEF	int ERR_DBPTRMAP = 150374795;
LITDEF	int ERR_IFBADPARM = 150374802;
LITDEF	int ERR_IFNOTINIT = 150374810;
LITDEF	int ERR_GTMSECSHRSOCKET = 150374818;
LITDEF	int ERR_LOADBGSZ = 150374826;
LITDEF	int ERR_LOADFMT = 150374834;
LITDEF	int ERR_LOADFILERR = 150374842;
LITDEF	int ERR_NOREGION = 150374850;
LITDEF	int ERR_PATLOAD = 150374858;
LITDEF	int ERR_EXTRACTFILERR = 150374866;
LITDEF	int ERR_FREEZE = 150374875;
LITDEF	int ERR_NOSELECT = 150374880;
LITDEF	int ERR_EXTRFAIL = 150374890;
LITDEF	int ERR_LDBINFMT = 150374898;
LITDEF	int ERR_NOPREVLINK = 150374906;
LITDEF	int ERR_CCEDUMPON = 150374916;
LITDEF	int ERR_CCEDMPQUALREQ = 150374922;
LITDEF	int ERR_CCEDBDUMP = 150374931;
LITDEF	int ERR_CCEDBNODUMP = 150374939;
LITDEF	int ERR_CCPMBX = 150374946;
LITDEF	int ERR_REQRUNDOWN = 150374954;
LITDEF	int ERR_CCPINTQUE = 150374962;
LITDEF	int ERR_CCPBADMSG = 150374970;
LITDEF	int ERR_CNOTONSYS = 150374978;
LITDEF	int ERR_CCPNAME = 150374988;
LITDEF	int ERR_CCPNOTFND = 150374994;
LITDEF	int ERR_OPRCCPSTOP = 150375003;
LITDEF	int ERR_SELECTSYNTAX = 150375012;
LITDEF	int ERR_LOADABORT = 150375018;
LITDEF	int ERR_FNOTONSYS = 150375026;
LITDEF	int ERR_AMBISYIPARAM = 150375034;
LITDEF	int ERR_PREVJNLNOEOF = 150375042;
LITDEF	int ERR_LKSECINIT = 150375050;
LITDEF	int ERR_BACKUPREPL = 150375059;
LITDEF	int ERR_BACKUPSEQNO = 150375067;
LITDEF	int ERR_DIRACCESS = 150375074;
LITDEF	int ERR_TXTSRCMAT = 150375082;
LITDEF	int ERR_CCENOGROUP = 150375088;
LITDEF	int ERR_BADDBVER = 150375098;
LITDEF	int ERR_LINKVERSION = 150375108;
LITDEF	int ERR_TOTALBLKMAX = 150375114;
LITDEF	int ERR_LOADCTRLY = 150375123;
LITDEF	int ERR_CLSTCONFLICT = 150375130;
LITDEF	int ERR_SRCNAM = 150375139;
LITDEF	int ERR_LCKGONE = 150375145;
LITDEF	int ERR_SUB2LONG = 150375154;
LITDEF	int ERR_EXTRACTCTRLY = 150375163;
LITDEF	int ERR_CCENOWORLD = 150375168;
LITDEF	int ERR_GVQUERYFAIL = 150375178;
LITDEF	int ERR_LCKSCANCELLED = 150375186;
LITDEF	int ERR_INVNETFILNM = 150375194;
LITDEF	int ERR_NETDBOPNERR = 150375202;
LITDEF	int ERR_BADSRVRNETMSG = 150375210;
LITDEF	int ERR_BADGTMNETMSG = 150375218;
LITDEF	int ERR_SERVERERR = 150375226;
LITDEF	int ERR_NETFAIL = 150375234;
LITDEF	int ERR_NETLCKFAIL = 150375242;
LITDEF	int ERR_TTINVFILTER = 150375251;
LITDEF	int ERR_BACKUPTN = 150375259;
LITDEF	int ERR_WCSFLUFAIL = 150375266;
LITDEF	int ERR_BADTRNPARAM = 150375274;
LITDEF	int ERR_DSEONLYBGMM = 150375280;
LITDEF	int ERR_DSEINVLCLUSFN = 150375288;
LITDEF	int ERR_RDFLTOOSHORT = 150375298;
LITDEF	int ERR_TIMRBADVAL = 150375307;
LITDEF	int ERR_CCENOSYSLCK = 150375312;
LITDEF	int ERR_CCPGRP = 150375324;
LITDEF	int ERR_UNSOLCNTERR = 150375330;
LITDEF	int ERR_BACKUPCTRL = 150375339;
LITDEF	int ERR_NOCCPPID = 150375346;
LITDEF	int ERR_CCPJNLOPNERR = 150375354;
LITDEF	int ERR_LCKSGONE = 150375361;
LITDEF	int ERR_ZLKIDBADARG = 150375370;
LITDEF	int ERR_DBFILOPERR = 150375378;
LITDEF	int ERR_CCERDERR = 150375386;
LITDEF	int ERR_CCEDBCL = 150375395;
LITDEF	int ERR_CCEDBNTCL = 150375403;
LITDEF	int ERR_CCEWRTERR = 150375410;
LITDEF	int ERR_CCEBADFN = 150375418;
LITDEF	int ERR_CCERDTIMOUT = 150375426;
LITDEF	int ERR_CCPSIGCONT = 150375435;
LITDEF	int ERR_CCEBGONLY = 150375442;
LITDEF	int ERR_CCENOCCP = 150375451;
LITDEF	int ERR_CCECCPPID = 150375459;
LITDEF	int ERR_CCECLSTPRCS = 150375467;
LITDEF	int ERR_ZSHOWBADFUNC = 150375474;
LITDEF	int ERR_NOTALLJNLEN = 150375480;
LITDEF	int ERR_BADLOCKNEST = 150375490;
LITDEF	int ERR_NOLBRSRC = 150375498;
LITDEF	int ERR_INVZSTEP = 150375506;
LITDEF	int ERR_ZSTEPARG = 150375514;
LITDEF	int ERR_INVSTRLEN = 150375522;
LITDEF	int ERR_RECCNT = 150375531;
LITDEF	int ERR_TEXT = 150375539;
LITDEF	int ERR_ZWRSPONE = 150375546;
LITDEF	int ERR_FILEDEL = 150375555;
LITDEF	int ERR_JNLBADLABEL = 150375562;
LITDEF	int ERR_JNLREADEOF = 150375570;
LITDEF	int ERR_JNLRECFMT = 150375578;
LITDEF	int ERR_BLKTOODEEP = 150375584;
LITDEF	int ERR_NESTFORMP = 150375594;
LITDEF	int ERR_BINHDR = 150375603;
LITDEF	int ERR_GOQPREC = 150375611;
LITDEF	int ERR_LDGOQFMT = 150375618;
LITDEF	int ERR_BEGINST = 150375627;
LITDEF	int ERR_INVMVXSZ = 150375636;
LITDEF	int ERR_JNLWRTNOWWRTR = 150375642;
LITDEF	int ERR_GTMSECSHRSHMCONCPROC = 150375648;
LITDEF	int ERR_JNLINVALLOC = 150375656;
LITDEF	int ERR_JNLINVEXT = 150375664;
LITDEF	int ERR_MUPCLIERR = 150375674;
LITDEF	int ERR_JNLTMQUAL4 = 150375682;
LITDEF	int ERR_GTMSECSHRREMSHM = 150375691;
LITDEF	int ERR_GTMSECSHRREMFILE = 150375699;
LITDEF	int ERR_MUNODBNAME = 150375706;
LITDEF	int ERR_FILECREATE = 150375715;
LITDEF	int ERR_FILENOTCREATE = 150375723;
LITDEF	int ERR_JNLPROCSTUCK = 150375728;
LITDEF	int ERR_INVGLOBALQUAL = 150375738;
LITDEF	int ERR_COLLARGLONG = 150375746;
LITDEF	int ERR_NOPINI = 150375754;
LITDEF	int ERR_DBNOCRE = 150375762;
LITDEF	int ERR_JNLSPACELOW = 150375771;
LITDEF	int ERR_DBCOMMITCLNUP = 150375779;
LITDEF	int ERR_BFRQUALREQ = 150375786;
LITDEF	int ERR_REQDVIEWPARM = 150375794;
LITDEF	int ERR_COLLFNMISSING = 150375802;
LITDEF	int ERR_JNLACTINCMPLT = 150375808;
LITDEF	int ERR_NCTCOLLDIFF = 150375818;
LITDEF	int ERR_DLRCUNXEOR = 150375826;
LITDEF	int ERR_DLRCTOOBIG = 150375834;
LITDEF	int ERR_WCERRNOTCHG = 150375842;
LITDEF	int ERR_WCWRNNOTCHG = 150375848;
LITDEF	int ERR_ZCWRONGDESC = 150375858;
LITDEF	int ERR_MUTNWARN = 150375864;
LITDEF	int ERR_GTMSECSHRUPDDBHDR = 150375875;
LITDEF	int ERR_LCKSTIMOUT = 150375880;
LITDEF	int ERR_CTLMNEMAXLEN = 150375890;
LITDEF	int ERR_CTLMNEXPECTED = 150375898;
LITDEF	int ERR_USRIOINIT = 150375906;
LITDEF	int ERR_CRITSEMFAIL = 150375914;
LITDEF	int ERR_TERMWRITE = 150375922;
LITDEF	int ERR_COLLTYPVERSION = 150375930;
LITDEF	int ERR_LVNULLSUBS = 150375938;
LITDEF	int ERR_GVREPLERR = 150375946;
LITDEF	int ERR_DBFILERDONLY = 150375954;
LITDEF	int ERR_RMWIDTHPOS = 150375962;
LITDEF	int ERR_OFFSETINV = 150375970;
LITDEF	int ERR_JOBPARTOOLONG = 150375978;
LITDEF	int ERR_RLNKINTEGINFO = 150375987;
LITDEF	int ERR_RUNPARAMERR = 150375994;
LITDEF	int ERR_FNNAMENEG = 150376002;
LITDEF	int ERR_ORDER2 = 150376010;
LITDEF	int ERR_MUNOUPGRD = 150376018;
LITDEF	int ERR_REORGCTRLY = 150376027;
LITDEF	int ERR_TSTRTPARM = 150376034;
LITDEF	int ERR_TRIGNAMENF = 150376042;
LITDEF	int ERR_TRIGZBREAKREM = 150376048;
LITDEF	int ERR_TLVLZERO = 150376058;
LITDEF	int ERR_TRESTNOT = 150376066;
LITDEF	int ERR_TPLOCK = 150376074;
LITDEF	int ERR_TPQUIT = 150376082;
LITDEF	int ERR_TPFAIL = 150376090;
LITDEF	int ERR_TPRETRY = 150376098;
LITDEF	int ERR_TPTOODEEP = 150376106;
LITDEF	int ERR_ZDEFACTIVE = 150376114;
LITDEF	int ERR_ZDEFOFLOW = 150376122;
LITDEF	int ERR_MUPRESTERR = 150376130;
LITDEF	int ERR_MUBCKNODIR = 150376138;
LITDEF	int ERR_TRANS2BIG = 150376146;
LITDEF	int ERR_INVBITLEN = 150376154;
LITDEF	int ERR_INVBITSTR = 150376162;
LITDEF	int ERR_INVBITPOS = 150376170;
LITDEF	int ERR_PARNORMAL = 150376177;
LITDEF	int ERR_FILEPATHTOOLONG = 150376186;
LITDEF	int ERR_RMWIDTHTOOBIG = 150376194;
LITDEF	int ERR_PATTABNOTFND = 150376202;
LITDEF	int ERR_OBJFILERR = 150376210;
LITDEF	int ERR_SRCFILERR = 418811674;
LITDEF	int ERR_NEGFRACPWR = 150376226;
LITDEF	int ERR_MTNOSKIP = 150376234;
LITDEF	int ERR_CETOOMANY = 150376242;
LITDEF	int ERR_CEUSRERROR = 150376250;
LITDEF	int ERR_CEBIGSKIP = 150376258;
LITDEF	int ERR_CETOOLONG = 150376266;
LITDEF	int ERR_CENOINDIR = 150376274;
LITDEF	int ERR_COLLATIONUNDEF = 150376282;
LITDEF	int ERR_MSTACKCRIT = 150376290;
LITDEF	int ERR_GTMSECSHRSRVF = 150376298;
LITDEF	int ERR_FREEZECTRL = 150376307;
LITDEF	int ERR_JNLFLUSH = 150376315;
LITDEF	int ERR_CCPSIGDMP = 150376323;
LITDEF	int ERR_NOPRINCIO = 150376332;
LITDEF	int ERR_INVPORTSPEC = 150376338;
LITDEF	int ERR_INVADDRSPEC = 150376346;
LITDEF	int ERR_MUREENCRYPTEND = 150376355;
LITDEF	int ERR_CRYPTJNLMISMATCH = 150376362;
LITDEF	int ERR_SOCKWAIT = 150376370;
LITDEF	int ERR_SOCKACPT = 150376378;
LITDEF	int ERR_SOCKINIT = 150376386;
LITDEF	int ERR_OPENCONN = 150376394;
LITDEF	int ERR_DEVNOTIMP = 150376402;
LITDEF	int ERR_PATALTER2LARGE = 150376410;
LITDEF	int ERR_DBREMOTE = 150376418;
LITDEF	int ERR_JNLREQUIRED = 150376426;
LITDEF	int ERR_TPMIXUP = 150376434;
LITDEF	int ERR_HTOFLOW = 150376442;
LITDEF	int ERR_RMNOBIGRECORD = 150376450;
LITDEF	int ERR_DBBMSIZE = 150376459;
LITDEF	int ERR_DBBMBARE = 150376467;
LITDEF	int ERR_DBBMINV = 150376475;
LITDEF	int ERR_DBBMMSTR = 150376483;
LITDEF	int ERR_DBROOTBURN = 150376491;
LITDEF	int ERR_REPLSTATEERR = 150376498;
LITDEF	int ERR_VMSMEMORY = 150376508;
LITDEF	int ERR_DBDIRTSUBSC = 150376515;
LITDEF	int ERR_TIMEROVFL = 150376522;
LITDEF	int ERR_GTMASSERT = 150376532;
LITDEF	int ERR_DBFHEADERR4 = 150376539;
LITDEF	int ERR_DBADDRANGE = 150376547;
LITDEF	int ERR_DBQUELINK = 150376555;
LITDEF	int ERR_DBCRERR = 150376563;
LITDEF	int ERR_MUSTANDALONE = 150376571;
LITDEF	int ERR_MUNOACTION = 150376578;
LITDEF	int ERR_RMBIGSHARE = 150376586;
LITDEF	int ERR_TPRESTART = 150376595;
LITDEF	int ERR_SOCKWRITE = 150376602;
LITDEF	int ERR_DBCNTRLERR = 150376611;
LITDEF	int ERR_NOTERMENV = 150376619;
LITDEF	int ERR_NOTERMENTRY = 150376627;
LITDEF	int ERR_NOTERMINFODB = 150376635;
LITDEF	int ERR_INVACCMETHOD = 150376642;
LITDEF	int ERR_JNLOPNERR = 150376650;
LITDEF	int ERR_JNLRECTYPE = 150376658;
LITDEF	int ERR_JNLTRANSGTR = 150376666;
LITDEF	int ERR_JNLTRANSLSS = 150376674;
LITDEF	int ERR_JNLWRERR = 150376682;
LITDEF	int ERR_FILEIDMATCH = 150376690;
LITDEF	int ERR_EXTSRCLIN = 150376699;
LITDEF	int ERR_EXTSRCLOC = 150376707;
LITDEF	int ERR_BIGNOACL = 150376714;
LITDEF	int ERR_ERRCALL = 150376722;
LITDEF	int ERR_ZCCTENV = 150376730;
LITDEF	int ERR_ZCCTOPN = 418812194;
LITDEF	int ERR_ZCCTNULLF = 150376746;
LITDEF	int ERR_ZCUNAVAIL = 150376754;
LITDEF	int ERR_ZCENTNAME = 150376762;
LITDEF	int ERR_ZCCOLON = 150376770;
LITDEF	int ERR_ZCRTNTYP = 150376778;
LITDEF	int ERR_ZCRCALLNAME = 150376786;
LITDEF	int ERR_ZCRPARMNAME = 150376794;
LITDEF	int ERR_ZCUNTYPE = 150376802;
LITDEF	int ERR_ZCMLTSTATUS = 150376810;
LITDEF	int ERR_ZCSTATUSRET = 150376818;
LITDEF	int ERR_ZCMAXPARAM = 150376826;
LITDEF	int ERR_ZCCSQRBR = 150376834;
LITDEF	int ERR_ZCPREALLNUMEX = 150376842;
LITDEF	int ERR_ZCPREALLVALPAR = 150376850;
LITDEF	int ERR_VERMISMATCH = 150376858;
LITDEF	int ERR_JNLCNTRL = 150376866;
LITDEF	int ERR_TRIGNAMBAD = 150376874;
LITDEF	int ERR_BUFRDTIMEOUT = 150376882;
LITDEF	int ERR_INVALIDRIP = 150376890;
LITDEF	int ERR_BLKSIZ512 = 150376899;
LITDEF	int ERR_MUTEXERR = 150376906;
LITDEF	int ERR_JNLVSIZE = 150376914;
LITDEF	int ERR_MUTEXLCKALERT = 150376920;
LITDEF	int ERR_MUTEXFRCDTERM = 150376928;
LITDEF	int ERR_GTMSECSHR = 150376938;
LITDEF	int ERR_GTMSECSHRSRVFID = 150376944;
LITDEF	int ERR_GTMSECSHRSRVFIL = 150376952;
LITDEF	int ERR_FREEBLKSLOW = 150376960;
LITDEF	int ERR_PROTNOTSUP = 150376970;
LITDEF	int ERR_DELIMSIZNA = 150376978;
LITDEF	int ERR_INVCTLMNE = 150376986;
LITDEF	int ERR_SOCKLISTEN = 150376994;
LITDEF	int ERR_RESTORESUCCESS = 150377003;
LITDEF	int ERR_ADDRTOOLONG = 150377010;
LITDEF	int ERR_GTMSECSHRGETSEMFAIL = 150377016;
LITDEF	int ERR_CPBEYALLOC = 150377026;
LITDEF	int ERR_DBRDONLY = 418812490;
LITDEF	int ERR_DUPTN = 150377040;
LITDEF	int ERR_TRESTLOC = 150377050;
LITDEF	int ERR_REPLPOOLINST = 150377058;
LITDEF	int ERR_ZCVECTORINDX = 150377064;
LITDEF	int ERR_REPLNOTON = 150377074;
LITDEF	int ERR_JNLMOVED = 150377082;
LITDEF	int ERR_EXTRFMT = 150377090;
LITDEF	int ERR_CALLERID = 150377099;
LITDEF	int ERR_KRNLKILL = 150377108;
LITDEF	int ERR_MEMORYRECURSIVE = 150377116;
LITDEF	int ERR_FREEZEID = 150377123;
LITDEF	int ERR_UNUSEDMSG778 = 150377130;
LITDEF	int ERR_DSEINVALBLKID = 150377138;
LITDEF	int ERR_PINENTRYERR = 150377146;
LITDEF	int ERR_BCKUPBUFLUSH = 150377154;
LITDEF	int ERR_NOFORKCORE = 150377160;
LITDEF	int ERR_JNLREAD = 150377170;
LITDEF	int ERR_JNLMINALIGN = 150377176;
LITDEF	int ERR_JOBSTARTCMDFAIL = 150377186;
LITDEF	int ERR_JNLPOOLSETUP = 150377194;
LITDEF	int ERR_JNLSTATEOFF = 150377202;
LITDEF	int ERR_RECVPOOLSETUP = 150377210;
LITDEF	int ERR_REPLCOMM = 150377218;
LITDEF	int ERR_NOREPLCTDREG = 150377224;
LITDEF	int ERR_REPLINFO = 150377235;
LITDEF	int ERR_REPLWARN = 150377240;
LITDEF	int ERR_REPLERR = 150377250;
LITDEF	int ERR_JNLNMBKNOTPRCD = 150377258;
LITDEF	int ERR_REPLFILIOERR = 150377266;
LITDEF	int ERR_REPLBRKNTRANS = 150377274;
LITDEF	int ERR_TTWIDTHTOOBIG = 150377282;
LITDEF	int ERR_REPLLOGOPN = 418812746;
LITDEF	int ERR_REPLFILTER = 150377298;
LITDEF	int ERR_GBLMODFAIL = 150377306;
LITDEF	int ERR_TTLENGTHTOOBIG = 150377314;
LITDEF	int ERR_TPTIMEOUT = 150377322;
LITDEF	int ERR_NORTN = 150377330;
LITDEF	int ERR_JNLFILNOTCHG = 150377338;
LITDEF	int ERR_EVENTLOGERR = 150377346;
LITDEF	int ERR_UPDATEFILEOPEN = 418812810;
LITDEF	int ERR_JNLBADRECFMT = 150377362;
LITDEF	int ERR_NULLCOLLDIFF = 150377370;
LITDEF	int ERR_MUKILLIP = 150377376;
LITDEF	int ERR_JNLRDONLY = 418812842;
LITDEF	int ERR_ANCOMPTINC = 150377394;
LITDEF	int ERR_ABNCOMPTINC = 150377402;
LITDEF	int ERR_RECLOAD = 150377410;
LITDEF	int ERR_SOCKNOTFND = 150377418;
LITDEF	int ERR_CURRSOCKOFR = 150377426;
LITDEF	int ERR_SOCKETEXIST = 150377434;
LITDEF	int ERR_LISTENPASSBND = 150377442;
LITDEF	int ERR_DBCLNUPINFO = 150377451;
LITDEF	int ERR_MUNODWNGRD = 150377458;
LITDEF	int ERR_REPLTRANS2BIG = 150377466;
LITDEF	int ERR_RDFLTOOLONG = 150377474;
LITDEF	int ERR_MUNOFINISH = 150377482;
LITDEF	int ERR_DBFILEXT = 150377491;
LITDEF	int ERR_JNLFSYNCERR = 150377498;
LITDEF	int ERR_ICUNOTENABLED = 150377504;
LITDEF	int ERR_ZCPREALLVALINV = 150377514;
LITDEF	int ERR_NEWJNLFILECREAT = 150377523;
LITDEF	int ERR_DSKSPACEFLOW = 150377531;
LITDEF	int ERR_GVINCRFAIL = 150377538;
LITDEF	int ERR_ISOLATIONSTSCHN = 150377546;
LITDEF	int ERR_REPLGBL2LONG = 150377554;
LITDEF	int ERR_TRACEON = 150377562;
LITDEF	int ERR_TOOMANYCLIENTS = 150377570;
LITDEF	int ERR_NOEXCLUDE = 150377579;
LITDEF	int ERR_GVINCRISOLATION = 150377586;
LITDEF	int ERR_EXCLUDEREORG = 150377592;
LITDEF	int ERR_REORGINC = 150377600;
LITDEF	int ERR_ASC2EBCDICCONV = 150377610;
LITDEF	int ERR_GTMSECSHRSTART = 150377618;
LITDEF	int ERR_DBVERPERFWARN1 = 150377624;
LITDEF	int ERR_FILEIDGBLSEC = 150377634;
LITDEF	int ERR_GBLSECNOTGDS = 150377642;
LITDEF	int ERR_BADGBLSECVER = 150377650;
LITDEF	int ERR_RECSIZENOTEVEN = 150377658;
LITDEF	int ERR_BUFFLUFAILED = 150377666;
LITDEF	int ERR_MUQUALINCOMP = 150377674;
LITDEF	int ERR_DISTPATHMAX = 150377682;
LITDEF	int ERR_FILEOPENFAIL = 418813146;
LITDEF	int ERR_IMAGENAME = 150377698;
LITDEF	int ERR_GTMSECSHRPERM = 150377706;
LITDEF	int ERR_GTMDISTUNDEF = 150377714;
LITDEF	int ERR_SYSCALL = 150377722;
LITDEF	int ERR_MAXGTMPATH = 150377730;
LITDEF	int ERR_TROLLBK2DEEP = 150377738;
LITDEF	int ERR_INVROLLBKLVL = 150377746;
LITDEF	int ERR_OLDBINEXTRACT = 150377752;
LITDEF	int ERR_ACOMPTBINC = 150377762;
LITDEF	int ERR_NOTREPLICATED = 150377768;
LITDEF	int ERR_DBPREMATEOF = 150377778;
LITDEF	int ERR_KILLBYSIG = 150377788;
LITDEF	int ERR_KILLBYSIGUINFO = 150377796;
LITDEF	int ERR_KILLBYSIGSINFO1 = 150377804;
LITDEF	int ERR_KILLBYSIGSINFO2 = 150377812;
LITDEF	int ERR_SIGILLOPC = 150377820;
LITDEF	int ERR_SIGILLOPN = 150377828;
LITDEF	int ERR_SIGILLADR = 150377836;
LITDEF	int ERR_SIGILLTRP = 150377844;
LITDEF	int ERR_SIGPRVOPC = 150377852;
LITDEF	int ERR_SIGPRVREG = 150377860;
LITDEF	int ERR_SIGCOPROC = 150377868;
LITDEF	int ERR_SIGBADSTK = 150377876;
LITDEF	int ERR_SIGADRALN = 150377884;
LITDEF	int ERR_SIGADRERR = 150377892;
LITDEF	int ERR_SIGOBJERR = 150377900;
LITDEF	int ERR_SIGINTDIV = 150377908;
LITDEF	int ERR_SIGINTOVF = 150377916;
LITDEF	int ERR_SIGFLTDIV = 150377924;
LITDEF	int ERR_SIGFLTOVF = 150377932;
LITDEF	int ERR_SIGFLTUND = 150377940;
LITDEF	int ERR_SIGFLTRES = 150377948;
LITDEF	int ERR_SIGFLTINV = 150377956;
LITDEF	int ERR_SIGMAPERR = 150377964;
LITDEF	int ERR_SIGACCERR = 418813428;
LITDEF	int ERR_TRNLOGFAIL = 150377978;
LITDEF	int ERR_INVDBGLVL = 150377986;
LITDEF	int ERR_DBMAXNRSUBS = 150377995;
LITDEF	int ERR_GTMSECSHRSCKSEL = 150378002;
LITDEF	int ERR_GTMSECSHRTMOUT = 150378011;
LITDEF	int ERR_GTMSECSHRRECVF = 150378018;
LITDEF	int ERR_GTMSECSHRSENDF = 150378026;
LITDEF	int ERR_SIZENOTVALID8 = 150378034;
LITDEF	int ERR_GTMSECSHROPCMP = 150378044;
LITDEF	int ERR_GTMSECSHRSUIDF = 150378048;
LITDEF	int ERR_GTMSECSHRSGIDF = 150378056;
LITDEF	int ERR_GTMSECSHRSSIDF = 150378064;
LITDEF	int ERR_GTMSECSHRFORKF = 150378076;
LITDEF	int ERR_DBFSYNCERR = 150378082;
LITDEF	int ERR_UNUSEDMSG898 = 150378090;
LITDEF	int ERR_SCNDDBNOUPD = 150378098;
LITDEF	int ERR_MUINFOUINT4 = 150378107;
LITDEF	int ERR_NLMISMATCHCALC = 150378114;
LITDEF	int ERR_RELINKCTLFULL = 150378122;
LITDEF	int ERR_MUPIPSET2BIG = 150378128;
LITDEF	int ERR_DBBADNSUB = 150378138;
LITDEF	int ERR_DBBADKYNM = 150378146;
LITDEF	int ERR_DBBADPNTR = 150378154;
LITDEF	int ERR_DBBNPNTR = 150378162;
LITDEF	int ERR_DBINCLVL = 150378170;
LITDEF	int ERR_DBBFSTAT = 150378178;
LITDEF	int ERR_DBBDBALLOC = 150378186;
LITDEF	int ERR_DBMRKFREE = 150378194;
LITDEF	int ERR_DBMRKBUSY = 150378200;
LITDEF	int ERR_DBBSIZZRO = 150378210;
LITDEF	int ERR_DBSZGT64K = 150378218;
LITDEF	int ERR_DBNOTMLTP = 150378226;
LITDEF	int ERR_DBTNTOOLG = 150378235;
LITDEF	int ERR_DBBPLMLT512 = 150378242;
LITDEF	int ERR_DBBPLMGT2K = 150378250;
LITDEF	int ERR_MUINFOUINT8 = 150378259;
LITDEF	int ERR_DBBPLNOT512 = 150378266;
LITDEF	int ERR_MUINFOSTR = 150378275;
LITDEF	int ERR_DBUNDACCMT = 150378283;
LITDEF	int ERR_DBTNNEQ = 150378291;
LITDEF	int ERR_MUPGRDSUCC = 150378297;
LITDEF	int ERR_DBDSRDFMTCHNG = 150378307;
LITDEF	int ERR_DBFGTBC = 150378312;
LITDEF	int ERR_DBFSTBC = 150378322;
LITDEF	int ERR_DBFSTHEAD = 150378330;
LITDEF	int ERR_DBCREINCOMP = 150378338;
LITDEF	int ERR_DBFLCORRP = 150378346;
LITDEF	int ERR_DBHEADINV = 150378354;
LITDEF	int ERR_DBINCRVER = 150378362;
LITDEF	int ERR_DBINVGBL = 150378370;
LITDEF	int ERR_DBKEYGTIND = 150378378;
LITDEF	int ERR_DBGTDBMAX = 150378386;
LITDEF	int ERR_DBKGTALLW = 150378394;
LITDEF	int ERR_DBLTSIBL = 150378402;
LITDEF	int ERR_DBLRCINVSZ = 150378410;
LITDEF	int ERR_MUREUPDWNGRDEND = 150378419;
LITDEF	int ERR_DBLOCMBINC = 150378424;
LITDEF	int ERR_DBLVLINC = 150378432;
LITDEF	int ERR_DBMBSIZMX = 150378440;
LITDEF	int ERR_DBMBSIZMN = 150378450;
LITDEF	int ERR_DBMBTNSIZMX = 150378459;
LITDEF	int ERR_DBMBMINCFRE = 150378464;
LITDEF	int ERR_DBMBPINCFL = 150378472;
LITDEF	int ERR_DBMBPFLDLBM = 150378480;
LITDEF	int ERR_DBMBPFLINT = 150378488;
LITDEF	int ERR_DBMBPFLDIS = 150378496;
LITDEF	int ERR_DBMBPFRDLBM = 150378504;
LITDEF	int ERR_DBMBPFRINT = 150378512;
LITDEF	int ERR_DBMAXKEYEXC = 150378522;
LITDEF	int ERR_REPLAHEAD = 150378530;
LITDEF	int ERR_MUPIPSET2SML = 150378536;
LITDEF	int ERR_DBREADBM = 150378546;
LITDEF	int ERR_DBCOMPTOOLRG = 150378554;
LITDEF	int ERR_DBVERPERFWARN2 = 150378560;
LITDEF	int ERR_DBRBNTOOLRG = 150378570;
LITDEF	int ERR_DBRBNLBMN = 150378578;
LITDEF	int ERR_DBRBNNEG = 150378586;
LITDEF	int ERR_DBRLEVTOOHI = 150378594;
LITDEF	int ERR_DBRLEVLTONE = 150378602;
LITDEF	int ERR_DBSVBNMIN = 150378610;
LITDEF	int ERR_DBTTLBLK0 = 150378618;
LITDEF	int ERR_DBNOTDB = 150378626;
LITDEF	int ERR_DBTOTBLK = 150378634;
LITDEF	int ERR_DBTN = 150378643;
LITDEF	int ERR_DBNOREGION = 418814106;
LITDEF	int ERR_DBTNRESETINC = 150378656;
LITDEF	int ERR_DBTNLTCTN = 150378666;
LITDEF	int ERR_DBTNRESET = 150378674;
LITDEF	int ERR_MUTEXRSRCCLNUP = 150378683;
LITDEF	int ERR_SEMWT2LONG = 150378690;
LITDEF	int ERR_REPLINSTOPEN = 418814154;
LITDEF	int ERR_REPLINSTCLOSE = 150378706;
LITDEF	int ERR_JOBSETUP = 150378714;
LITDEF	int ERR_DBCRERR8 = 150378723;
LITDEF	int ERR_NUMPROCESSORS = 150378728;
LITDEF	int ERR_DBADDRANGE8 = 150378739;
LITDEF	int ERR_RNDWNSEMFAIL = 150378747;
LITDEF	int ERR_GTMSECSHRSHUTDN = 150378755;
LITDEF	int ERR_NOSPACECRE = 150378762;
LITDEF	int ERR_LOWSPACECRE = 150378768;
LITDEF	int ERR_WAITDSKSPACE = 150378779;
LITDEF	int ERR_OUTOFSPACE = 150378788;
LITDEF	int ERR_JNLPVTINFO = 150378795;
LITDEF	int ERR_NOSPACEEXT = 150378802;
LITDEF	int ERR_WCBLOCKED = 150378808;
LITDEF	int ERR_REPLJNLCLOSED = 150378818;
LITDEF	int ERR_RENAMEFAIL = 150378824;
LITDEF	int ERR_FILERENAME = 150378835;
LITDEF	int ERR_JNLBUFINFO = 150378843;
LITDEF	int ERR_SDSEEKERR = 150378850;
LITDEF	int ERR_LOCALSOCKREQ = 150378858;
LITDEF	int ERR_TPNOTACID = 150378867;
LITDEF	int ERR_JNLSETDATA2LONG = 150378874;
LITDEF	int ERR_JNLNEWREC = 150378882;
LITDEF	int ERR_REPLFTOKSEM = 150378890;
LITDEF	int ERR_SOCKNOTPASSED = 150378898;
LITDEF	int ERR_EXTRIOERR = 150378906;
LITDEF	int ERR_EXTRCLOSEERR = 150378914;
LITDEF	int ERR_CONNSOCKREQ = 150378922;
LITDEF	int ERR_REPLEXITERR = 150378930;
LITDEF	int ERR_MUDESTROYSUC = 150378939;
LITDEF	int ERR_DBRNDWN = 150378946;
LITDEF	int ERR_MUDESTROYFAIL = 150378955;
LITDEF	int ERR_NOTALLDBOPN = 150378964;
LITDEF	int ERR_MUSELFBKUP = 150378970;
LITDEF	int ERR_DBDANGER = 150378976;
LITDEF	int ERR_UNUSEDMSG1010 = 150378986;
LITDEF	int ERR_TCGETATTR = 150378994;
LITDEF	int ERR_TCSETATTR = 150379002;
LITDEF	int ERR_IOWRITERR = 150379010;
LITDEF	int ERR_REPLINSTWRITE = 418814474;
LITDEF	int ERR_DBBADFREEBLKCTR = 150379024;
LITDEF	int ERR_REQ2RESUME = 150379035;
LITDEF	int ERR_TIMERHANDLER = 150379040;
LITDEF	int ERR_FREEMEMORY = 150379050;
LITDEF	int ERR_MUREPLSECDEL = 150379059;
LITDEF	int ERR_MUREPLSECNOTDEL = 150379067;
LITDEF	int ERR_MUJPOOLRNDWNSUC = 150379075;
LITDEF	int ERR_MURPOOLRNDWNSUC = 150379083;
LITDEF	int ERR_MUJPOOLRNDWNFL = 150379090;
LITDEF	int ERR_MURPOOLRNDWNFL = 150379098;
LITDEF	int ERR_MUREPLPOOL = 150379107;
LITDEF	int ERR_REPLACCSEM = 150379114;
LITDEF	int ERR_JNLFLUSHNOPROG = 150379120;
LITDEF	int ERR_REPLINSTCREATE = 150379130;
LITDEF	int ERR_SUSPENDING = 150379139;
LITDEF	int ERR_SOCKBFNOTEMPTY = 150379146;
LITDEF	int ERR_ILLESOCKBFSIZE = 150379154;
LITDEF	int ERR_NOSOCKETINDEV = 150379162;
LITDEF	int ERR_SETSOCKOPTERR = 150379170;
LITDEF	int ERR_GETSOCKOPTERR = 150379178;
LITDEF	int ERR_NOSUCHPROC = 150379187;
LITDEF	int ERR_DSENOFINISH = 150379194;
LITDEF	int ERR_LKENOFINISH = 150379202;
LITDEF	int ERR_NOCHLEFT = 150379212;
LITDEF	int ERR_MULOGNAMEDEF = 150379218;
LITDEF	int ERR_BUFOWNERSTUCK = 150379226;
LITDEF	int ERR_ACTIVATEFAIL = 150379234;
LITDEF	int ERR_DBRNDWNWRN = 150379240;
LITDEF	int ERR_DLLNOOPEN = 150379250;
LITDEF	int ERR_DLLNORTN = 150379258;
LITDEF	int ERR_DLLNOCLOSE = 150379266;
LITDEF	int ERR_FILTERNOTALIVE = 150379274;
LITDEF	int ERR_FILTERCOMM = 150379282;
LITDEF	int ERR_FILTERBADCONV = 150379290;
LITDEF	int ERR_PRIMARYISROOT = 150379298;
LITDEF	int ERR_GVQUERYGETFAIL = 150379306;
LITDEF	int ERR_UNUSEDMSG1051 = 150379314;
LITDEF	int ERR_MERGEDESC = 150379322;
LITDEF	int ERR_MERGEINCOMPL = 150379328;
LITDEF	int ERR_DBNAMEMISMATCH = 150379338;
LITDEF	int ERR_DBIDMISMATCH = 150379346;
LITDEF	int ERR_DEVOPENFAIL = 150379354;
LITDEF	int ERR_IPCNOTDEL = 150379363;
LITDEF	int ERR_XCVOIDRET = 150379370;
LITDEF	int ERR_MURAIMGFAIL = 150379378;
LITDEF	int ERR_REPLINSTUNDEF = 150379386;
LITDEF	int ERR_REPLINSTACC = 150379394;
LITDEF	int ERR_NOJNLPOOL = 150379402;
LITDEF	int ERR_NORECVPOOL = 150379410;
LITDEF	int ERR_FTOKERR = 150379418;
LITDEF	int ERR_REPLREQRUNDOWN = 150379426;
LITDEF	int ERR_BLKCNTEDITFAIL = 150379435;
LITDEF	int ERR_SEMREMOVED = 150379443;
LITDEF	int ERR_REPLINSTFMT = 150379450;
LITDEF	int ERR_SEMKEYINUSE = 150379458;
LITDEF	int ERR_XTRNTRANSERR = 150379466;
LITDEF	int ERR_XTRNTRANSDLL = 150379474;
LITDEF	int ERR_XTRNRETVAL = 150379482;
LITDEF	int ERR_XTRNRETSTR = 150379490;
LITDEF	int ERR_INVECODEVAL = 150379498;
LITDEF	int ERR_SETECODE = 150379506;
LITDEF	int ERR_INVSTACODE = 150379514;
LITDEF	int ERR_REPEATERROR = 150379522;
LITDEF	int ERR_NOCANONICNAME = 150379530;
LITDEF	int ERR_NOSUBSCRIPT = 150379538;
LITDEF	int ERR_SYSTEMVALUE = 150379546;
LITDEF	int ERR_SIZENOTVALID4 = 150379554;
LITDEF	int ERR_STRNOTVALID = 150379562;
LITDEF	int ERR_CREDNOTPASSED = 150379570;
LITDEF	int ERR_ERRWETRAP = 150379578;
LITDEF	int ERR_TRACINGON = 150379587;
LITDEF	int ERR_CITABENV = 150379594;
LITDEF	int ERR_CITABOPN = 150379602;
LITDEF	int ERR_CIENTNAME = 150379610;
LITDEF	int ERR_CIRTNTYP = 150379618;
LITDEF	int ERR_CIRCALLNAME = 150379626;
LITDEF	int ERR_CIRPARMNAME = 150379634;
LITDEF	int ERR_CIDIRECTIVE = 150379642;
LITDEF	int ERR_CIPARTYPE = 150379650;
LITDEF	int ERR_CIUNTYPE = 150379658;
LITDEF	int ERR_CINOENTRY = 150379666;
LITDEF	int ERR_JNLINVSWITCHLMT = 150379674;
LITDEF	int ERR_SETZDIR = 418815138;
LITDEF	int ERR_JOBACTREF = 150379690;
LITDEF	int ERR_ECLOSTMID = 150379696;
LITDEF	int ERR_ZFF2MANY = 150379706;
LITDEF	int ERR_JNLFSYNCLSTCK = 150379712;
LITDEF	int ERR_DELIMWIDTH = 150379722;
LITDEF	int ERR_DBBMLCORRUPT = 150379730;
LITDEF	int ERR_DLCKAVOIDANCE = 150379738;
LITDEF	int ERR_WRITERSTUCK = 150379746;
LITDEF	int ERR_PATNOTFOUND = 150379754;
LITDEF	int ERR_INVZDIRFORM = 150379762;
LITDEF	int ERR_ZDIROUTOFSYNC = 150379768;
LITDEF	int ERR_GBLNOEXIST = 150379779;
LITDEF	int ERR_MAXBTLEVEL = 150379786;
LITDEF	int ERR_INVMNEMCSPC = 150379794;
LITDEF	int ERR_JNLALIGNSZCHG = 150379803;
LITDEF	int ERR_SEFCTNEEDSFULLB = 150379810;
LITDEF	int ERR_GVFAILCORE = 150379818;
LITDEF	int ERR_UNUSEDMSG1115 = 150379826;
LITDEF	int ERR_DBFRZRESETSUC = 150379835;
LITDEF	int ERR_JNLFILEXTERR = 150379842;
LITDEF	int ERR_JOBEXAMDONE = 150379851;
LITDEF	int ERR_JOBEXAMFAIL = 150379858;
LITDEF	int ERR_JOBINTRRQST = 150379866;
LITDEF	int ERR_ERRWZINTR = 150379874;
LITDEF	int ERR_CLIERR = 150379882;
LITDEF	int ERR_REPLNOBEFORE = 150379888;
LITDEF	int ERR_REPLJNLCNFLCT = 150379896;
LITDEF	int ERR_JNLDISABLE = 150379904;
LITDEF	int ERR_FILEEXISTS = 150379914;
LITDEF	int ERR_JNLSTATE = 150379923;
LITDEF	int ERR_REPLSTATE = 150379931;
LITDEF	int ERR_JNLCREATE = 150379939;
LITDEF	int ERR_JNLNOCREATE = 150379946;
LITDEF	int ERR_JNLFNF = 150379955;
LITDEF	int ERR_PREVJNLLINKCUT = 150379963;
LITDEF	int ERR_PREVJNLLINKSET = 150379971;
LITDEF	int ERR_FILENAMETOOLONG = 150379978;
LITDEF	int ERR_REQRECOV = 150379986;
LITDEF	int ERR_JNLTRANS2BIG = 150379994;
LITDEF	int ERR_JNLSWITCHTOOSM = 150380002;
LITDEF	int ERR_JNLSWITCHSZCHG = 150380011;
LITDEF	int ERR_NOTRNDMACC = 150380018;
LITDEF	int ERR_TMPFILENOCRE = 150380026;
LITDEF	int ERR_UNUSEDMSG1141 = 150380034;
LITDEF	int ERR_JNLSENDOPER = 150380043;
LITDEF	int ERR_DDPSUBSNUL = 150380050;
LITDEF	int ERR_DDPNOCONNECT = 150380058;
LITDEF	int ERR_DDPCONGEST = 150380066;
LITDEF	int ERR_DDPSHUTDOWN = 150380074;
LITDEF	int ERR_DDPTOOMANYPROCS = 150380082;
LITDEF	int ERR_DDPBADRESPONSE = 150380090;
LITDEF	int ERR_DDPINVCKT = 150380098;
LITDEF	int ERR_DDPVOLSETCONFIG = 150380106;
LITDEF	int ERR_DDPCONFGOOD = 150380113;
LITDEF	int ERR_DDPCONFIGNORE = 150380121;
LITDEF	int ERR_DDPCONFINCOMPL = 150380128;
LITDEF	int ERR_DDPCONFBADVOL = 150380138;
LITDEF	int ERR_DDPCONFBADUCI = 150380146;
LITDEF	int ERR_DDPCONFBADGLD = 150380154;
LITDEF	int ERR_DDPRECSIZNOTNUM = 150380162;
LITDEF	int ERR_DDPOUTMSG2BIG = 150380170;
LITDEF	int ERR_DDPNOSERVER = 150380178;
LITDEF	int ERR_MUTEXRELEASED = 150380186;
LITDEF	int ERR_JNLCRESTATUS = 150380192;
LITDEF	int ERR_ZBREAKFAIL = 150380203;
LITDEF	int ERR_DLLVERSION = 150380210;
LITDEF	int ERR_INVZROENT = 150380218;
LITDEF	int ERR_DDPLOGERR = 150380226;
LITDEF	int ERR_GETSOCKNAMERR = 150380234;
LITDEF	int ERR_INVGTMEXIT = 150380242;
LITDEF	int ERR_CIMAXPARAM = 150380250;
LITDEF	int ERR_CITPNESTED = 150380258;
LITDEF	int ERR_CIMAXLEVELS = 150380266;
LITDEF	int ERR_JOBINTRRETHROW = 150380274;
LITDEF	int ERR_STARFILE = 150380282;
LITDEF	int ERR_NOSTARFILE = 150380290;
LITDEF	int ERR_MUJNLSTAT = 150380299;
LITDEF	int ERR_JNLTPNEST = 150380304;
LITDEF	int ERR_REPLOFFJNLON = 150380314;
LITDEF	int ERR_FILEDELFAIL = 150380320;
LITDEF	int ERR_INVQUALTIME = 150380330;
LITDEF	int ERR_NOTPOSITIVE = 150380338;
LITDEF	int ERR_INVREDIRQUAL = 150380346;
LITDEF	int ERR_INVERRORLIM = 150380354;
LITDEF	int ERR_INVIDQUAL = 150380362;
LITDEF	int ERR_INVTRNSQUAL = 150380370;
LITDEF	int ERR_JNLNOBIJBACK = 150380378;
LITDEF	int ERR_SETREG2RESYNC = 150380387;
LITDEF	int ERR_JNLALIGNTOOSM = 150380392;
LITDEF	int ERR_JNLFILEOPNERR = 150380402;
LITDEF	int ERR_JNLFILECLOSERR = 150380410;
LITDEF	int ERR_REPLSTATEOFF = 150380418;
LITDEF	int ERR_MUJNLPREVGEN = 150380427;
LITDEF	int ERR_MUPJNLINTERRUPT = 150380434;
LITDEF	int ERR_ROLLBKINTERRUPT = 150380442;
LITDEF	int ERR_RLBKJNSEQ = 150380451;
LITDEF	int ERR_REPLRECFMT = 150380460;
LITDEF	int ERR_PRIMARYNOTROOT = 150380466;
LITDEF	int ERR_DBFRZRESETFL = 150380474;
LITDEF	int ERR_JNLCYCLE = 150380482;
LITDEF	int ERR_JNLPREVRECOV = 150380490;
LITDEF	int ERR_RESOLVESEQNO = 150380499;
LITDEF	int ERR_BOVTNGTEOVTN = 150380506;
LITDEF	int ERR_BOVTMGTEOVTM = 150380514;
LITDEF	int ERR_BEGSEQGTENDSEQ = 150380522;
LITDEF	int ERR_DBADDRALIGN = 150380531;
LITDEF	int ERR_DBWCVERIFYSTART = 150380539;
LITDEF	int ERR_DBWCVERIFYEND = 150380547;
LITDEF	int ERR_MUPIPSIG = 150380555;
LITDEF	int ERR_HTSHRINKFAIL = 150380560;
LITDEF	int ERR_STPEXPFAIL = 150380570;
LITDEF	int ERR_DBBTUWRNG = 150380576;
LITDEF	int ERR_DBBTUFIXED = 150380587;
LITDEF	int ERR_DBMAXREC2BIG = 150380594;
LITDEF	int ERR_UNUSEDMSG1212 = 150380602;
LITDEF	int ERR_UNUSEDMSG1213 = 150380610;
LITDEF	int ERR_UNUSEDMSG1214 = 150380618;
LITDEF	int ERR_UNUSEDMSG1215 = 150380626;
LITDEF	int ERR_DBMINRESBYTES = 150380634;
LITDEF	int ERR_UNUSEDMSG1217 = 150380642;
LITDEF	int ERR_UNUSEDMSG1218 = 150380651;
LITDEF	int ERR_UNUSEDMSG1219 = 150380658;
LITDEF	int ERR_UNUSEDMSG1220 = 150380666;
LITDEF	int ERR_UNUSEDMSG1221 = 150380674;
LITDEF	int ERR_UNUSEDMSG1222 = 150380682;
LITDEF	int ERR_UNUSEDMSG1223 = 150380690;
LITDEF	int ERR_DYNUPGRDFAIL = 150380698;
LITDEF	int ERR_MMNODYNDWNGRD = 150380706;
LITDEF	int ERR_MMNODYNUPGRD = 150380714;
LITDEF	int ERR_MUDWNGRDNRDY = 150380722;
LITDEF	int ERR_MUDWNGRDTN = 150380730;
LITDEF	int ERR_MUDWNGRDNOTPOS = 150380738;
LITDEF	int ERR_MUUPGRDNRDY = 150380746;
LITDEF	int ERR_TNWARN = 150380752;
LITDEF	int ERR_TNTOOLARGE = 150380762;
LITDEF	int ERR_SHMPLRECOV = 150380771;
LITDEF	int ERR_MUNOSTRMBKUP = 150380776;
LITDEF	int ERR_EPOCHTNHI = 150380786;
LITDEF	int ERR_CHNGTPRSLVTM = 150380795;
LITDEF	int ERR_JNLUNXPCTERR = 150380802;
LITDEF	int ERR_OMISERVHANG = 150380811;
LITDEF	int ERR_RSVDBYTE2HIGH = 150380818;
LITDEF	int ERR_BKUPTMPFILOPEN = 418816282;
LITDEF	int ERR_BKUPTMPFILWRITE = 418816290;
LITDEF	int ERR_UNUSEDMSG1242 = 150380842;
LITDEF	int ERR_UNUSEDMSG1243 = 150380850;
LITDEF	int ERR_UNUSEDMSG1244 = 150380858;
LITDEF	int ERR_REPLINSTMISMTCH = 150380866;
LITDEF	int ERR_REPLINSTREAD = 418816330;
LITDEF	int ERR_REPLINSTDBMATCH = 150380882;
LITDEF	int ERR_REPLINSTNMSAME = 150380890;
LITDEF	int ERR_REPLINSTNMUNDEF = 150380898;
LITDEF	int ERR_REPLINSTNMLEN = 150380906;
LITDEF	int ERR_REPLINSTNOHIST = 150380914;
LITDEF	int ERR_REPLINSTSECLEN = 150380922;
LITDEF	int ERR_REPLINSTSECMTCH = 150380930;
LITDEF	int ERR_REPLINSTSECNONE = 150380938;
LITDEF	int ERR_REPLINSTSECUNDF = 150380946;
LITDEF	int ERR_REPLINSTSEQORD = 150380954;
LITDEF	int ERR_REPLINSTSTNDALN = 150380962;
LITDEF	int ERR_REPLREQROLLBACK = 150380970;
LITDEF	int ERR_REQROLLBACK = 150380978;
LITDEF	int ERR_INVOBJFILE = 150380986;
LITDEF	int ERR_SRCSRVEXISTS = 150380994;
LITDEF	int ERR_SRCSRVNOTEXIST = 150381002;
LITDEF	int ERR_SRCSRVTOOMANY = 150381010;
LITDEF	int ERR_JNLPOOLBADSLOT = 150381016;
LITDEF	int ERR_NOENDIANCVT = 150381026;
LITDEF	int ERR_ENDIANCVT = 150381035;
LITDEF	int ERR_DBENDIAN = 150381042;
LITDEF	int ERR_BADCHSET = 150381050;
LITDEF	int ERR_BADCASECODE = 150381058;
LITDEF	int ERR_BADCHAR = 150381066;
LITDEF	int ERR_DLRCILLEGAL = 150381074;
LITDEF	int ERR_NONUTF8LOCALE = 150381082;
LITDEF	int ERR_INVDLRCVAL = 150381090;
LITDEF	int ERR_DBMISALIGN = 150381098;
LITDEF	int ERR_LOADINVCHSET = 150381106;
LITDEF	int ERR_DLLCHSETM = 150381114;
LITDEF	int ERR_DLLCHSETUTF8 = 150381122;
LITDEF	int ERR_BOMMISMATCH = 150381130;
LITDEF	int ERR_WIDTHTOOSMALL = 150381138;
LITDEF	int ERR_SOCKMAX = 150381146;
LITDEF	int ERR_PADCHARINVALID = 150381154;
LITDEF	int ERR_ZCNOPREALLOUTPAR = 150381162;
LITDEF	int ERR_SVNEXPECTED = 150381170;
LITDEF	int ERR_SVNONEW = 150381178;
LITDEF	int ERR_ZINTDIRECT = 150381186;
LITDEF	int ERR_ZINTRECURSEIO = 150381194;
LITDEF	int ERR_MRTMAXEXCEEDED = 150381202;
LITDEF	int ERR_JNLCLOSED = 150381210;
LITDEF	int ERR_RLBKNOBIMG = 150381218;
LITDEF	int ERR_RLBKJNLNOBIMG = 150381227;
LITDEF	int ERR_RLBKLOSTTNONLY = 150381235;
LITDEF	int ERR_KILLBYSIGSINFO3 = 150381244;
LITDEF	int ERR_GTMSECSHRTMPPATH = 150381251;
LITDEF	int ERR_GTMERREXIT = 150381258;
LITDEF	int ERR_INVMEMRESRV = 150381264;
LITDEF	int ERR_OPCOMMISSED = 150381275;
LITDEF	int ERR_COMMITWAITSTUCK = 150381282;
LITDEF	int ERR_COMMITWAITPID = 150381290;
LITDEF	int ERR_UPDREPLSTATEOFF = 150381298;
LITDEF	int ERR_LITNONGRAPH = 150381304;
LITDEF	int ERR_DBFHEADERR8 = 150381315;
LITDEF	int ERR_MMBEFOREJNL = 150381320;
LITDEF	int ERR_MMNOBFORRPL = 150381328;
LITDEF	int ERR_KILLABANDONED = 150381336;
LITDEF	int ERR_BACKUPKILLIP = 150381344;
LITDEF	int ERR_LOGTOOLONG = 150381354;
LITDEF	int ERR_NOALIASLIST = 150381362;
LITDEF	int ERR_ALIASEXPECTED = 150381370;
LITDEF	int ERR_VIEWLVN = 150381378;
LITDEF	int ERR_DZWRNOPAREN = 150381386;
LITDEF	int ERR_DZWRNOALIAS = 150381394;
LITDEF	int ERR_FREEZEERR = 150381402;
LITDEF	int ERR_CLOSEFAIL = 150381410;
LITDEF	int ERR_CRYPTINIT = 150381418;
LITDEF	int ERR_CRYPTOPFAILED = 150381426;
LITDEF	int ERR_CRYPTDLNOOPEN = 150381434;
LITDEF	int ERR_CRYPTNOV4 = 150381442;
LITDEF	int ERR_CRYPTNOMM = 150381450;
LITDEF	int ERR_READONLYNOBG = 150381458;
LITDEF	int ERR_CRYPTKEYFETCHFAILED = 418816922;
LITDEF	int ERR_CRYPTKEYFETCHFAILEDNF = 150381474;
LITDEF	int ERR_CRYPTHASHGENFAILED = 150381482;
LITDEF	int ERR_CRYPTNOKEY = 150381490;
LITDEF	int ERR_BADTAG = 150381498;
LITDEF	int ERR_ICUVERLT36 = 150381506;
LITDEF	int ERR_ICUSYMNOTFOUND = 150381514;
LITDEF	int ERR_STUCKACT = 150381523;
LITDEF	int ERR_CALLINAFTERXIT = 150381530;
LITDEF	int ERR_LOCKSPACEFULL = 150381538;
LITDEF	int ERR_IOERROR = 150381546;
LITDEF	int ERR_MAXSSREACHED = 150381554;
LITDEF	int ERR_SNAPSHOTNOV4 = 150381562;
LITDEF	int ERR_SSV4NOALLOW = 150381570;
LITDEF	int ERR_SSTMPDIRSTAT = 418817034;
LITDEF	int ERR_SSTMPCREATE = 418817042;
LITDEF	int ERR_JNLFILEDUP = 150381594;
LITDEF	int ERR_SSPREMATEOF = 150381602;
LITDEF	int ERR_SSFILOPERR = 150381610;
LITDEF	int ERR_REGSSFAIL = 150381618;
LITDEF	int ERR_SSSHMCLNUPFAIL = 150381626;
LITDEF	int ERR_SSFILCLNUPFAIL = 150381634;
LITDEF	int ERR_SETINTRIGONLY = 150381642;
LITDEF	int ERR_MAXTRIGNEST = 150381650;
LITDEF	int ERR_TRIGCOMPFAIL = 150381658;
LITDEF	int ERR_NOZTRAPINTRIG = 150381666;
LITDEF	int ERR_ZTWORMHOLE2BIG = 150381674;
LITDEF	int ERR_JNLENDIANLITTLE = 150381682;
LITDEF	int ERR_JNLENDIANBIG = 150381690;
LITDEF	int ERR_TRIGINVCHSET = 150381698;
LITDEF	int ERR_TRIGREPLSTATE = 150381706;
LITDEF	int ERR_GVDATAGETFAIL = 150381714;
LITDEF	int ERR_TRIG2NOTRIG = 150381720;
LITDEF	int ERR_ZGOTOINVLVL = 150381730;
LITDEF	int ERR_TRIGTCOMMIT = 150381738;
LITDEF	int ERR_TRIGTLVLCHNG = 150381746;
LITDEF	int ERR_TRIGNAMEUNIQ = 150381754;
LITDEF	int ERR_ZTRIGINVACT = 150381762;
LITDEF	int ERR_INDRCOMPFAIL = 150381770;
LITDEF	int ERR_QUITALSINV = 150381778;
LITDEF	int ERR_PROCTERM = 150381784;
LITDEF	int ERR_SRCLNNTDSP = 150381795;
LITDEF	int ERR_ARROWNTDSP = 150381803;
LITDEF	int ERR_TRIGDEFBAD = 150381810;
LITDEF	int ERR_TRIGSUBSCRANGE = 150381818;
LITDEF	int ERR_TRIGDATAIGNORE = 150381827;
LITDEF	int ERR_TRIGIS = 150381835;
LITDEF	int ERR_TCOMMITDISALLOW = 150381842;
LITDEF	int ERR_SSATTACHSHM = 150381850;
LITDEF	int ERR_TRIGDEFNOSYNC = 150381856;
LITDEF	int ERR_TRESTMAX = 150381866;
LITDEF	int ERR_ZLINKBYPASS = 150381875;
LITDEF	int ERR_GBLEXPECTED = 150381882;
LITDEF	int ERR_GVZTRIGFAIL = 150381890;
LITDEF	int ERR_MUUSERLBK = 150381898;
LITDEF	int ERR_SETINSETTRIGONLY = 150381906;
LITDEF	int ERR_DZTRIGINTRIG = 150381914;
LITDEF	int ERR_LSINSERTED = 150381920;
LITDEF	int ERR_BOOLSIDEFFECT = 150381928;
LITDEF	int ERR_DBBADUPGRDSTATE = 150381936;
LITDEF	int ERR_WRITEWAITPID = 150381946;
LITDEF	int ERR_ZGOCALLOUTIN = 150381954;
LITDEF	int ERR_REPLNOXENDIAN = 150381962;
LITDEF	int ERR_REPLXENDIANFAIL = 150381970;
LITDEF	int ERR_ZGOTOINVLVL2 = 150381978;
LITDEF	int ERR_GTMSECSHRCHDIRF = 150381986;
LITDEF	int ERR_JNLORDBFLU = 418817450;
LITDEF	int ERR_ZCCLNUPRTNMISNG = 150382002;
LITDEF	int ERR_ZCINVALIDKEYWORD = 150382010;
LITDEF	int ERR_REPLMULTINSTUPDATE = 150382018;
LITDEF	int ERR_DBSHMNAMEDIFF = 150382026;
LITDEF	int ERR_SHMREMOVED = 150382035;
LITDEF	int ERR_DEVICEWRITEONLY = 150382042;
LITDEF	int ERR_ICUERROR = 150382050;
LITDEF	int ERR_ZDATEBADDATE = 150382058;
LITDEF	int ERR_ZDATEBADTIME = 150382066;
LITDEF	int ERR_COREINPROGRESS = 150382074;
LITDEF	int ERR_MAXSEMGETRETRY = 150382082;
LITDEF	int ERR_JNLNOREPL = 150382090;
LITDEF	int ERR_JNLRECINCMPL = 150382098;
LITDEF	int ERR_JNLALLOCGROW = 150382107;
LITDEF	int ERR_INVTRCGRP = 150382114;
LITDEF	int ERR_MUINFOUINT6 = 150382123;
LITDEF	int ERR_NOLOCKMATCH = 150382131;
LITDEF	int ERR_BADREGION = 150382138;
LITDEF	int ERR_LOCKSPACEUSE = 150382147;
LITDEF	int ERR_JIUNHNDINT = 150382154;
LITDEF	int ERR_GTMASSERT2 = 150382164;
LITDEF	int ERR_ZTRIGNOTRW = 418817626;
LITDEF	int ERR_TRIGMODREGNOTRW = 418817634;
LITDEF	int ERR_INSNOTJOINED = 150382186;
LITDEF	int ERR_INSROLECHANGE = 150382194;
LITDEF	int ERR_INSUNKNOWN = 150382202;
LITDEF	int ERR_NORESYNCSUPPLONLY = 150382210;
LITDEF	int ERR_NORESYNCUPDATERONLY = 150382218;
LITDEF	int ERR_NOSUPPLSUPPL = 150382226;
LITDEF	int ERR_REPL2OLD = 150382234;
LITDEF	int ERR_EXTRFILEXISTS = 150382242;
LITDEF	int ERR_MUUSERECOV = 150382250;
LITDEF	int ERR_SECNOTSUPPLEMENTARY = 150382258;
LITDEF	int ERR_SUPRCVRNEEDSSUPSRC = 150382266;
LITDEF	int ERR_PEERPIDMISMATCH = 150382274;
LITDEF	int ERR_SETITIMERFAILED = 150382284;
LITDEF	int ERR_UPDSYNC2MTINS = 150382290;
LITDEF	int ERR_UPDSYNCINSTFILE = 150382298;
LITDEF	int ERR_REUSEINSTNAME = 150382306;
LITDEF	int ERR_RCVRMANYSTRMS = 150382314;
LITDEF	int ERR_RSYNCSTRMVAL = 150382322;
LITDEF	int ERR_RLBKSTRMSEQ = 150382331;
LITDEF	int ERR_RESOLVESEQSTRM = 150382339;
LITDEF	int ERR_REPLINSTDBSTRM = 150382346;
LITDEF	int ERR_RESUMESTRMNUM = 150382354;
LITDEF	int ERR_ORLBKSTART = 150382363;
LITDEF	int ERR_ORLBKTERMNTD = 150382370;
LITDEF	int ERR_ORLBKCMPLT = 150382379;
LITDEF	int ERR_ORLBKNOSTP = 150382387;
LITDEF	int ERR_ORLBKFRZPROG = 150382395;
LITDEF	int ERR_ORLBKFRZOVER = 150382403;
LITDEF	int ERR_ORLBKNOV4BLK = 150382410;
LITDEF	int ERR_DBROLLEDBACK = 150382418;
LITDEF	int ERR_DSEWCREINIT = 150382427;
LITDEF	int ERR_MURNDWNOVRD = 150382435;
LITDEF	int ERR_REPLONLNRLBK = 150382442;
LITDEF	int ERR_SRVLCKWT2LNG = 150382450;
LITDEF	int ERR_IGNBMPMRKFREE = 150382459;
LITDEF	int ERR_PERMGENFAIL = 418817922;
LITDEF	int ERR_PERMGENDIAG = 150382475;
LITDEF	int ERR_MUTRUNC1ATIME = 150382483;
LITDEF	int ERR_MUTRUNCBACKINPROG = 150382491;
LITDEF	int ERR_MUTRUNCERROR = 150382498;
LITDEF	int ERR_MUTRUNCFAIL = 150382506;
LITDEF	int ERR_MUTRUNCNOSPACE = 150382515;
LITDEF	int ERR_MUTRUNCNOTBG = 150382522;
LITDEF	int ERR_MUTRUNCNOV4 = 150382532;
LITDEF	int ERR_MUTRUNCPERCENT = 150382538;
LITDEF	int ERR_MUTRUNCSSINPROG = 150382547;
LITDEF	int ERR_MUTRUNCSUCCESS = 150382555;
LITDEF	int ERR_RSYNCSTRMSUPPLONLY = 150382562;
LITDEF	int ERR_STRMNUMIS = 150382571;
LITDEF	int ERR_STRMNUMMISMTCH1 = 150382578;
LITDEF	int ERR_STRMNUMMISMTCH2 = 150382586;
LITDEF	int ERR_STRMSEQMISMTCH = 150382594;
LITDEF	int ERR_LOCKSPACEINFO = 150382603;
LITDEF	int ERR_JRTNULLFAIL = 150382610;
LITDEF	int ERR_LOCKSUB2LONG = 150382618;
LITDEF	int ERR_RESRCWAIT = 150382627;
LITDEF	int ERR_RESRCINTRLCKBYPAS = 150382635;
LITDEF	int ERR_DBFHEADERRANY = 150382643;
LITDEF	int ERR_REPLINSTFROZEN = 150382650;
LITDEF	int ERR_REPLINSTFREEZECOMMENT = 150382659;
LITDEF	int ERR_REPLINSTUNFROZEN = 150382667;
LITDEF	int ERR_DSKNOSPCAVAIL = 150382675;
LITDEF	int ERR_DSKNOSPCBLOCKED = 150382682;
LITDEF	int ERR_DSKSPCAVAILABLE = 150382691;
LITDEF	int ERR_ENOSPCQIODEFER = 150382699;
LITDEF	int ERR_CUSTOMFILOPERR = 150382706;
LITDEF	int ERR_CUSTERRNOTFND = 150382714;
LITDEF	int ERR_CUSTERRSYNTAX = 150382722;
LITDEF	int ERR_ORLBKINPROG = 150382731;
LITDEF	int ERR_DBSPANGLOINCMP = 150382738;
LITDEF	int ERR_DBSPANCHUNKORD = 150382746;
LITDEF	int ERR_DBDATAMX = 150382754;
LITDEF	int ERR_DBIOERR = 150382762;
LITDEF	int ERR_INITORRESUME = 150382770;
LITDEF	int ERR_GTMSECSHRNOARG0 = 150382780;
LITDEF	int ERR_GTMSECSHRISNOT = 150382788;
LITDEF	int ERR_GTMSECSHRBADDIR = 150382796;
LITDEF	int ERR_JNLBUFFREGUPD = 150382800;
LITDEF	int ERR_JNLBUFFDBUPD = 150382808;
LITDEF	int ERR_LOCKINCR2HIGH = 150382818;
LITDEF	int ERR_LOCKIS = 150382827;
LITDEF	int ERR_LDSPANGLOINCMP = 150382834;
LITDEF	int ERR_MUFILRNDWNFL2 = 150382842;
LITDEF	int ERR_MUINSTFROZEN = 150382851;
LITDEF	int ERR_MUINSTUNFROZEN = 150382859;
LITDEF	int ERR_GTMEISDIR = 150382866;
LITDEF	int ERR_SPCLZMSG = 150382874;
LITDEF	int ERR_MUNOTALLINTEG = 150382880;
LITDEF	int ERR_BKUPRUNNING = 150382890;
LITDEF	int ERR_MUSIZEINVARG = 150382898;
LITDEF	int ERR_MUSIZEFAIL = 150382906;
LITDEF	int ERR_SIDEEFFECTEVAL = 150382912;
LITDEF	int ERR_CRYPTINIT2 = 150382922;
LITDEF	int ERR_CRYPTDLNOOPEN2 = 150382930;
LITDEF	int ERR_CRYPTBADCONFIG = 418818394;
LITDEF	int ERR_DBCOLLREQ = 150382944;
LITDEF	int ERR_SETEXTRENV = 150382954;
LITDEF	int ERR_NOTALLDBRNDWN = 150382962;
LITDEF	int ERR_TPRESTNESTERR = 150382970;
LITDEF	int ERR_JNLFILRDOPN = 150382978;
LITDEF	int ERR_SEQNUMSEARCHTIMEOUT = 418818442;
LITDEF	int ERR_FTOKKEY = 150382995;
LITDEF	int ERR_SEMID = 150383003;
LITDEF	int ERR_JNLQIOSALVAGE = 150383011;
LITDEF	int ERR_FAKENOSPCLEARED = 150383019;
LITDEF	int ERR_MMFILETOOLARGE = 150383026;
LITDEF	int ERR_BADZPEEKARG = 150383034;
LITDEF	int ERR_BADZPEEKRANGE = 150383042;
LITDEF	int ERR_BADZPEEKFMT = 150383050;
LITDEF	int ERR_DBMBMINCFREFIXED = 150383056;
LITDEF	int ERR_NULLENTRYREF = 150383066;
LITDEF	int ERR_ZPEEKNORPLINFO = 150383074;
LITDEF	int ERR_MMREGNOACCESS = 150383082;
LITDEF	int ERR_UNUSEDMSG1525 = 150383090;
LITDEF	int ERR_MALLOCCRIT = 150383096;
LITDEF	int ERR_HOSTCONFLICT = 150383106;
LITDEF	int ERR_GETADDRINFO = 150383114;
LITDEF	int ERR_GETNAMEINFO = 150383122;
LITDEF	int ERR_SOCKBIND = 150383130;
LITDEF	int ERR_INSTFRZDEFER = 150383139;
LITDEF	int ERR_VIEWARGTOOLONG = 150383146;
LITDEF	int ERR_REGOPENFAIL = 150383154;
LITDEF	int ERR_REPLINSTNOSHM = 150383162;
LITDEF	int ERR_DEVPARMTOOSMALL = 150383170;
LITDEF	int ERR_REMOTEDBNOSPGBL = 150383178;
LITDEF	int ERR_NCTCOLLSPGBL = 150383186;
LITDEF	int ERR_ACTCOLLMISMTCH = 150383194;
LITDEF	int ERR_GBLNOMAPTOREG = 150383202;
LITDEF	int ERR_ISSPANGBL = 150383210;
LITDEF	int ERR_TPNOSUPPORT = 150383218;
LITDEF	int ERR_EXITSTATUS = 150383226;
LITDEF	int ERR_ZATRANSERR = 150383234;
LITDEF	int ERR_FILTERTIMEDOUT = 150383242;
LITDEF	int ERR_TLSDLLNOOPEN = 150383250;
LITDEF	int ERR_TLSINIT = 150383258;
LITDEF	int ERR_TLSCONVSOCK = 150383266;
LITDEF	int ERR_TLSHANDSHAKE = 150383274;
LITDEF	int ERR_TLSCONNINFO = 150383280;
LITDEF	int ERR_TLSIOERROR = 150383290;
LITDEF	int ERR_TLSRENEGOTIATE = 150383298;
LITDEF	int ERR_REPLNOTLS = 150383306;
LITDEF	int ERR_COLTRANSSTR2LONG = 150383314;
LITDEF	int ERR_SOCKPASS = 150383322;
LITDEF	int ERR_SOCKACCEPT = 150383330;
LITDEF	int ERR_NOSOCKHANDLE = 150383338;
LITDEF	int ERR_TRIGLOADFAIL = 150383346;
LITDEF	int ERR_SOCKPASSDATAMIX = 150383354;
LITDEF	int ERR_NOGTCMDB = 150383362;
LITDEF	int ERR_NOUSERDB = 150383370;
LITDEF	int ERR_DSENOTOPEN = 150383378;
LITDEF	int ERR_ZSOCKETATTR = 150383386;
LITDEF	int ERR_ZSOCKETNOTSOCK = 150383394;
LITDEF	int ERR_CHSETALREADY = 150383402;
LITDEF	int ERR_DSEMAXBLKSAV = 150383410;
LITDEF	int ERR_BLKINVALID = 150383418;
LITDEF	int ERR_CANTBITMAP = 150383426;
LITDEF	int ERR_AIMGBLKFAIL = 150383434;
LITDEF	int ERR_GTMDISTUNVERIF = 150383442;
LITDEF	int ERR_CRYPTNOAPPEND = 150383450;
LITDEF	int ERR_CRYPTNOSEEK = 150383458;
LITDEF	int ERR_CRYPTNOTRUNC = 150383466;
LITDEF	int ERR_CRYPTNOKEYSPEC = 150383474;
LITDEF	int ERR_CRYPTNOOVERRIDE = 150383482;
LITDEF	int ERR_CRYPTKEYTOOBIG = 150383490;
LITDEF	int ERR_CRYPTBADWRTPOS = 150383498;
LITDEF	int ERR_LABELNOTFND = 150383506;
LITDEF	int ERR_RELINKCTLERR = 150383514;
LITDEF	int ERR_INVLINKTMPDIR = 150383522;
LITDEF	int ERR_NOEDITOR = 150383530;
LITDEF	int ERR_UPDPROC = 150383538;
LITDEF	int ERR_HLPPROC = 150383546;
LITDEF	int ERR_REPLNOHASHTREC = 150383554;
LITDEF	int ERR_REMOTEDBNOTRIG = 150383562;
LITDEF	int ERR_NEEDTRIGUPGRD = 150383570;
LITDEF	int ERR_REQRLNKCTLRNDWN = 150383578;
LITDEF	int ERR_RLNKCTLRNDWNSUC = 150383587;
LITDEF	int ERR_RLNKCTLRNDWNFL = 150383594;
LITDEF	int ERR_MPROFRUNDOWN = 150383602;
LITDEF	int ERR_ZPEEKNOJNLINFO = 150383610;
LITDEF	int ERR_TLSPARAM = 150383618;
LITDEF	int ERR_RLNKRECLATCH = 150383626;
LITDEF	int ERR_RLNKSHMLATCH = 150383634;
LITDEF	int ERR_JOBLVN2LONG = 150383642;
LITDEF	int ERR_NLRESTORE = 150383648;
LITDEF	int ERR_PREALLOCATEFAIL = 150383658;
LITDEF	int ERR_NODFRALLOCSUPP = 150383664;
LITDEF	int ERR_LASTWRITERBYPAS = 150383672;
LITDEF	int ERR_TRIGUPBADLABEL = 150383682;
LITDEF	int ERR_WEIRDSYSTIME = 150383690;
LITDEF	int ERR_REPLSRCEXITERR = 150383696;
LITDEF	int ERR_INVZBREAK = 150383706;
LITDEF	int ERR_INVTMPDIR = 150383714;
LITDEF	int ERR_ARCTLMAXHIGH = 150383720;
LITDEF	int ERR_ARCTLMAXLOW = 150383728;
LITDEF	int ERR_NONTPRESTART = 150383739;
LITDEF	int ERR_PBNPARMREQ = 150383746;
LITDEF	int ERR_PBNNOPARM = 150383754;
LITDEF	int ERR_PBNUNSUPSTRUCT = 150383762;
LITDEF	int ERR_PBNINVALID = 150383770;
LITDEF	int ERR_PBNNOFIELD = 150383778;
LITDEF	int ERR_JNLDBSEQNOMATCH = 150383786;
LITDEF	int ERR_MULTIPROCLATCH = 150383794;
LITDEF	int ERR_INVLOCALE = 150383802;
LITDEF	int ERR_NOMORESEMCNT = 150383811;
LITDEF	int ERR_SETQUALPROB = 150383818;
LITDEF	int ERR_EXTRINTEGRITY = 150383826;
LITDEF	int ERR_CRYPTKEYRELEASEFAILED = 418819290;
LITDEF	int ERR_MUREENCRYPTSTART = 150383843;
LITDEF	int ERR_MUREENCRYPTV4NOALLOW = 150383850;
LITDEF	int ERR_ENCRYPTCONFLT = 150383858;
LITDEF	int ERR_JNLPOOLRECOVERY = 150383866;
LITDEF	int ERR_LOCKTIMINGINTP = 150383872;
LITDEF	int ERR_PBNUNSUPTYPE = 150383882;
LITDEF	int ERR_DBFHEADLRU = 150383891;
LITDEF	int ERR_ASYNCIONOV4 = 150383898;
LITDEF	int ERR_AIOCANCELTIMEOUT = 150383906;
LITDEF	int ERR_DBGLDMISMATCH = 150383914;
LITDEF	int ERR_DBBLKSIZEALIGN = 150383922;
LITDEF	int ERR_ASYNCIONOMM = 150383930;
LITDEF	int ERR_RESYNCSEQLOW = 150383938;
LITDEF	int ERR_DBNULCOL = 150383946;
LITDEF	int ERR_UTF16ENDIAN = 150383954;
LITDEF	int ERR_OFRZACTIVE = 150383960;
LITDEF	int ERR_OFRZAUTOREL = 150383968;
LITDEF	int ERR_OFRZCRITREL = 150383976;
LITDEF	int ERR_OFRZCRITSTUCK = 150383984;
LITDEF	int ERR_OFRZNOTHELD = 150383992;
LITDEF	int ERR_AIOBUFSTUCK = 150384002;
LITDEF	int ERR_DBDUPNULCOL = 150384010;
LITDEF	int ERR_CHANGELOGINTERVAL = 150384019;
LITDEF	int ERR_DBNONUMSUBS = 150384026;
LITDEF	int ERR_AUTODBCREFAIL = 150384034;
LITDEF	int ERR_RNDWNSTATSDBFAIL = 150384042;
LITDEF	int ERR_STATSDBNOTSUPP = 150384050;
LITDEF	int ERR_TPNOSTATSHARE = 150384058;
LITDEF	int ERR_FNTRANSERROR = 150384066;
LITDEF	int ERR_NOCRENETFILE = 150384074;
LITDEF	int ERR_DSKSPCCHK = 150384082;
LITDEF	int ERR_NOCREMMBIJ = 150384090;
LITDEF	int ERR_FILECREERR = 150384098;
LITDEF	int ERR_RAWDEVUNSUP = 150384106;
LITDEF	int ERR_DBFILECREATED = 150384115;
LITDEF	int ERR_PCTYRESERVED = 150384122;
LITDEF	int ERR_REGFILENOTFOUND = 418819587;
LITDEF	int ERR_DRVLONGJMP = 150384138;
LITDEF	int ERR_INVSTATSDB = 150384146;
LITDEF	int ERR_STATSDBERR = 150384154;
LITDEF	int ERR_STATSDBINUSE = 150384162;
LITDEF	int ERR_STATSDBFNERR = 150384170;
LITDEF	int ERR_JNLSWITCHRETRY = 150384179;
LITDEF	int ERR_JNLSWITCHFAIL = 150384186;
LITDEF	int ERR_CLISTRTOOLONG = 150384194;
LITDEF	int ERR_LVMONBADVAL = 150384202;
LITDEF	int ERR_RESTRICTEDOP = 418819666;
LITDEF	int ERR_RESTRICTSYNTAX = 150384218;
LITDEF	int ERR_MUCREFILERR = 418819682;
LITDEF	int ERR_JNLBUFFPHS2SALVAGE = 150384235;
LITDEF	int ERR_JNLPOOLPHS2SALVAGE = 150384243;
LITDEF	int ERR_MURNDWNARGLESS = 150384251;
LITDEF	int ERR_DBFREEZEON = 150384259;
LITDEF	int ERR_DBFREEZEOFF = 150384267;
LITDEF	int ERR_STPCRIT = 150384274;
LITDEF	int ERR_STPOFLOW = 150384284;
LITDEF	int ERR_SYSUTILCONF = 150384290;
LITDEF	int ERR_MSTACKSZNA = 150384299;
LITDEF	int ERR_JNLEXTRCTSEQNO = 150384306;
LITDEF	int ERR_INVSEQNOQUAL = 150384314;
LITDEF	int ERR_LOWSPC = 150384323;
LITDEF	int ERR_FAILEDRECCOUNT = 150384330;
LITDEF	int ERR_LOADRECCNT = 150384339;
LITDEF	int ERR_COMMFILTERERR = 150384346;
LITDEF	int ERR_NOFILTERNEST = 150384354;
LITDEF	int ERR_MLKHASHTABERR = 150384362;
LITDEF	int ERR_LOCKCRITOWNER = 150384371;
LITDEF	int ERR_MLKHASHWRONG = 150384378;
LITDEF	int ERR_XCRETNULLREF = 150384386;
LITDEF	int ERR_EXTCALLBOUNDS = 150384396;
LITDEF	int ERR_EXCEEDSPREALLOC = 150384402;
LITDEF	int ERR_ZTIMEOUT = 150384408;
LITDEF	int ERR_ERRWZTIMEOUT = 150384418;
LITDEF	int ERR_MLKHASHRESIZE = 150384427;
LITDEF	int ERR_MLKHASHRESIZEFAIL = 150384432;
LITDEF	int ERR_MLKCLEANED = 150384443;
LITDEF	int ERR_NOTMNAME = 150384450;
LITDEF	int ERR_DEVNAMERESERVED = 150384458;
LITDEF	int ERR_ORLBKREL = 150384467;
LITDEF	int ERR_ORLBKRESTART = 150384475;
LITDEF	int ERR_UNIQNAME = 150384482;
LITDEF	int ERR_APDINITFAIL = 150384490;
LITDEF	int ERR_APDCONNFAIL = 150384498;
LITDEF	int ERR_APDLOGFAIL = 150384506;
LITDEF	int ERR_STATSDBMEMERR = 150384514;
LITDEF	int ERR_BUFSPCDELAY = 150384520;
LITDEF	int ERR_AIOQUEUESTUCK = 150384530;
LITDEF	int ERR_INVGVPATQUAL = 150384538;
LITDEF	int ERR_NULLPATTERN = 150384544;
LITDEF	int ERR_MLKREHASH = 150384555;
LITDEF	int ERR_MUKEEPPERCENT = 150384562;
LITDEF	int ERR_MUKEEPNODEC = 150384570;
LITDEF	int ERR_MUKEEPNOTRUNC = 150384578;
LITDEF	int ERR_MUTRUNCNOSPKEEP = 150384587;
LITDEF	int ERR_TERMHANGUP = 150384594;
LITDEF	int ERR_DBFILNOFULLWRT = 150384600;
LITDEF	int ERR_BADCONNECTPARAM = 150384610;
LITDEF	int ERR_BADPARAMCOUNT = 150384618;
LITDEF	int ERR_REPLALERT = 150384624;
LITDEF	int ERR_SHUT2QUICK = 150384632;
LITDEF	int ERR_REPLNORESP = 150384640;
LITDEF	int ERR_REPL0BACKLOG = 150384649;
LITDEF	int ERR_REPLBACKLOG = 150384658;
LITDEF	int ERR_INVSHUTDOWN = 150384666;
LITDEF	int ERR_SOCKBLOCKERR = 150384674;
LITDEF	int ERR_SOCKWAITARG = 150384682;
LITDEF	int ERR_LASTTRANS = 150384691;
LITDEF	int ERR_SRCBACKLOGSTATUS = 150384699;
LITDEF	int ERR_BKUPRETRY = 150384707;
LITDEF	int ERR_BKUPPROGRESS = 150384715;
LITDEF	int ERR_BKUPFILEPERM = 150384722;
>>>>>>> eb3ea98c (GT.M V7.0-002):sr_i386/merrors_ctl.c


LITDEF	int merrors_undocarr[] = {
	0,	/* ACK */
	656,	/* ASC2EBCDICCONV */
	1444,	/* DBGLDMISMATCH */
	1472,	/* DRVLONGJMP */
	44,	/* ENQ */
	1332,	/* FAKENOSPCLEARED */
	595,	/* FREEZEID */
	703,	/* INVDBGLVL */
	508,	/* JNLREQUIRED */
	410,	/* JNLWRTNOWWRTR */
	989,	/* JOBINTRRETHROW */
	938,	/* JOBINTRRQST */
	336,	/* LKSECINIT */
	1480,	/* LVMONBADVAL */
	824,	/* MUDESTROYFAIL */
	822,	/* MUDESTROYSUC */
	895,	/* REPEATERROR */
	1260,	/* REPLONLNRLBK */
	898,	/* SYSTEMVALUE */
	467,	/* TPRETRY */
	252,	/* WILLEXPIRE */
	62,	/* YDIRTSZ */
	469,	/* ZDEFACTIVE */
	470,	/* ZDEFOFLOW */
	1189,	/* ZLINKBYPASS */
};


GBLDEF	err_ctl merrors_ctl = {
	246,
	"GTM",
	&merrors[0],
	1546,
	&merrors_undocarr[0],
	25
};

