!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!								!
!	Copyright 2001, 2013 Fidelity Information Services, Inc	!
!								!
!	This source code contains the intellectual property	!
!	of its copyright holder(s), and is made available	!
!	under a license.  If you do not know the terms of	!
!	the license, please stop and do not read further.	!
!								!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	.FACILITY	GTM,246/PREFIX=ERR_
	.TITLE	MERRORS	Error Messages for GTM
!
!   The code numbers for the errors that have been
!   standardized by the MUMPS Development Committee are
!   listed below.
!   Those error messages that have a standardized
!   error code have a definition that ends in !/ansi=###
!   If not specified, the value for "ansi" defaults to 0.
!   (Note that the first entry in this file MUST have
!   an !/ansi= specification; any subsequent ones are
!   optional.)
!
!     M1, Naked indicator undefined
!     M2, Invalid combination with P _fncodatom_
!     M3, $RANDOM seed less than 1
!     M4, No true condition in $SELECT
!     M5, _lineref_ less than zero
!     M6, Undefined _lvn_
!     M7, Undefined _gvn_
!     M8, Undefined _svn_
!     M9, Divide by zero
!    M10, Invalid pattern match range
!    M11, No parameters passed
!    M12, Invalid _lineref_ (negative offset)
!    M13, Invalid _lineref_ (label not found)
!    M14, _line_ level not 1
!    M15, Undefined index variable
!    M16, Argumented QUIT not allowed
!    M17, Argumented QUIT required
!    M18, Fixed length READ not greater than zero
!    M19, Cannot copy a tree or subtree into itself
!    M20, _line_ must have _formallist_
!    M21, Formal parameter occurs multiple times (original text: Algorithm specification invalid)
!    M22, SET or KILL to ^$GLOBAL when data in global
!    M23, SET or KILL to ^$JOB for non-existent job number
!    M24, Change to collation algorithm while subscripted local variables defined
!    M25, Attempt to modify currently executing routine
!    M26, Non-existent _environment_
!    M27, Attempt to rollback a transaction that is not restartable
!    M28, Mathematical function, parameter out of range
!    M29, SET or KILL on _ssvn_ not allowed by implementation
!    M30, Reference to _glvn_ with different collating sequence within a collating algorithm
!    M31, _controlmnemonic_ used for device without a _mnemonicspace_ selected
!    M32, _controlmnemonic_ used in user-defined _mnemonicspace_ which has no associated line
!    M33, SET or KILL to ^$ROUTINE when _routine_ exists
!    M34, --- currently unassigned ---
!    M35, Device does not support _mnemonicspace_
!    M36, Incompatible _mnemonicspace_s
!    M37, READ from device identified by the empty string
!    M38, Invalid _ssvn_ subscript
!    M39, Name of variable expected (original text: Invalid $NAME argument)
!    M40, Call-by-reference in JOB _actual_
!    M41, Invalid LOCK argument within a TRANSACTION
!    M42, Invalid QUIT within a TRANSACTION
!    M43, Invalid range ($X, $Y)
!    M44, Invalid _command_ outside of a TRANSACTION
!    M45, Invalid GOTO reference
!    M46, Invalid attribute name
!    M47, Invalid attribute value (original text: Invalid attribute name)
!    M48, Nonexistent window, element or choice
!    M49, Invalid attempt to set focus
!    M50, Attempt to reference a non M-Term window in an OPEN command
!    M51, Attempt to destroy M-Term window prior to CLOSE
!    M52, Required attribute missing
!    M53, Invalid argument for font function
!    M54, Attempt to create non-modal child of a modal parent
!    M55, Invalid nested TSTART command
!    M56, Name length exceeds implementation's limit
!    M57, More than one defining occurrence of label in routine
!    M58, Too few formal parameters
!    M59, Environment reference not permitted for this _ssvn_
!    M60, Undefined _ssvn_
!    M61, Attempt to OPEN file with conflicting ACCESS parameters
!    M62, Illegal value for ACCESS parameter while attempting to OPEN file
!    M63, Illegal value for DISPOSITION parameter while attempting to CLOSE file
!    M64, Illegal value for RENAME parameter while attempting to CLOSE file
!    M65, Illegal value for VOLUME label
!    M66, Illegal value for DENSITY parameter
!    M67, Illegal value for ACCESS parameter
!    M68, Illegal value for MOUNT parameter
!    M69, Attempted tape I/O while no tape mounted
!    M70, Illegal value for BLOCKSIZE parameter
!    M71, Attempt to read data block larger than buffer size
!    M72, Illegal value for recordsize parameter
!    M73, Invalid usage of _devicekeyword_ NEWFILE
!    M74, Illegal value for TRANSLATION parameter
!    M75, String length exceeds implementation's limit
!    M76, TCP socket state incorrect for CONNECT or LISTEN
!    M77, TCP _deviceattribute_ missing
!    M78, TCP _devicekeyword_ missing
!    M79, TCP socket allocated to another device
!    M80, Network error not otherwise specified
!    M81, Unable to establish network connection
!    M82, Network connection suspended: wait to resume
!    M83, Network connection lost
!    M84, Network protocol error: invalid client message
!    M85, Network protocol error: invalid server message
!    M86, Cannot relinquish device with I/O pending
!    M87, Network buffer overflow
!    M88, Non-existent _routine_
!    M89, Specified pattern is not a _subpattern_
!    M90, Invalid _namevalue_
!    M91, Routine source is not available
!    M92, Mathematical overflow
!    M93, Mathematical underflow
!    M94, Attempt to compute zero to the zero-eth power
!    M95, Exponentiation returns complex number with non-zero imaginary part
!    M96, Attempt to assign value to already valued write-once _ssvn_
!    M97, Routine associated with user-defined _ssvn_ does not exist
!    M98, Resource unavailable
!    M99, Invalid operation for context
!   M100, Output time-out expired
!   M101, Attempt to assign incorrect value to $ECODE
!   M102, Simultaneous synchronous and asynchronous event class
!   M103, Invalid event identifier
!   M104, IPC event identifier is not a valid job-number
!   M105, Object not currently accessible
!   M106, Object does not support requested method or property
!   M107, Object has no default value
!   M108, Value if not of data type OREF
!   M109, Undefined _devicekeyword_
!   M110, Event identifier not available
!   M111, Invalid number of days for date
!   M112, Invalid number of seconds for time
!
!  List of messages which are not reported by GT.M standard error routines(rts_error etc.) but
!  by PRINTFs. NOTE: if the value of any these messages changes in future, all its non-standard
!  usages should be reflected with the new value.
!	ERR_GTMDISTUNDEF	150377714 (reported in gtm.c if $gtm_dist is not defined)
!	ERR_DISTPATHMAX		150377682 (reported in gtm.c if $gtm_dist buffer is insufficient)
!	ERR_DLLNOOPEN		150379250 (reported in gtm.c if libgtmshr cannot be opened)
!	ERR_DLLNORTN		150379258 (reported in gtm.c if gtm_main is not found in libgtmshr)
!	ERR_OPCOMMISSED		150381275 (sent via $SNDOPR in util_output.c if prior errors)
!
!  List of known undocumented messages follows (along with a comment)
!	ERR_ACK			internal error (not displayed to the users)
!	ERR_ASC2EBCDICCONV	not yet documented since zOS is not officially supported
!	ERR_DEFEREVENT		referenced only if #define DEBUG is TRUE (i.e. for debug builds only)
!	ERR_ENQ			internal error (not displayed to the users)
!	ERR_FREEZEID		referenced only if #define DEBUG_FREEZE is TRUE.
!	ERR_INVDBGLVL		referenced only if "gtmdbglvl" is set to non-zero value (which is a debugging feature)
!	ERR_JNLREQUIRED		message referenced only in GT.CX code (not supported currently)
!	ERR_JNLWRTNOWWRTR	internal error (not displayed to the users)
!	ERR_JOBINTRRETRHOW	internal error (not displayed to users - drives rethrow of jobinterrupt)
!	ERR_JOBINTRRQST		internal error (not displayed to users - drives jobinterrupt)
!	ERR_LKSECINIT		message referenced only in GT.CX code (not supported currently)
!	ERR_MUDESTROYFAIL	referenced only if #define IPCRM_FOR_SANCHEZ_ONLY is TRUE.
!	ERR_MUDESTROYSUC	referenced only if #define IPCRM_FOR_SANCHEZ_ONLY is TRUE.
!	ERR_RBWRNNOTCHG		not seen by user since return status of mupip_set_file (that triggers this) is not displayed.
!	ERR_REPEATERROR		internal error (not displayed to the users)
!	ERR_SYSTEMVALUE		code is not executed (set $system won't reach op_svput at all to signal the error)
!	ERR_TPRETRY		internal error (not displayed to the users)
!	ERR_WILLEXPIRE		error triggered by the license management code which has been since disabled.
!	ERR_YDIRTSZ		used by a percent utility, which is not documented intentionally
!	ERR_ZDEFACTIVE		functionality not documented (GT.CM related weirdness)
!	ERR_ZDEFOFLOW		functionality not documented (GT.CM related weirdness)
!
!
! ----- Buffer to introduce new undocumented error messages without affecting UNUSEDMSGnnn match with corresponding line numbers.
!
!
!
!
!
!  In addition all messages of the LMU and GTLP message facility are not documented as Licensing is out of GT.M since V4.2.
!
ACK		<>/success/fao=0!/ansi=0
BREAKZST	<Break instruction encountered during ZSTEP action>/info/fao=0!/ansi=0
BADACCMTHD	<Invalid access method was specified, file not created>/info/fao=0!/ansi=0
BADJPIPARAM	<!AD is not a legal parameter for $ZGETJPI()>/error/fao=2!/ansi=0
BADSYIPARAM	<!AD is not a legal parameter for $ZGETSYI()>/error/fao=2!/ansi=0
BITMAPSBAD	<Database bit maps are incorrect>/error/fao=0!/ansi=0
BREAK		<Break instruction encountered>/info/fao=0!/ansi=0
BREAKDEA	<Break instruction encountered during Device error action>/info/fao=0!/ansi=0
BREAKZBA	<Break instruction encountered during ZBREAK action>/info/fao=0!/ansi=0
STATCNT		<!AD:!_  Key cnt: !UL  max subsc len: !UL  max data len: !UL>/info/fao=5!/ansi=0
BTFAIL		<The database block table is corrupt; error type !UL>/error/fao=1!/ansi=0
MUPRECFLLCK	<Database file !AD is locked by MUPIP RECOVER.  Could not secure access.>/error/fao=2!/ansi=0
CMD		<Command expected but not found>/error/fao=0!/ansi=0
COLON		<Colon (:) expected in this context>/error/fao=0!/ansi=0
COMMA		<Comma expected in this context>/error/fao=0!/ansi=0
COMMAORRPAREXP	<Comma or right parenthesis expected but not found>/error/fao=0!/ansi=0
COMMENT		<Comment line.  Placed zbreak at next executable line.>/info/fao=0!/ansi=0
CTRAP		<Character trap $C(!UL) encountered>/error/fao=1!/ansi=0
CTRLC		<CTRL_C encountered>/info/fao=0!/ansi=0
CTRLY		<User interrupt encountered>/info/fao=0!/ansi=0
DBCCERR		<Interlock instruction failure in critical mechanism for region !AD>/error/fao=2!/ansi=0
DUPTOKEN	<Token 0x!16@XQ is duplicate in the journal file !AD for database !AD>/error/fao=5!/ansi=0
DBJNLNOTMATCH	<Database !AD points to journal file name !AD but the journal file points to database file !AD>/error/fao=6!/ansi=0
DBFILERR	<Error with database file !AD>/error/fao=2!/ansi=0
DBNOTGDS	<!AD - Unrecognized database file format>/error/fao=2!/ansi=0
DBOPNERR	<Error opening database file !AD>/error/fao=2!/ansi=0
DBRDERR		<Cannot read database file !AD after opening>/error/fao=2!/ansi=0
CCEDUMPNOW	<>/fatal/fao=0!/ansi=0
DEVPARINAP	<Device parameter inappropriate to this command>/error/fao=0!/ansi=0
RECORDSTAT	<!AD:!_  Key cnt: !UL  max subsc len: !UL  max data len: !UL  max rec len: !UL>/info/fao=6!/ansi=0
NOTGBL		<!_!AD!/!_!_!_"^" Expected>/error/fao=2!/ansi=0
DEVPARPROT	<The protection specification is invalid>/error/fao=0!/ansi=0
PREMATEOF	<Premature end of file detected>/error/fao=0!/ansi=0
GVINVALID	<!_!AD!/!_!_!_Invalid global name>/error/fao=2!/ansi=0
DEVPARTOOBIG	<String deviceparameter exceeds 255 character limit>/error/fao=0!/ansi=0
DEVPARUNK	<Deviceparameter unknown>/error/fao=0!/ansi=0
DEVPARVALREQ	<A value is required for this device parameter>/error/fao=0!/ansi=0
DEVPARMNEG	<Deviceparameter must be a positive value>/error/fao=0!/ansi=0
DSEBLKRDFAIL	<Failed attempt to read block>/error/fao=0!/ansi=0
DSEFAIL		<DSE failed.  Failure code: !AD.>/error/fao=2!/ansi=0
NOTALLREPLON	<Replication state is not on for all regions>/warning/fao=0!/ansi=0
BADLKIPARAM	<!AD is not a legal parameter for $ZGETLKI()>/error/fao=2!/ansi=0
JNLREADBOF	<Beginning of journal file encountered for !AD>/error/fao=2!/ansi=0
DVIKEYBAD	<$ZGETDVI("!AD","!AD") contains an illegal keyword>/error/fao=4!/ansi=0
ENQ		<>/success/fao=0!/ansi=0
EQUAL		<Equal sign expected but not found>/error/fao=0!/ansi=0
ERRORSUMMARY	<Errors occurred during compilation>/error/fao=0!/ansi=0
ERRWEXC		<Error while processing exception string>/error/fao=0!/ansi=0
ERRWIOEXC	<Error while processing I/O exception string>/error/fao=0!/ansi=0
ERRWZBRK	<Error while processing ZBREAK action string>/error/fao=0!/ansi=0
ERRWZTRAP	<Error while processing $ZTRAP>/error/fao=0!/ansi=0
NUMUNXEOR	<!_!AD!/!_!_!_unexpected end of record in numeric subscript>/error/fao=2!/ansi=0
EXPR		<Expression expected but not found>/error/fao=0!/ansi=0
STRUNXEOR	<!_!AD!/!_!_!_unexpected end of record in string subscript>/error/fao=2!/ansi=0
JNLEXTEND	<Journal file extension error for file !AD>/error/fao=2!/ansi=0
FCHARMAXARGS	<Argument count of $CHAR function exceeded the maximum of 255>/error/fao=0!/ansi=0
FCNSVNEXPECTED	<Function or special variable expected in this context>/error/fao=0!/ansi=0
FNARGINC	<Format specifiers to $FNUMBER are incompatible: "!AD">/error/fao=2!/ansi=2
JNLACCESS	<Error accessing journal file !AD>/error/fao=2!/ansi=0
TRANSNOSTART	<ZTCOMMIT(s) issued without corresponding ZTSTART(s)>/error/fao=0!/ansi=44
FNUMARG		<$FNUMBER format specifier "!AD" contains an illegal character: "!AD">/error/fao=4!/ansi=0
FOROFLOW	<FOR commands nested more than !UL deep on a line>/error/fao=1!/ansi=0
YDIRTSZ		<Size of YDIRT data too large>/error/fao=0!/ansi=0
JNLSUCCESS	<!AD successful>/success/fao=2!/ansi=0
GBLNAME		<Either an identifier or a left parenthesis is expected after a ^ in this context>/error/fao=0!/ansi=29
GBLOFLOW	<Database segment is full>/error/fao=0!/ansi=0
CORRUPT		<Corrupt input in Blk # !UL, Key #!UL; resuming with next global block>/error/fao=2!/ansi=0
GTMCHECK	<Internal GT.M error--Report to your GT.M Support Channel>/fatal/fao=0!/ansi=0
GVDATAFAIL	<Global variable $DATA function failed.  Failure code: !AD.>/error/fao=2!/ansi=0
EORNOTFND	<!_!AD!/!_!_!_End of record not found>/error/fao=2!/ansi=0
GVGETFAIL	<Global variable retrieval failed.  Failure code: !AD.>/error/fao=2!/ansi=0
GVIS		<!_!_Global variable: !AD>/info/fao=2!/ansi=0
GVKILLFAIL	<Global variable kill failed.  Failure code: !AD.>/error/fao=2!/ansi=0
GVNAKED		<Illegal naked global reference>/error/fao=0!/ansi=1
GVNEXTARG	<Argument to global variable $NEXT must be subscripted>/error/fao=0!/ansi=0
GVORDERFAIL	<Global variable $ORDER or $NEXT function failed.  Failure code: !AD.>/error/fao=2!/ansi=0
GVPUTFAIL	<Global variable put failed.  Failure code: !AD.>/error/fao=2!/ansi=0
PATTABSYNTAX	<Error in !AD at line !UL>/error/fao=3!/ansi=0
GVSUBOFLOW	<Maximum combined length of subscripts exceeded>/error/fao=0!/ansi=0
GVUNDEF		<Global variable undefined: !AD>/error/fao=2!/ansi=7
TRANSNEST	<Maximum transaction nesting levels exceeded>/error/fao=0!/ansi=0
INDEXTRACHARS	<Indirection string contains extra trailing characters>/error/fao=0!/ansi=0
UNUSEDMSG260	<INDMAXNEST Last used in V6.0-000>/error/fao=0!/ansi=0
INDRMAXLEN	<Maximum length !UL of an indirection argument was exceeded>/error/fao=1!/ansi=0
INSFFBCNT	<Insufficient byte count quota left for requested operation>/error/fao=0!/ansi=0
INTEGERRS	<Database integrity errors>/error/fao=0!/ansi=0
INVCMD		<Invalid command keyword encountered>/warning/fao=0!/ansi=0
INVFCN		<Invalid function name>/error/fao=0!/ansi=0
INVOBJ		<Cannot ZLINK object file due to unexpected format>/error/fao=0!/ansi=0
INVSVN		<Invalid special variable name>/error/fao=0!/ansi=8
IOEOF		<Attempt to read past an end-of-file>/error/fao=0!/ansi=0
IONOTOPEN	<Attempt to USE an I/O device which has not been opened>/error/fao=0!/ansi=0
MUPIPINFO	<!AD>/info/fao=2!/ansi=0
IVTIME		<Invalid time specification: !AD>/error/fao=2!/ansi=0
JOBFAIL		<JOB command failure>/error/fao=0!/ansi=0
JOBLABOFF	<Label and offset not found in created process>/error/fao=0!/ansi=13
JOBPARNOVAL	<This job parameter cannot take a value>/error/fao=0!/ansi=0
JOBPARNUM	<The value of this job parameter must be an integer>/error/fao=0!/ansi=0
JOBPARSTR	<The value of this job parameter must be a string>/error/fao=0!/ansi=0
JOBPARUNK	<Job parameter unknown>/error/fao=0!/ansi=0
JOBPARVALREQ	<A value is required for this job parameter>/error/fao=0!/ansi=0
JUSTFRACT	<Fraction specifier to $JUSTIFY cannot be negative>/error/fao=0!/ansi=0
KEY2BIG		<Key size (!UL) is greater than maximum (!UL) for region: !AD>/error/fao=4!/ansi=0
LABELEXPECTED	<Label expected in this context>/error/fao=0!/ansi=0
LABELMISSING	<Label referenced but not defined: !AD>/error/fao=2!/ansi=13
LABELUNKNOWN	<Label referenced but not defined>/error/fao=0!/ansi=13
DIVZERO		<Attempt to divide by zero>/error/fao=0!/ansi=9
LKNAMEXPECTED	<An identifier is expected after a ^ in this context>/error/fao=0!/ansi=0
JNLRDERR	<Error reading journal file !AD.  Unable to initialize.>/error/fao=2!/ansi=0
LOADRUNNING	<Cannot ZLINK an active routine !AD>/error/fao=2!/ansi=25
LPARENMISSING	<Left parenthesis expected>/error/fao=0!/ansi=0
LSEXPECTED	<A line separator is expected here>/error/fao=0!/ansi=0
LVORDERARG	<Argument to local variable $NEXT must be subscripted>/error/fao=0!/ansi=0
MAXFORARGS	<Maximum number of arguments to a single FOR command exceeded>/error/fao=0!/ansi=0
TRANSMINUS	<Negative numbers not allowed with ZTCOMMIT>/error/fao=0!/ansi=0
MAXNRSUBSCRIPTS	<Maximum number of subscripts exceeded>/error/fao=0!/ansi=0
MAXSTRLEN	<Maximum string length exceeded>/error/fao=0!/ansi=75
JNLDBERR	<Journal file !AD does not correspond to database file !AD>/error/fao=4!/ansi=0
JNLFILOPN	<Error opening journal file !AD for database file !AD>/error/fao=4!/ansi=0
MBXRDONLY	<Mailbox is read only, cannot write to it>/error/fao=0!/ansi=0
JNLINVALID	<!AD is not a valid journal file !/ for database file: !AD>/error/fao=4!/ansi=0
MBXWRTONLY	<Mailbox is write only, cannot read from it>/error/fao=0!/ansi=0
MEMORY		<Central memory exhausted during request for !UJ bytes from 0x!XJ>/fatal/fao=2!/ansi=0
MTBLKTOOBIG	<Magtape BLOCK_SIZE exceeds maximum size allowed>/error/fao=0!/ansi=70
MTBLKTOOSM	<Magtape BLOCK_SIZE is less than !UL bytes>/error/fao=1!/ansi=70
MTFIXRECSZ	<BLOCK_SIZE !UL must be multiple of fixed record size !UL>/error/fao=2!/ansi=70
MTIS		<Magnetic tape: !AD>/info/fao=2!/ansi=0
MTRDBADBLK	<Block read too small, contained only !UL bytes, block size = !UL>/error/fao=2!/ansi=0
MTRDONLY	<Cannot write to a READONLY magtape>/error/fao=0!/ansi=62
MTRDTHENWRT	<Attempt to read after a write to a magtape>/error/fao=0!/ansi=0
MTRECGTRBLK	<Magtape record size cannot exceed block size>/error/fao=0!/ansi=71
MTRECTOOBIG	<Magtape record size exceeds maximum allowed>/error/fao=0!/ansi=72
MTRECTOOSM	<Magtape record size is too small for record type>/error/fao=0!/ansi=72
JNLTMQUAL3	<Time qualifier BEFORE_TIME="!AZ" is less than the journal file(s) minimum timestamp="!AZ">/error/fao=2!/ansi=0
MULTLAB		<This label has been previously defined>/error/fao=0!/ansi=57
BLKCNT		<Last LOAD Block/RMS Record number: !UL>/info/fao=1!/ansi=0
CCEDUMPOFF	<>/fatal/fao=0!/ansi=0
NOPLACE		<Line specified in a ZBREAK cannot be found>/error/fao=0!/ansi=0
JNLCLOSE	<Error closing journal file !AD>/error/fao=2!/ansi=0
NOTPRINCIO	<Output currently directed to device !AD>/warning/fao=2!/ansi=0
NOTTOEOFONPUT	<Not positioned to EOF on write (sequential organization only)>/error/fao=0!/ansi=0
NOZBRK		<No zbreak at that location>/info/fao=0!/ansi=0
NULSUBSC	<Null subscripts are not allowed for region: !AD>/error/fao=2!/ansi=0
NUMOFLOW	<Numeric overflow>/error/fao=0!/ansi=92
PARFILSPC	<Parameter: !AD  file specification: !AD>/error/fao=4!/ansi=0
PATCLASS	<Illegal character class for pattern code>/error/fao=0!/ansi=0
PATCODE		<Illegal syntax for pattern>/error/fao=0!/ansi=10
PATLIT		<Illegal character or unbalanced quotes for pattern literal>/error/fao=0!/ansi=0
PATMAXLEN	<Pattern code exceeds maximum length>/error/fao=0!/ansi=0
LPARENREQD	<!_!AD!/!_!_!_Left parenthesis expected>/error/fao=2!/ansi=0
PATUPPERLIM	<Pattern code upper limit is less than lower limit>/error/fao=0!/ansi=10
PCONDEXPECTED	<Post-conditional expression expected but not found>/error/fao=0!/ansi=0
PRCNAMLEN	<Process name !AD length is greater than !SL>/error/fao=3!/ansi=0
RANDARGNEG	<Random number generator argument must be greater than or equal to one>/error/fao=0!/ansi=3
DBPRIVERR	<No privilege for attempted update operation for file: !AD>/error/fao=2!/ansi=0
REC2BIG		<Record size (!UL) is greater than maximum (!UL) for region: !AD>/error/fao=4!/ansi=75
RHMISSING	<Right-hand side of expression expected>/error/fao=0!/ansi=0
DEVICEREADONLY	<Cannot write to read-only device>/error/fao=0!/ansi=62
COLLDATAEXISTS	<Collation type cannot be changed while data exists>/error/fao=0!/ansi=0
ROUTINEUNKNOWN	<Routine could not be found>/error/fao=0!/ansi=88
RPARENMISSING	<Right parenthesis expected>/error/fao=0!/ansi=0
RTNNAME		<Routine name expected here>/error/fao=0!/ansi=0
VIEWGVN		<Invalid global key name used with VIEW/$VIEW(): !AD>/error/fao=2!/ansi=0
RTSLOC		<!_!_At M source location !AD>/info/fao=2!/ansi=0
RWARG		<This is not a legal argument for a READ command>/error/fao=0!/ansi=0
RWFORMAT	<A valid format expression (!!, #, or ?expr) expected here>/error/fao=0!/ansi=0
JNLWRTDEFER	<Journal write start deferred>/info/fao=0!/ansi=0
SELECTFALSE	<No argument to $SELECT was true>/error/fao=0!/ansi=4
SPOREOL		<Either a space or an end-of-line was expected but not found>/error/fao=0!/ansi=0
SRCLIN		<!_!AD!/!_!AD>/info/fao=4!/ansi=0
SRCLOC		<!_!_At column !UL, line !UL, source module !AD>/info/fao=4!/ansi=0
SRCLOCUNKNOWN	<!_!_M source location unknown>/info/fao=0!/ansi=0
STACKCRIT	<Stack space critical>/error/fao=0!/ansi=0
STACKOFLOW	<Stack overflow>/fatal/fao=0!/ansi=0
STACKUNDERFLO	<Stack underflow>/error/fao=0!/ansi=0
STRINGOFLOW	<String pool overflow>/error/fao=0!/ansi=0
SVNOSET		<Cannot SET this special variable>/error/fao=0!/ansi=0
VIEWFN		<View parameter is not valid with $VIEW()>/error/fao=0!/ansi=0
TERMASTQUOTA	<Process AST quota exceeded, cannot open terminal>/error/fao=0!/ansi=0
TEXTARG		<Invalid argument to $TEXT function>/error/fao=0!/ansi=5
TMPSTOREMAX	<Maximum space for temporary values exceeded>/error/fao=0!/ansi=0
VIEWCMD		<View parameter is not valid with VIEW command>/error/fao=0!/ansi=0
JNI		<!AD>/error/fao=2!/ansi=0
TXTSRCFMT	<$TEXT encountered an invalid source program file format>/error/fao=0!/ansi=0
UIDMSG		<Unidentified message received>/error/fao=0!/ansi=0
UIDSND		<Unidentified sender PID>/error/fao=0!/ansi=0
UNDEF		<Undefined local variable: !AD>/error/fao=2!/ansi=6
UNIMPLOP	<Unimplemented construct encountered>/error/fao=0!/ansi=0
VAREXPECTED	<Variable expected in this context>/error/fao=0!/ansi=39
VARRECBLKSZ	<Blocksize must be at least record size + 4 bytes>/error/fao=0!/ansi=0
MAXARGCNT	<Maximum number of arguments !UL exceeded>/error/fao=1!/ansi=0
GTMSECSHRSEMGET	<semget error errno = !UL>/error/fao=1!/ansi=0
VIEWARGCNT	<View parameter !AD has inappropriate number of subparameters>/error/fao=2!/ansi=0
GTMSECSHRDMNSTARTED	<gtmsecshr daemon started (key: 0x!XL) for version !AD from !AD>/info/fao=5!/ansi=0
ZATTACHERR	<Error attaching to "!AD">/error/fao=2!/ansi=0
ZDATEFMT	<$ZDATE format string contains invalid character>/error/fao=0!/ansi=0
ZEDFILSPEC	<Illegal ZEDIT file specification: !AD> /error/fao=2!/ansi=0
ZFILENMTOOLONG	<!AD is longer than 255 characters>/error/fao=2!/ansi=75
ZFILKEYBAD	<!AD is not a legal keyword for $ZFILE()>/error/fao=2!/ansi=0
ZFILNMBAD	<!AD is not a legal file name>/error/fao=2!/ansi=0
ZGOTOLTZERO	<Cannot ZGOTO a level less than zero>/error/fao=0!/ansi=0
ZGOTOTOOBIG	<Cannot ZGOTO a level greater than present level>/error/fao=0!/ansi=0
ZLINKFILE	<Error while zlinking "!AD">/error/fao=2!/ansi=0
ZPARSETYPE	<Illegal TYPE argument to $ZPARSE(): !AD>/error/fao=2!/ansi=0
ZPARSFLDBAD	<Illegal $ZPARSE() field parameter: !AD>/error/fao=2!/ansi=0
ZPIDBADARG	<The tvexpr must be FALSE if last $ZPID() not found>/error/fao=0!/ansi=0
ZPRIVARGBAD	<!AD is not a legal privilege for $ZPRIV()>/error/fao=2!/ansi=0
ZPRIVSYNTAXERR	<Privilege string cannot end with a comma>/error/fao=0!/ansi=0
ZPRTLABNOTFND	<Label not found in routine>/error/fao=0!/ansi=13
VIEWAMBIG	<View parameter !AD is ambiguous>/error/fao=2!/ansi=0
VIEWNOTFOUND	<View parameter !AD not valid>/error/fao=2!/ansi=0
ZSETPRVARGBAD	<!AD is not a legal privilege for $ZSETPRIV()>/error/fao=2!/ansi=0
INVSPECREC	<Invalid global modifier record>/error/fao=0!/ansi=0
ZSETPRVSYNTAX	<$ZSETPRIV() privileges string cannot end with a comma>/error/fao=0!/ansi=0
ZSRCHSTRMCT	<Search stream identifier out of range>/error/fao=0!/ansi=0
VERSION		<Version mismatch - This program must be recompiled>/error/fao=0!/ansi=0
MUNOTALLSEC	<WARNING: not all global sections accessed were successfully rundown>/warning/fao=0!/ansi=0
MUSECDEL	<Section !AD deleted>/info/fao=2!/ansi=0
MUSECNOTDEL	<Section !AD not deleted>/info/fao=2!/ansi=0
RPARENREQD	<!_!AD!/!_!_!_Right parenthesis expected>/error/fao=2!/ansi=0
ZGBLDIRACC	<Cannot access global directory !AD!AD!AD.>/error/fao=6!/ansi=26
GVNAKEDEXTNM	<Cannot reference different global directory in a naked reference>/error/fao=0!/ansi=0
EXTGBLDEL	<Invalid delimiter for extended global syntax>/error/fao=0!/ansi=0
DSEWCINITCON	<No action taken, enter YES at CONFIRMATION prompt to initialize global buffers>/info/fao=0!/ansi=0
LASTFILCMPLD	<The file currently being compiled is !AD>/info/fao=2!/ansi=0
NOEXCNOZTRAP	<Neither an exception nor a Ztrap is specified>/info/fao=0!/ansi=0
UNSDCLASS	<Unsupported descriptor class>/error/fao=0!/ansi=0
UNSDDTYPE	<Unsupported descriptor data type>/error/fao=0!/ansi=0
ZCUNKTYPE	<External call: Unknown argument type>/error/fao=0!/ansi=0
ZCUNKMECH	<External call: Unknown parameter-passing mechanism>/error/fao=0!/ansi=0
ZCUNKQUAL	<External call: Unknown input qualifier>/error/fao=0!/ansi=0
JNLDBTNNOMATCH	<Journal file !AD has !AD transaction number [0x!16@XQ], but database !AD has current transaction number [0x!16@XQ] and journal end transaction number [0x!16@XQ]>/error/fao=9!/ansi=0
ZCALLTABLE	<External call Table format error>/error/fao=0!/ansi=0
ZCARGMSMTCH	<External call: Actual argument count, !UL, is greater than formal argument count, !UL>/error/fao=2!/ansi=58
ZCCONMSMTCH	<External call: Too many input arguments>/error/fao=0!/ansi=58
ZCOPT0		<External call: Qualifier OPTIONAL_0 can be used only with mechanisms REFERENCE or DESCRIPTOR>/error/fao=0!/ansi=0
ZCSTATUS	<External call: Unsuccessful return status>/error/fao=0!/ansi=0
ZCUSRRTN	<External call: Run-time error in user routine>/error/fao=0!/ansi=0
ZCPOSOVR	<External call: Invalid overlapping of arguments in table position !UL>/error/fao=1!/ansi=0
ZCINPUTREQ	<External call: Required input argument missing>/error/fao=0!/ansi=0
JNLTNOUTOFSEQ	<End transaction [0x!16@XQ] of journal !AD different from Begin transaction [0x!16@XQ] of next generation journal !AD>/error/fao=6!/ansi=0
ACTRANGE	<Alternate Collating Type !UL is out of range>/error/fao=1!/ansi=0
ZCCONVERT	<External call: error converting output argument>/error/fao=0!/ansi=0
ZCRTENOTF	<External call routine !AD not found>/error/fao=2!/ansi=0
GVRUNDOWN	<Error during global database rundown>/error/fao=0!/ansi=0
LKRUNDOWN	<Error during lock database rundown>/error/fao=0!/ansi=0
IORUNDOWN	<Error during image rundown>/error/fao=0!/ansi=0
FILENOTFND	<File !AD not found>/error/fao=2!/ansi=0
MUFILRNDWNFL	<File !AD rundown failed>/error/fao=2!/ansi=0
JNLTMQUAL1	<Time qualifier BEFORE_TIME="!AZ" is less than SINCE_TIME="!AZ">/error/fao=2!/ansi=0
FORCEDHALT	<Image HALTed by MUPIP STOP>/fatal/fao=0!/ansi=0
LOADEOF		<Load error: EOF reached prior to BEGIN record !UL.  No records loaded.>/error/fao=1!/ansi=0
WILLEXPIRE	<This copy of GT.M will expire within one week>/info/fao=0!/ansi=0
LOADEDBG	<Load error: END smaller than BEGIN.  No records loaded.>/error/fao=0!/ansi=0
LABELONLY	<Routine !AD was compiled for label-only entry>/error/fao=2!/ansi=0
MUREORGFAIL	<MUPIP REORG failed.  Failure code: !AD.>/error/fao=2!/ansi=0
GVZPREVFAIL	<Global variable $ZPREVIOUS function failed.  Failure code: !AD.>/error/fao=2!/ansi=0
MULTFORMPARM	<This formal parameter is multiply defined>/error/fao=0!/ansi=21
QUITARGUSE	<Quit cannot take an argument in this context>/error/fao=0!/ansi=16
NAMEEXPECTED	<A local variable name is expected in this context>/error/fao=0!/ansi=0
FALLINTOFLST	<Fall-through to a label with formallist is not allowed>/error/fao=0!/ansi=11
NOTEXTRINSIC	<Quit does not return to an extrinsic function: argument not allowed>/error/fao=0!/ansi=16
GTMSECSHRREMSEMFAIL	<error removing semaphore errno = !UL>/error/fao=1!/ansi=0
FMLLSTMISSING	<The formal list is absent from a label called with an actual list: !AD>/error/fao=2!/ansi=20
ACTLSTTOOLONG	<More actual parameters than formal parameters: !AD>/error/fao=2!/ansi=58
ACTOFFSET	<Actuallist not allowed with offset>/error/fao=0!/ansi=0
MAXACTARG	<Maximum number of actual arguments exceeded>/error/fao=0!/ansi=0
GTMSECSHRREMSEM	<[client pid !UL] Semaphore (!UL) removed>/error/fao=2!/ansi=0
JNLTMQUAL2	<Time qualifier LOOKBACK_TIME="!AZ" is later than SINCE_TIME="!AZ">/error/fao=2!/ansi=0
GDINVALID	<Unrecognized Global Directory file format: !AD, expected label: !AD, found: !AD>/error/fao=6!/ansi=0
ASSERT		<Assert failed in !AD line !UL for expression (!AD)>/fatal/fao=5!/ansi=0
MUFILRNDWNSUC	<File !AD successfully rundown>/info/fao=2!/ansi=0
LOADEDSZ	<Load error: END too small.  No records loaded.>/error/fao=0!/ansi=0
QUITARGLST	<Quit cannot take a list of arguments>/error/fao=0!/ansi=16
QUITARGREQD	<Quit from an extrinsic must have an argument>/error/fao=0!/ansi=17
CRITRESET	<The critical section crash count for region !AD has been incremented>/error/fao=2!/ansi=0
UNKNOWNFOREX	<Process halted by a forced exit from a source other than MUPIP>/fatal/fao=0!/ansi=0
FSEXP		<File specification expected but not found>/error/fao=0!/ansi=0
WILDCARD	<Wild cards are prohibited: !AD>/error/fao=2!/ansi=0
DIRONLY		<Directories only are allowed in file specs: !AD>/error/fao=2!/ansi=0
FILEPARSE	<Error parsing file specification: !AD>/error/fao=2!/ansi=0
QUALEXP		<Qualifier expected but not found>/error/fao=0!/ansi=0
BADQUAL		<Unrecognized qualifier: !AD>/error/fao=2!/ansi=0
QUALVAL		<Qualifier value required but not found>/error/fao=0!/ansi=0
ZROSYNTAX	<$ZROUTINES syntax error: !AD>/error/fao=2!/ansi=0
COMPILEQUALS	<Error in compiler qualifiers: !AD>/error/fao=2!/ansi=0
ZLNOOBJECT	<No object module was produced>/error/fao=0!/ansi=0
ZLMODULE	<Object file name does not match module name: !AD>/error/fao=2!/ansi=0
DBBLEVMX	<!AD Block level higher than maximum>/info/fao=2!/ansi=0
DBBLEVMN	<!AD Block level less than zero>/info/fao=2!/ansi=0
DBBSIZMN	<!AD Block too small>/error/fao=2!/ansi=0
DBBSIZMX	<!AD Block larger than file block size>/error/fao=2!/ansi=0
DBRSIZMN	<!AD Physical record too small>/error/fao=2!/ansi=0
DBRSIZMX	<!AD Physical record too large>/error/fao=2!/ansi=0
DBCMPNZRO	<!AD First record of block has nonzero compression count>/error/fao=2!/ansi=0
DBSTARSIZ	<!AD Star record has wrong size> /info /fao=2!/ansi=0
DBSTARCMP	<!AD Star record has nonzero compression count> /info /fao=2!/ansi=0
DBCMPMX		<!AD Record compression count is too large> /info /fao=2!/ansi=0
DBKEYMX		<!AD Key too long>/error/fao=2!/ansi=0
DBKEYMN		<!AD Key too short>/error/fao=2!/ansi=0
DBCMPBAD	<!AD Compression count not maximal>/warning/fao=2!/ansi=0
DBKEYORD	<!AD Keys out of order>/error/fao=2!/ansi=0
DBPTRNOTPOS	<!AD Block pointer negative>/error/fao=2!/ansi=0
DBPTRMX		<!AD Block pointer larger than file maximum>/error/fao=2!/ansi=0
DBPTRMAP	<!AD Block pointer is a bit map block number> /info /fao=2!/ansi=0
IFBADPARM	<External Interface Bad Parameter>/error/fao=0!/ansi=0
IFNOTINIT	<External Interface must first call GTM$INIT or M routine>/error/fao=0!/ansi=0
GTMSECSHRSOCKET	<!AD - !UL : Error initializing gtmsecshr socket>/error/fao=3!/ansi=0
LOADBGSZ	<Load error: BEGIN too small.  No records loaded.>/error/fao=0!/ansi=0
LOADFMT		<Load error: invalid format type.  Must be ZWR, GO, BINARY, or GOQ.>/error/fao=0!/ansi=0
LOADFILERR	<Error with load file !AD>/error/fao=2!/ansi=0
NOREGION	<REGION not found: !AD>/error/fao=2!/ansi=0
PATLOAD		<Error loading pattern file !AD>/error/fao=2!/ansi=0
EXTRACTFILERR	<Error with extract file !AD>/error/fao=2!/ansi=0
FREEZE		<Region: !AD is already frozen>/info/fao=2!/ansi=0
NOSELECT	<None of the selected variables exist -- halting>/warning/fao=0!/ansi=0
EXTRFAIL	<Extract failed for the global ^!AD. MUPIP INTEG should be run.>/error/fao=2!/ansi=0
LDBINFMT	<Corrupt binary format header information>/error/fao=0!/ansi=0
NOPREVLINK	<Journal file !AD has a null previous link>/error/fao=2!/ansi=0
CCEDUMPON	<>/fatal/fao=0!/ansi=0
CCEDMPQUALREQ	<A qualifier (DB,[NO]ON, or NOW) is required with the DUMP command>/error/fao=0!/ansi=0
CCEDBDUMP	<Section !AD dumped>/info/fao=2!/ansi=0
CCEDBNODUMP	<Section !AD not dumped; status = >/info/fao=2!/ansi=0
CCPMBX		<Error accessing Cluster Control Program Mailbox>/error/fao=0!/ansi=0
REQRUNDOWN	<Error accessing database !AD.  Must be rundown on cluster node !AD.>/error/fao=4!/ansi=0
CCPINTQUE	<Interlock failure accessing Cluster Control Program queue>/error/fao=0!/ansi=0
CCPBADMSG	<Invalid message code received by Cluster Control Program>/error/fao=0!/ansi=0
CNOTONSYS	<Command is not supported by this operating system>/error/fao=0!/ansi=0
CCPNAME		<Error setting the Cluster Control Program process name>/fatal/fao=0!/ansi=0
CCPNOTFND	<The Cluster Control Program is not responding>/error/fao=0!/ansi=0
OPRCCPSTOP	<The Cluster Control Program has been halted by an operator stop request>/info/fao=0!/ansi=0
SELECTSYNTAX	<Argument to !AD clause is not valid>/fatal/fao=2!/ansi=0
LOADABORT	<Aborting load at record !UL>/error/fao=1!/ansi=0
FNOTONSYS	<Function or special variable is not supported by this operating system>/error/fao=0!/ansi=0
AMBISYIPARAM	<Parameter !AD is ambiguous to $ZGETSYI()>/error/fao=2!/ansi=0
PREVJNLNOEOF	<A previous generation journal file !AD does not have valid EOF>/error/fao=2!/ansi=0
LKSECINIT	<Error creating lock section for database !AD>/error/fao=2!/ansi=0
MTDOSLAB	<Tape label is not in valid DOS-11 format>/error/fao=0!/ansi=0
MTDOSFOR	<Use of DOS-11 labels requires stream format>/error/fao=0!/ansi=0
MTINVLAB	<Invalid label type specified in magtape OPEN>/error/fao=0!/ansi=0
TXTSRCMAT	<M object module and source file do not match>/error/fao=0!/ansi=0
CCENOGROUP	<CCE does not have GROUP privilege.  Information may be incomplete.>/warning/fao=0!/ansi=0
BADDBVER	<Incorrect database version: !AD>/error/fao=2!/ansi=0
LINKVERSION	<This image must be relinked with the current version of GT.M>/fatal/fao=0!/ansi=0
TOTALBLKMAX	<Extension exceeds maximum total blocks.  Not extending.>/error/fao=0!/ansi=0
LOADCTRLY	<User interrupt encountered during load.  Load halting.>/info/fao=0!/ansi=0
CLSTCONFLICT	<Cluster conflict opening database file !AD; could not secure access.  Already open on node !AD.>/error/fao=4!/ansi=0
SRCNAM		<in source module !AD>/info/fao=2!/ansi=0
LCKGONE		<Lock removed: !AD>/success/fao=2!/ansi=0
SUB2LONG	<Subscript invalid, too long>/error/fao=0!/ansi=0
EXTRACTCTRLY	<User interrupt encountered during extract -- halting>/info/fao=0!/ansi=0
CCENOWORLD	<CCE does not have WORLD privilege.  Information may be incomplete.>/warning/fao=0!/ansi=0
GVQUERYFAIL	<Global variable $QUERY function failed.  Failure code: !AD.>/error/fao=2!/ansi=0
LCKSCANCELLED	<Error on remote node holding locks or zallocates.  All locks and zallocates cancelled.>/error/fao=0!/ansi=0
INVNETFILNM	<Invalid file name following node designation in global directory>/error/fao=0!/ansi=0
NETDBOPNERR	<Error while attempting to open database across net>/error/fao=0!/ansi=0
BADSRVRNETMSG	<Invalid message received from GT.CM server>/error/fao=0!/ansi=0
BADGTMNETMSG	<Invalid message sent to GT.CM server, type: 0x!XL>/error/fao=1!/ansi=0
SERVERERR	<Severe error on server: !AD>/error/fao=2!/ansi=0
NETFAIL		<Failure of Net operation>/error/fao=0!/ansi=0
NETLCKFAIL	<Lock operation across Net failed>/error/fao=0!/ansi=0
TTINVFILTER	<Invalid FILTER argument>/info/fao=0!/ansi=0
MTANSILAB	<Tape label is not in valid ANSI format>/error/fao=0!/ansi=0
MTANSIFOR	<Use of ANSI labels does not allow stream format>/error/fao=0!/ansi=0
BADTRNPARAM	<!AD is not a legal parameter to $ZTRNLNM>/error/fao=2!/ansi=0
DSEONLYBGMM	<!AD is supported only for BG/MM access methods>/warning/fao=2!/ansi=0
DSEINVLCLUSFN	<Specified function is invalid for clustered databases>/warning/fao=0!/ansi=0
RDFLTOOSHORT	<Length specified for fixed length read less than or equal to zero>/error/fao=0!/ansi=18
TIMRBADVAL	<Bad value specified.  Timer not changed.>/info/fao=0!/ansi=0
CCENOSYSLCK	<CCE does not have SYSLCK privilege.  Information may be incomplete.>/warning/fao=0!/ansi=0
CCPGRP		<Error with the Cluster Control Program's group number>/fatal/fao=0!/ansi=0
UNSOLCNTERR	<An unsolicited error message has been received from the network>/error/fao=0!/ansi=0
BACKUPCTRL	<Control Y or control C encountered during backup, aborting backup>/info/fao=0!/ansi=0
NOCCPPID	<Cannot find CCP process id>/error/fao=0!/ansi=0
CCPJNLOPNERR	<Error opening journal file.  Database not opened.>/error/fao=0!/ansi=0
LCKSGONE	<Locks selected for deletion removed>/success/fao=0!/ansi=0
ZLKIDBADARG	<The tvexpr must be FALSE if last ZLKID not found>/error/fao=0!/ansi=0
DBFILOPERR	<Error doing database I/O to region !AD>/error/fao=2!/ansi=0
CCERDERR	<Error reading from database file !AD>/error/fao=2!/ansi=0
CCEDBCL		<Database file !AD is clustered>/info/fao=2!/ansi=0
CCEDBNTCL	<Database file !AD is not clustered>/info/fao=2!/ansi=0
CCEWRTERR	<Error writing to database file !AD>/error/fao=2!/ansi=0
CCEBADFN	<Filename error>/error/fao=0!/ansi=0
CCERDTIMOUT	<Read timeout, CCP has not responded to request>/error/fao=0!/ansi=0
CCPSIGCONT	<CCP non fatal error at pc 0x!XJ.  Continuing operation.>/info/fao=1!/ansi=0
CCEBGONLY	<Only BG databases can be clustered>/error/fao=0!/ansi=0
CCENOCCP	<The cluster control program is not running on this node>/info/fao=0!/ansi=0
CCECCPPID	<The cluster control program has PID 0x!XL>/info/fao=1!/ansi=0
CCECLSTPRCS	<!UL processes are accessing clustered database files>/info/fao=1!/ansi=0
ZSHOWBADFUNC	<An illegal function was specified for ZSHOW>/error/fao=0!/ansi=0
NOTALLJNLEN	<Journaling not enabled and on for all regions>/warning/fao=0!/ansi=0
ZSHOWGLOSMALL	<Global output variable is too small for ZSHOW output>/error/fao=0!/ansi=0
NOLBRSRC	<Object libraries cannot have SRC paths associated>/error/fao=0!/ansi=0
INVZSTEP	<Invalid ZSTEP qualifier>/error/fao=0!/ansi=0
ZSTEPARG	<ZSTEP argument expected>/error/fao=0!/ansi=0
INVSTRLEN	<Invalid string length !UL: max !UL>/error/fao=2!/ansi=0
RECCNT		<Last LOAD record number: !UL>/info/fao=1!/ansi=0
TEXT		<!AD>/info/fao=2!/ansi=0
ZWRSPONE	<Subscript patterns in ZWRITE are atomic; Invalid delimiter>/error/fao=0!/ansi=0
FILEDEL		<File !AD successfully deleted>/info/fao=2!/ansi=0
JNLBADLABEL	<Journal file !AD does not have a GT.M Journal File Label>/error/fao=2!/ansi=0
JNLREADEOF	<End of journal file encountered for !AD>/error/fao=2!/ansi=0
JNLRECFMT	<Journal file record format error encountered>/error/fao=0!/ansi=0
BLKTOODEEP	<Block level too deep>/error/fao=0!/ansi=0
NESTFORMP	<Formal parameter list cannot be combined with nested line>/error/fao=0!/ansi=0
BINHDR		<!AD!/!/Date: !AD!/Time: !AD!/Extract Region Characteristics!/!_Blk Size: !AD!/!_Rec Size: !AD!/!_Key Size: !AD!/!_Std Null Coll: !AD!/!AD!/>/info/fao=16!/ansi=0
GOQPREC		<Numeric precision in key error:  Blk #!UL, Key #!UL.  Record not loaded.>/info/fao=2!/ansi=0
LDGOQFMT	<Corrupt GOQ format header information!/>/error/fao=0!/ansi=0
BEGINST		<Beginning LOAD at record number: !UL>/info/fao=1!/ansi=0
INVMVXSZ	<Invalid block size for GOQ load format>/fatal/fao=0!/ansi=0
JNLWRTNOWWRTR	<Journal writer attempting another write>/error/fao=0!/ansi=0
GTMSECSHRSHMCONCPROC	<More than one process attached to Shared memory segment (!UL) not removed (!UL)>/error/fao=2!/ansi=0
JNLINVALLOC	<Journal file allocation !UL is not within the valid range of !UL to !UL.  Journal file not created.>/warning/fao=3!/ansi=0
JNLINVEXT	<Journal file extension !UL is greater than the maximum allowed size of !UL.  Journal file not created.>/warning/fao=2!/ansi=0
MUPCLIERR	<Action not taken due to CLI errors>/error/fao=0!/ansi=0
JNLTMQUAL4	<Time qualifier BEFORE_TIME="!AZ" is less than AFTER_TIME="!AZ">/error/fao=2!/ansi=0
GTMSECSHRREMSHM	<[client pid !UL] Shared memory segment (!UL) removed, nattch = !UL>/info/fao=3!/ansi=0
GTMSECSHRREMFILE	<[client pid !UL] File (!AD) removed>/info/fao=3!/ansi=0
MUNODBNAME	<A database name or the region qualifier must be specified>/error/fao=0!/ansi=0
FILECREATE	<!AD file !AD created>/info/fao=4!/ansi=0
FILENOTCREATE	<!AD file !AD not created>/info/fao=4!/ansi=0
JNLPROCSTUCK	<Journal file writes blocked by process !UL>/warning/fao=1!/ansi=0
INVGLOBALQUAL	<Error in GLOBAL qualifier : Parse error at offset !UL in !AD>/error/fao=3!/ansi=0
COLLARGLONG	<Collation sequence !UL does not contain routines for long strings>/error/fao=1!/ansi=0
NOPINI		<PINI journal record expected but not found in journal file !AD at offset [0x!XL]>/error/fao=3!/ansi=0
DBNOCRE		<Not all specified database files, or their associated journal files were created>/error/fao=0!/ansi=0
JNLSPACELOW	<Journal file !AD nearing maximum size, !UL blocks to go>/info/fao=3!/ansi=0
DBCOMMITCLNUP	<Pid !UL [0x!XL] handled error (code = !UL) during commit of !AZ transaction in database file !AD>/info/fao=6!/ansi=0
BFRQUALREQ	<The [NO]BEFORE qualifier is required for this command>/error/fao=0!/ansi=0
REQDVIEWPARM	<Required View parameter is missing>/error/fao=0!/ansi=0
COLLFNMISSING	<Routine !AD is not found for collation sequence !UL>/error/fao=3!/ansi=0
JNLACTINCMPLT	<Mupip journal action might be incomplete>/warning/fao=0!/ansi=0
NCTCOLLDIFF	<Source and destination for MERGE cannot have different numerical collation type>/error/fao=0!/ansi=0
DLRCUNXEOR	<!_!AD!/!_!_!_unexpected end of record in $CHAR()/$ZCHAR() subscript>/error/fao=2!/ansi=0
DLRCTOOBIG	<!_!AD!/!_!_!_!AD value cannot be greater than 255>/error/fao=4!/ansi=0
WCERRNOTCHG	<Not all specified database files were changed>/error/fao=0!/ansi=0
WCWRNNOTCHG	<Not all specified database files were changed>/warning/fao=0!/ansi=0
ZCWRONGDESC	<A string longer than 65535 is passed via 32-bit descriptor>/error/fao=0!/ansi=0
MUTNWARN	<Database file !AD has 0x!16@XQ more transactions to go before reaching the transaction number limit (0x!16@XQ). Renew database with MUPIP INTEG TN_RESET>/warning/fao=4!/ansi=0
GTMSECSHRUPDDBHDR	<[client pid !UL] database fileheader (!AD) updated !AD>/info/fao=5!/ansi=0
LCKSTIMOUT	<DAL timed lock request expired>/warning/fao=0!/ansi=0
CTLMNEMAXLEN	<The maximum length of a control mnemonic has been exceeded>/error/fao=0!/ansi=0
CTLMNEXPECTED	<Control mnemonic is expected in this context>/error/fao=0!/ansi=0
USRIOINIT	<User-defined device driver not successfully initialized>/error/fao=0!/ansi=0
CRITSEMFAIL	<Error with semaphores for region !AD>/error/fao=2!/ansi=0
TERMWRITE	<Error writing to terminal>/error/fao=0!/ansi=0
COLLTYPVERSION	<Collation type !UL, version !UL mismatch>/error/fao=2!/ansi=0
LVNULLSUBS	<Null subscripts not allowed in local variables>/error/fao=0!/ansi=0
GVREPLERR	<Error replicating global in region !AD>/error/fao=2!/ansi=0
MTIOERR		<I/O Error with magnetic tape device !AD>/error/fao=2!/ansi=0
RMWIDTHPOS	<File record size or width must be greater than zero>/error/fao=0!/ansi=72
OFFSETINV	<Entry point !AD+!SL not valid>/error/fao=3!/ansi=13
JOBPARTOOLONG	<Total parameter length is too long for job command>/error/fao=0!/ansi=0
JOBARGMISSING	<Missing job argument !UL - can't skip non-trailing arguments to a JOB command in OpenVMS editions>/error/fao=1!/ansi=0
RUNPARAMERR	<Error accessing parameter for run command>/error/fao=0!/ansi=0
FNNAMENEG	<Depth argument to $NAME cannot be negative>/error/fao=0!/ansi=0
ORDER2		<Invalid second argument to $ORDER.  Must be -1 or 1.>/error/fao=0!/ansi=0
MUNOUPGRD	<Database not upgraded because of preceding errors>/error/fao=0!/ansi=0
REORGCTRLY	<User interrupt encountered during database reorg -- halting>/info/fao=0!/ansi=0
TSTRTPARM	<Error parsing TSTART qualifier>/error/fao=0!/ansi=0
TRIGNAMENF	<Trigger name !AD not found with the current default global directory>/error/fao=2!/ansi=0
TRIGZBREAKREM	<ZBREAK in trigger !AD removed due to trigger being reloaded>/warning/fao=2!/ansi=0
TLVLZERO	<Transaction is not in progress>/error/fao=0!/ansi=44
TRESTNOT	<Cannot TRESTART, transaction is not restartable>/error/fao=0!/ansi=27
TPLOCK		<Cannot release lock(s) held prior to current TSTART>/error/fao=0!/ansi=41
TPQUIT		<Cannot QUIT out of a routine with an active transaction>/error/fao=0!/ansi=42
TPFAIL		<Transaction COMMIT failed.  Failure code: !AD.>/error/fao=2!/ansi=0
TPRETRY		<Restart transaction from non-concurrency DB failure>/error/fao=0!/ansi=0
TPTOODEEP	<$TLEVEL cannot exceed 127>/error/fao=0!/ansi=0
ZDEFACTIVE	<ZDEFER already active>/error/fao=0!/ansi=0
ZDEFOFLOW	<ZDEFER Buffer overflow to node !AD>/error/fao=2!/ansi=0
MUPRESTERR	<MUPIP restore aborted due to preceding errors>/error/fao=0!/ansi=0
MUBCKNODIR	<MUPIP backup aborted due to error in output directory>/error/fao=0!/ansi=0
TRANS2BIG	<Transaction exceeded available buffer space for region !AD>/error/fao=2!/ansi=0
INVBITLEN	<Invalid size of the bit string>/error/fao=0!/ansi=0
INVBITSTR	<Invalid bit string>/error/fao=0!/ansi=0
INVBITPOS	<Invalid position in the bit string>/error/fao=0!/ansi=0
PARNORMAL	<Parse successful>/success/fao=0!/ansi=0
PARBUFSM	<Parse buffer too small>/error/fao=0!/ansi=0
RMWIDTHTOOBIG	<File record size or width too big>/error/fao=0!/ansi=72
PATTABNOTFND	<Pattern table !AD not found>/error/fao=2!/ansi=0
OBJFILERR	<Error with object file I/O on file !AD>/error/fao=2!/ansi=0
SRCFILERR	<Error with source file I/O on file !AD>/error/fao=2!/ansi=0
NEGFRACPWR	<Invalid operation: fractional power of negative number>/error/fao=0!/ansi=95
MTNOSKIP	<SKIP operation not supported on this device>/error/fao=0!/ansi=0
CETOOMANY	<Too many compiler escape substitutions in a single statement>/error/fao=0!/ansi=0
CEUSRERROR	<Compiler escape user routine returned error code !UL>/error/fao=1!/ansi=0
CEBIGSKIP	<Compiler escape user routine skip count is too large>/error/fao=0!/ansi=0
CETOOLONG	<Compiler escape substitution exceeds maximum line size>/error/fao=0!/ansi=0
CENOINDIR	<Indirection type information not available for compiler escape feature>/error/fao=0!/ansi=0
COLLATIONUNDEF	<Collation type !UL is not defined>/error/fao=1!/ansi=0
RBWRNNOTCHG	<Not all specified database files were changed>/warning/fao=0!/ansi=0
GTMSECSHRSRVF	<!AD - !UL : Attempt to service request failed (retry = !UL)>/error/fao=4!/ansi=0
FREEZECTRL	<Control Y or control C encountered during attempt to freeze the database. Aborting freeze.>/info/fao=0!/ansi=0
JNLFLUSH	<Error flushing journal buffers to journal file !AD>/info/fao=2!/ansi=0
CCPSIGDMP	<CCP non fatal dump, continuing operation. Report to your GT.M Support Channel.>/info/fao=0!/ansi=0
NOPRINCIO	<Unable to write to principal device>/fatal/fao=0!/ansi=0
INVPORTSPEC	<Invalid port specification>/error/fao=0!/ansi=0
INVADDRSPEC	<Invalid IP address specification>/error/fao=0!/ansi=0
SOCKPARMREQ	<Socket device parameter is required for TCP open>/error/fao=0!/ansi=78
IPADDRREQ	<Active connection requires IP address>/error/fao=0!/ansi=0
SOCKWAIT	<Error waiting for socket connection>/error/fao=0!/ansi=80
SOCKACPT	<Error accepting socket connection>/error/fao=0!/ansi=81
SOCKINIT	<Error initializing TCP socket: (errno == !UL) !AD>/error/fao=3!/ansi=80
OPENCONN	<Error opening TCP connection>/error/fao=0!/ansi=81
DEVNOTIMP	<!AD device not implemented on in this environment>/error/fao=2!/ansi=0
JNLEXTR		<Error writing journal extract file: !AD>/error/fao=2!/ansi=0
DBREMOTE	<Database region !AD is remote; perform maintenance on the server node>/error/fao=2!/ansi=0
JNLREQUIRED	<Journaling is required for clustered operation with file !AD>/error/fao=2!/ansi=0
TPMIXUP		<!AZ transaction cannot be started within !AZ transaction>/error/fao=2!/ansi=0
HTOFLOW		<Hash table overflow: Failed to allocate !UL elements>/error/fao=1!/ansi=0
RMNOBIGRECORD	<File record size requires BIGRECORD parameter>/error/fao=0!/ansi=72
DBBMSIZE	<!AD Bit map has incorrect size> /info /fao=2!/ansi=0
DBBMBARE	<!AD Bit map does not protect itself> /info /fao=2!/ansi=0
DBBMINV		<!AD Bit map contains an invalid pattern> /info /fao=2!/ansi=0
DBBMMSTR	<!AD Bit map does not match master map> /info /fao=2!/ansi=0
DBROOTBURN	<!AD Root block has data level> /info /fao=2!/ansi=0
REPLSTATEERR	<Replication state cannot be changed to the specified value for database file !AD>/error/fao=2!/ansi=0
VMSMEMORY	<Central memory exhausted during request for !UL bytes from 0x!XJ - check page file quota and page file size>/fatal/fao=2!/ansi=0
DBDIRTSUBSC	<!AD Directory tree block contains non name-level entries> /info /fao=2!/ansi=0
TIMEROVFL	<Timer overflow; interval probably too large> /error /fao=0!/ansi=0
GTMASSERT	<!AD - Assert failed !AD line !UL>/fatal/fao=5!/ansi=0
DBFHEADERR4	<Database file !AD: control problem: !AD was 0x!XL expecting 0x!XL> /info /fao=6!/ansi=0
DBADDRANGE	<Database file !AD, element location 0x!XJ: blk = 0x!XL: control 0x!XJ was outside !AD range 0x!XJ to 0x!XJ> /info /fao=9!/ansi=0
DBQUELINK	<Database file !AD, element location 0x!XJ: blk = 0x!XL: control !AD queue problem: was 0x!XJ, expecting 0x!XJ> /info /fao=8!/ansi=0
DBCRERR		<Database file !AD, cr location 0x!XJ blk = 0x!XL error: !AD was 0x!XL, expecting 0x!XL -- called from module !AD at line !UL> /info /fao=11!/ansi=0
MUSTANDALONE	<Could not get exclusive access to !AD> /info /fao=2!/ansi=0
MUNOACTION	<MUPIP unable to perform requested action> /error /fao=0!/ansi=0
RMBIGSHARE	<File with BIGRECORD specified may only be shared if READONLY>/error/fao=0!/ansi=0
TPRESTART	<Database !AD; code: !AD; blk: 0x!XL in glbl: ^!AD; pvtmods: !UL, blkmods: !UL, blklvl: !UL, type: !UL, readset: !UL, writeset: !UL, local_tn: 0x!16@XQ> /info /fao=14!/ansi=0
SOCKWRITE	<Write to a TCP/IP socket failed> /error /fao=0!/ansi=0
DBCNTRLERR	<Database file !AD: control error suspected but not found> /info /fao=2!/ansi=0
NOTERMENV	<Environment variable TERM not set.  Assuming "unknown."> /info /fao=0!/ansi=0
NOTERMENTRY	<TERM = "!AD" has no "terminfo" entry.  Possible terminal handling problems.> /info /fao=2!/ansi=0
NOTERMINFODB	<No "terminfo" database.  Terminal handling problems likely.> /info /fao=0!/ansi=0
INVACCMETHOD	<Invalid access method> /error /fao=0!/ansi=0
JNLOPNERR	<Error opening journal file !AD!/  for database !AD>/error/fao=4!/ansi=0
JNLRECTYPE	<Journal record type does not match expected type>/error/fao=0!/ansi=0
JNLTRANSGTR	<Transaction number in journal is greater than in database>/error/fao=0!/ansi=0
JNLTRANSLSS	<Transaction number in journal is less than in database>/error/fao=0!/ansi=0
JNLWRERR	<Error writing journal file !AD.  Unable to update header.>/error/fao=2!/ansi=0
FILEIDMATCH	<Saved File ID does not match the current ID - the file appears to have been moved>/error/fao=0!/ansi=0
EXTSRCLIN	<!_!AD!/!_!AD>/info/fao=4!/ansi=0
EXTSRCLOC	<!_!_At column !UL, line !UL, source module !AD>/info/fao=4!/ansi=0
BIGNOACL	<Existing file found when BIGRECORD specified with UDF format but no GT.M ACE, perhaps lost during COPY>/error/fao=0!/ansi=0
ERRCALL		<Error called from !AD line !UL>/error/fao=3!/ansi=0
ZCCTENV		<Environmental variable for external package !AD not set>/error/fao=2!/ansi=0
ZCCTOPN		<Unable to open external call table: !AD>/error/fao=2!/ansi=0
ZCCTNULLF	<External call table contains no records: !AD>/error/fao=2!/ansi=0
ZCUNAVAIL	<Package, !AD unavailable>/error/fao=2!/ansi=0
ZCENTNAME	<No entry name found in external call table>/error/fao=0!/ansi=0
ZCCOLON		<Colon expected but not found>/error/fao=0!/ansi=0
ZCRTNTYP	<Unknown return type>/error/fao=0!/ansi=0
ZCRCALLNAME	<Routine name expected but not found>/error/fao=0!/ansi=0
ZCRPARMNAME	<Parameter name expected but not found>/error/fao=0!/ansi=0
ZCUNTYPE	<Unknown type encountered>/error/fao=0!/ansi=0
ZCMLTSTATUS	<Multiple entries of xc_status in a single entry in external call table>/error/fao=0!/ansi=0
ZCSTATUSRET	<External call returned error status>/error/fao=0!/ansi=0
ZCMAXPARAM	<Exceeded maximum number of external call parameters>/error/fao=0!/ansi=0
ZCCSQRBR	<Closing Square bracket expected>/error/fao=0!/ansi=0
ZCPREALLNUMEX	<Pre-allocation value should be a decimal number>/error/fao=0!/ansi=0
ZCPREALLVALPAR	<Pre-allocation allowed only for variables passed by reference>/error/fao=0!/ansi=0
VERMISMATCH	<Attempt to access !AD with version !AD, while already using !AD>/error/fao=6!/ansi=0
JNLCNTRL	<Journal control unsynchronized for !AD.>/error/fao=2!/ansi=0
TRIGNAMBAD	<Trigger initialization failed. Error while processing ^#t("!AD",!AD)>/error/fao=4!/ansi=0
BUFRDTIMEOUT	<Pid [0x!XL] timed out waiting for buffered read of blk [0x!XL] into cr [0x!XL] by process [0x!XL] to complete in database file !AD>/error/fao=6!/ansi=0
INVALIDRIP	<Invalid read-in-progress field in Cache Record.  Resetting and continuing.  Region: !AD.>/error/fao=2!/ansi=0
BLKSIZ512	<Block size !UL rounds to !UL>/info/fao=2!/ansi=0
MUTEXERR	<Mutual Exclusion subsystem failure>/error/fao=0!/ansi=0
JNLVSIZE	<Journal File !AD has incorrect virtual_filesize !UL.  Allocation : !UL, Extension : !UL, Filesize : !UL, File system block size : !UL>/error/fao=7!/ansi=0
MUTEXLCKALERT	<Mutual Exclusion subsystem ALERT - lock attempt threshold crossed for region !AD.  Process !UL is in crit cycle !UL.>/warning/fao=4!/ansi=0
MUTEXFRCDTERM	<Mutual Exclusion subsystem detected forced termination of process !UL.  Crit salvaged from region !AD.>/warning/fao=3!/ansi=0
GTMSECSHR	<!UL : Error during gtmsecshr operation>/error/fao=1!/ansi=0
GTMSECSHRSRVFID	<!AD: !UL - Attempt to service request failed.!/ client id: !UL, mesg type: !UL, mesg data: !UL>/warning/fao=6!/ansi=0
GTMSECSHRSRVFIL	<!AD: !UL - Attempt to service request failed.!/ client id: !UL, mesg type: !UL!/file: !AD>/warning/fao=7!/ansi=0
FREEBLKSLOW	<Only !UL free blocks left out of !UL total blocks for !AD>/warning/fao=4!/ansi=0
PROTNOTSUP	<Protocol !AD not supported>/error/fao=2!/ansi=0
DELIMSIZNA	<Delimiter size is not appropriate>/error/fao=0!/ansi=0
INVCTLMNE	<Invalid control mnemonics>/error/fao=0!/ansi=0
SOCKLISTEN	<Error listening on a socket>/error/fao=0!/ansi=0
LQLENGTHNA	<Listening queue length !UL not appropriate.  Must be between 1 and 5.>/error/fao=1!/ansi=0
ADDRTOOLONG	<Socket address !AD of length !UL is longer than the maximum permissible length !UL>/error/fao=4!/ansi=0
GTMSECSHRGETSEMFAIL	<error getting semaphore errno = !UL>/error/fao=1!/ansi=0
CPBEYALLOC	<Attempt to copy beyond the allocated buffer>/error/fao=0!/ansi=0
DBRDONLY	<Database file !AD read only>/error/fao=2!/ansi=0
DUPTN		<Duplicate transaction found [TN = 0x!16@XQ] at offset 0x!XL in journal file !AD>/warning/fao=4!/ansi=0
TRESTLOC	<Transaction start: !AD, Transaction failure: !AD>/error/fao=4!/ansi=0
REPLPOOLINST	<Error with replication pool (id = !UL) for instance file !AD>/error/fao=3!/ansi=0
ZCVECTORINDX	<Invalid Vector Index !UL>/warning/fao=1!/ansi=0
REPLNOTON	<Replication is not on for journal file !AD, rollback will not continue>/error/fao=2!/ansi=0
JNLMOVED	<Journal file appears to have been moved.  Journaling activity will not be done.>/error/fao=0!/ansi=0
EXTRFMT		<Extract error: invalid format type.  Must be ZWR, GO, or BINARY.>/info/fao=0!/ansi=0
CALLERID	<Routine !AD called from 0x!XJ>/info/fao=3!/ansi=0
KRNLKILL	<Process was terminated by SIGDANGER signal from the system -- System swap space is too low -- Report to System Administrator>/fatal/fao=0!/ansi=0
MEMORYRECURSIVE	<Memory Subsystem called recursively>/fatal/fao=0!/ansi=0
FREEZEID	<Cache !AD on !AD by freeze id 0x!XL with match 0x!XL from 0x!XJ>/info/fao=7!/ansi=0
BLKWRITERR	<Unable to queue disk write for block 0x!XL.  Will keep trying.>/info/fao=1!/ansi=0
STOPTIMEOUT	<Waited too long for stopped process to release.  Region: !AD.>/error/fao=2!/ansi=0
TRIGMODINTP	<Triggers for a given global cannot be both used and modified or removed in the same transaction>/error/fao=0!/ansi=0
BCKUPBUFLUSH	<Unable to flush buffer for online backup>/error/fao=0!/ansi=0
NOFORKCORE	<Unable to fork off process to create core.  Core creation postponed.>/warning/fao=0!/ansi=0
JNLREAD		<Error reading from journal file !AD at offset [0x!XL]>/error/fao=3!/ansi=0
JNLMINALIGN	<Journal Record Alignment !UL is less than the minimum value of !UL>/warning/fao=2!/ansi=0
UNUSEDMSG781	<JNLDSKALIGN : Last used in V4.3-000>/error/fao=0!/ansi=0
JNLPOOLSETUP	<Journal Pool setup error>/error/fao=0!/ansi=0
JNLSTATEOFF	<ROLLBACK or RECOVER BACKWARD cannot proceed as database file !AD does not have journaling ENABLED and ON>/error/fao=2!/ansi=0
RECVPOOLSETUP	<Receive Pool setup error>/error/fao=0!/ansi=0
REPLCOMM	<Replication subsystem communication failure>/error/fao=0!/ansi=0
NOREPLCTDREG	<Replication subsystem found no region replicated for !AD !AZ>/warning/fao=3!/ansi=0
REPLINFO	<!AD>/info/fao=2!/ansi=0
REPLWARN	<!AD>/warning/fao=2!/ansi=0
REPLERR		<!AD>/error/fao=2!/ansi=0
JNLNMBKNOTPRCD	<Journal file !AD does not match the current journal file !AD of database file !AD>/error/fao=6!/ansi=0
REPLFILIOERR	<Replication subsystem file I/O error !AD>/error/fao=2!/ansi=0
REPLBRKNTRANS	<Replication subsystem found seqno !16@XQ broken or missing in the journal files>/error/fao=1!/ansi=0
TTWIDTHTOOBIG	<Terminal WIDTH exceeds the maximum allowed limit>/error/fao=0!/ansi=0
REPLLOGOPN	<Replication subsystem could not open log file !AD: !AD.  Logging done to !AD.>/error/fao=6!/ansi=0
REPLFILTER	<Replication filter subsystem failure>/error/fao=0!/ansi=0
GBLMODFAIL	<Global variable Conflict Test failed.  Failure code: !AD.>/error/fao=2!/ansi=0
TTLENGTHTOOBIG	<Terminal LENGTH exceeds the maximum allowed limit>/error/fao=0!/ansi=0
TPTIMEOUT	<Transaction timeout>/error/fao=0!/ansi=0
DEFEREVENT	<Transfer table reset for event type !UL when set for type !UL>/info/fao=2!/ansi=0
JNLFILNOTCHG	<Journal file not changed>/error/fao=0!/ansi=0
EVENTLOGERR	<Error in event logging subsystem>/error/fao=0!/ansi=0
UPDATEFILEOPEN	<Update file open error>/error/fao=0!/ansi=0
JNLBADRECFMT	<Journal File Record Format Error encountered for file !AD at disk address 0x!XL>/error/fao=3!/ansi=0
NULLCOLLDIFF	<Null collation order must be the same for all regions> /error/fao=0!/ansi=0
MUKILLIP	<Kill in progress indicator is set for file !AD - this !AD operation is likely to result in incorrectly marked busy errors>/warning/fao=4!/ansi=0
JNLRDONLY	<Journal file !AD read only>/error/fao=2!/ansi=0
ANCOMPTINC	<Deviceparameter !AD is not compatible with any other deviceparameters in the !AD command>/error/fao=4!/ansi=0
ABNCOMPTINC	<Deviceparameter !AD and deviceparameter !AD are not compatible in the !AD command>/error/fao=6!/ansi=0
UNUSEDMSG809	<GTMSECSHRLOGF last used in V5.5-000>/error/fao=0!/ansi=0
SOCKNOTFND	<Socket !AD not found>/error/fao=2!/ansi=0
CURRSOCKOFR	<Current socket of index !UL is out of range.  There are only !UL sockets.>/error/fao=2!/ansi=0
SOCKETEXIST	<Socket !AD already exists>/error/fao=2!/ansi=79
LISTENPASSBND	<Controlmnemonic LISTEN can be applied to PASSIVE socket in the state BOUND only>/error/fao=0!/ansi=76
DBCLNUPINFO	<Database file !AD !/!AD>/info/fao=4!/ansi=0
MUNODWNGRD	<Database not downgraded because of preceding errors>/error/fao=0!/ansi=0
REPLTRANS2BIG	<Transaction !16@XQ of size !UL (pre-filter size !UL) too large to be accommodated in the !AD pool>/error/fao=5!/ansi=0
RDFLTOOLONG	<Length specified for fixed length read exceeds the maximum string size>/error/fao=0!/ansi=0
MUNOFINISH	<MUPIP unable to finish all requested actions>/error/fao=0!/ansi=0
DBFILEXT	<Database file !AD extended from 0x!XL blocks to 0x!XL at transaction 0x!16@XQ>/info/fao=5!/ansi=0
JNLFSYNCERR	<Error synchronizing journal file !AD to disk>/error/fao=2!/ansi=0
FSYNCTIMOUT	<Timed out on fsync for journal file !AD>/error/fao=2!/ansi=0
ZCPREALLVALINV	<The pre-allocation value exceeded the maximum string length>/error/fao=0!/ansi=0
NEWJNLFILECREAT	<Journal file !AD nearing maximum size.  New journal file created.>/info/fao=2!/ansi=0
DSKSPACEFLOW	<Disk Space for file !AD nearing maximum size.  !UL blocks available.>/info/fao=3!/ansi=0
GVINCRFAIL	<Global variable $INCR failed.  Failure code: !AD.>/error/fao=2!/ansi=0
ISOLATIONSTSCHN	<Error changing NOISOLATION status for global ^!AD within a TP transaction from !UL to !UL>/error/fao=4!/ansi=0
REPLGBL2LONG	<Global names longer than 8 characters cannot be handled at the secondary>/error/fao=0!/ansi=0
TRACEON		<Missing global name (with optional subscripts) for recording M-tracing information>/error/fao=0!/ansi=0
TOOMANYCLIENTS	<GT.CM is serving the maximum number of clients.  Try again later.>/error/fao=0!/ansi=0
NOEXCLUDE	<None of the excluded variables exist>/info/fao=0!/ansi=0
GVINCRISOLATION	<$INCREMENT cannot be performed on global ^!AD as it has NOISOLATION turned ON>/error/fao=2!/ansi=0
EXCLUDEREORG	<Global: !AD is present in the EXCLUDE option.  REORG will skip the global.>/warning/fao=2!/ansi=0
REORGINC	<Reorg was incomplete.  Not all globals were reorged.>/error/fao=0!/ansi=0
ASC2EBCDICCONV	<ASCII/EBCDIC conversion failed when calling !AD>/error/fao=2!/ansi=0
GTMSECSHRSTART	<!AD - !UL : gtmsecshr failed to startup>/error/fao=3!/ansi=0
DBVERPERFWARN1	<Performance warning: Database !AD is running in compatibility mode which degrades performance. Run MUPIP REORG UPGRADE for best overall performance>/warning/fao=2!/ansi=0
FILEIDGBLSEC	<File ID in global section does not match with the database file !AD>/error/fao=2!/ansi=0
GBLSECNOTGDS	<Global Section !AD is not a GT.M global section>/error/fao=2!/ansi=0
BADGBLSECVER	<Global Section !AD does not match the current database version>/error/fao=2!/ansi=0
RECSIZENOTEVEN	<RECORDSIZE [!UL] needs to be a multiple of 2 if ICHSET or OCHSET is UTF-16, UTF-16LE or UTF-16BE>/error/fao=1!/ansi=0
BUFFLUFAILED	<Error flushing buffers from !AD for database file !AD>/error/fao=4!/ansi=0
MUQUALINCOMP	<Incompatible qualifiers - FILE and REGION>/error/fao=0!/ansi=0
DISTPATHMAX	<$gtm_dist path is greater than maximum (!UL)>/error/fao=1!/ansi=0
UNUSEDMSG844	<MAXTRACEHEIGHT last used in V5.4-002>/error/fao=0!/ansi=0
IMAGENAME	<The executing module name should be !AD instead of !AD>/error/fao=4!/ansi=0
GTMSECSHRPERM	<The gtmsecshr module in $gtm_dist does not have the correct permission and uid>/error/fao=0!/ansi=0
GTMDISTUNDEF	<Environment variable $gtm_dist is not defined>/error/fao=0!/ansi=0
SYSCALL		<Error received from system call !AD -- called from module !AD at line !UL>/error/fao=5!/ansi=0
MAXGTMPATH	<The executing module path is greater than the maximum !UL>/error/fao=1!/ansi=0
TROLLBK2DEEP	<Intended rollback(!SL) deeper than the current $tlevel(!UL)>/error/fao=2!/ansi=0
INVROLLBKLVL	<Rollback level (!UL) not less than current $TLEVEL(!UL).  Can't rollback.>/error/fao=2!/ansi=0
OLDBINEXTRACT	<Loading an older version(!UL) of binary extract. !/Database or global collation changes since the extract, if any, will result in database corruption.>/warning/fao=1!/ansi=0
ACOMPTBINC	<Deviceparameter !AD is compatible with only !AD in the command !AD>/error/fao=6!/ansi=0
NOTREPLICATED	<Transaction number !16@XQ generated by the !AD process (PID = !UL) is not replicated to the secondary>/warning/fao=4!/ansi=0
DBPREMATEOF	<Premature end of file with database file !AD>/error/fao=2!/ansi=0
KILLBYSIG	<!AD process !UL has been killed by a signal !UL>/fatal/fao=4!/ansi=0
KILLBYSIGUINFO	<!AD process !UL has been killed by a signal !UL from process !UL with userid number !UL>/fatal/fao=6!/ansi=0
KILLBYSIGSINFO1	<!AD process !UL has been killed by a signal !UL at address 0x!XJ (vaddr 0x!XJ)>/fatal/fao=6!/ansi=0
KILLBYSIGSINFO2	<!AD process !UL has been killed by a signal !UL at address 0x!XJ>/fatal/fao=5!/ansi=0
SIGILLOPC	<Signal was caused by an illegal opcode>/fatal/fao=0!/ansi=0
SIGILLOPN	<Signal was caused by an illegal operand>/fatal/fao=0!/ansi=0
SIGILLADR	<Signal was caused by illegal addressing mode>/fatal/fao=0!/ansi=0
SIGILLTRP	<Signal was caused by an illegal trap>/fatal/fao=0!/ansi=0
SIGPRVOPC	<Signal was caused by a privileged opcode>/fatal/fao=0!/ansi=0
SIGPRVREG	<Signal was caused by a privileged register>/fatal/fao=0!/ansi=0
SIGCOPROC	<Signal was caused by a coprocessor error>/fatal/fao=0!/ansi=0
SIGBADSTK	<Signal was caused by an internal stack error>/fatal/fao=0!/ansi=0
SIGADRALN	<Signal was caused by invalid address alignment>/fatal/fao=0!/ansi=0
SIGADRERR	<Signal was caused by a non-existent physical address>/fatal/fao=0!/ansi=0
SIGOBJERR	<Signal was caused by an object specific hardware error>/fatal/fao=0!/ansi=0
SIGINTDIV	<Signal was caused by an integer divided by zero>/fatal/fao=0!/ansi=0
SIGINTOVF	<Signal was caused by an integer overflow>/fatal/fao=0!/ansi=0
SIGFLTDIV	<Signal was caused by a floating point divide by zero>/fatal/fao=0!/ansi=0
SIGFLTOVF	<Signal was caused by a floating point overflow>/fatal/fao=0!/ansi=0
SIGFLTUND	<Signal was caused by a floating point underflow>/fatal/fao=0!/ansi=0
SIGFLTRES	<Signal was caused by a floating point inexact result>/fatal/fao=0!/ansi=0
SIGFLTINV	<Signal was caused by an invalid floating point operation>/fatal/fao=0!/ansi=0
SIGMAPERR	<Signal was caused by an address not mapped to an object>/fatal/fao=0!/ansi=0
SIGACCERR	<Signal was caused by invalid permissions for mapped object>/fatal/fao=0!/ansi=0
TRNLOGFAIL	<Translation of (VMS) logical name or (UNIX) environment variable !AD failed>/error/fao=2!/ansi=0
INVDBGLVL	<Invalid non-numeric debug level specified !AD in (VMS) logical name or (UNIX) environment variable !AD>/error/fao=4!/ansi=0
DBMAXNRSUBS	<!AD Maximum number of subscripts exceeded>/info/fao=2!/ansi=0
GTMSECSHRSCKSEL	<gtmsecshr select on socket failed>/error/fao=0!/ansi=0
GTMSECSHRTMOUT	<gtmsecshr exiting due to idle timeout>/info/fao=0!/ansi=0
GTMSECSHRRECVF	<gtmsecshr receive on server socket failed>/error/fao=0!/ansi=0
GTMSECSHRSENDF	<gtmsecshr send on server socket failed>/error/fao=0!/ansi=0
SIZENOTVALID8	<Size (in bytes) must be either 1, 2, 4, or 8>/error/fao=0!/ansi=0
GTMSECSHROPCMP	<gtmsecshr operation may be compromised>/warning/fao=0!/ansi=0
GTMSECSHRSUIDF	<gtmsecshr server setuid to root failed>/warning/fao=0!/ansi=0
GTMSECSHRSGIDF	<gtmsecshr server setgid to root failed>/warning/fao=0!/ansi=0
GTMSECSHRSSIDF	<gtmsecshr server setsid failed>/warning/fao=0!/ansi=0
GTMSECSHRFORKF	<gtmsecshr server unable to fork off a child process>/error/fao=0!/ansi=0
DBFSYNCERR	<Error synchronizing database file !AD to disk>/error/fao=2!/ansi=0
SECONDAHEAD	<Secondary ahead of primary. !/ Secondary database possibly updated by process other than the update process.  First perform rollback.>/error/fao=0!/ansi=0
SCNDDBNOUPD	<Database Updates not allowed on the secondary>/error/fao=0!/ansi=0
MUINFOUINT4	<!AD : !UL [0x!XL]>/info/fao=4!/ansi=0
NLMISMATCHCALC	<Location of !AD expected at 0x!XL, but found at 0x!XL>/error/fao=4!/ansi=0
UNUSEDMSG898	<GTMSECSHRLOGSWH last used in V5.5-000>/error/fao=0!/ansi=0
UNUSEDMSG899	<GTMSECSHRDEFLOG last used in V5.5-000>/info/fao=0!/ansi=0
DBBADNSUB	<!AD Bad numeric subscript>/error/fao=2!/ansi=0
DBBADKYNM	<!AD Bad key name>/error/fao=2!/ansi=0
DBBADPNTR	<!AD Bad pointer value in directory>/error/fao=2!/ansi=0
DBBNPNTR	<!AD Bit map block number as pointer>/error/fao=2!/ansi=0
DBINCLVL	<!AD Block at incorrect level>/error/fao=2!/ansi=0
DBBFSTAT	<!AD Block busy/free status unknown (local bitmap corrupted)>/error/fao=2!/ansi=0
DBBDBALLOC	<!AD Block doubly allocated>/error/fao=2!/ansi=0
DBMRKFREE	<!AD Block incorrectly marked free>/error/fao=2!/ansi=0
DBMRKBUSY	<!AD Block incorrectly marked busy>/warning/fao=2!/ansi=0
DBBSIZZRO	<!AD Block size equals zero>/error/fao=2!/ansi=0
DBSZGT64K	<!AD Block size is greater than 64K>/error/fao=2!/ansi=0
DBNOTMLTP	<!AD Block size not a multiple of 512 bytes>/error/fao=2!/ansi=0
DBTNTOOLG	<!AD Block transaction number too large>/info/fao=2!/ansi=0
DBBPLMLT512	<!AD Blocks per local map is less than 512>/error/fao=2!/ansi=0
DBBPLMGT2K	<!AD Blocks per local map is greater than 2K>/error/fao=2!/ansi=0
MUINFOUINT8	<!AD : !@ZQ [0x!16@XQ]>/info/fao=4!/ansi=0
DBBPLNOT512	<!AD Blocks per local map is not 512>/error/fao=2!/ansi=0
MUINFOSTR	<!AD : !AD>/info/fao=4!/ansi=0
DBUNDACCMT	<!AD Cannot determine access method; trying with BG>/info/fao=2!/ansi=0
DBTNNEQ		<!AD Current tn and early tn are not equal>/info/fao=2!/ansi=0
MUPGRDSUCC	<Database file !AD successfully !AD to !AD>/success/fao=6!/ansi=0
DBDSRDFMTCHNG	<Database file !AD, Desired DB Format set to !AD by !AD with pid !UL [0x!XL] at transaction number [0x!16@XQ]>/info/fao=9!/ansi=0
DBFGTBC		<!AD File size larger than block count would indicate>/warning/fao=2!/ansi=0
DBFSTBC		<!AD File size smaller than block count would indicate>/error/fao=2!/ansi=0
DBFSTHEAD	<!AD File smaller than database header>/error/fao=2!/ansi=0
DBCREINCOMP	<!AD Header indicates database file creation was interrupted before completion>/error/fao=2!/ansi=0
DBFLCORRP	<!AD Header indicates database file is corrupt>/error/fao=2!/ansi=0
DBHEADINV	<!AD Header size not valid for database>/error/fao=2!/ansi=0
DBINCRVER	<!AD Incorrect version of GT.M database>/error/fao=2!/ansi=0
DBINVGBL	<!AD Invalid mixing of global names>/error/fao=2!/ansi=0
DBKEYGTIND	<!AD Key greater than index key>/error/fao=2!/ansi=0
DBGTDBMAX	<!AD Key larger than database maximum>/error/fao=2!/ansi=0
DBKGTALLW	<!AD Key larger than maximum allowed length>/error/fao=2!/ansi=0
DBLTSIBL	<!AD Keys less than sibling's index key>/error/fao=2!/ansi=0
DBLRCINVSZ	<!AD Last record of block has invalid size>/error/fao=2!/ansi=0
MUREUPDWNGRDEND	<Region !AD : MUPIP REORG UPGRADE/DOWNGRADE finished by pid !UL [0x!XL] at transaction number [0x!16@XQ]>/info/fao=5!/ansi=0
DBLOCMBINC	<!AD Local bit map incorrect>/warning/fao=2!/ansi=0
DBLVLINC	<!AD Local bitmap block level incorrect>/warning/fao=2!/ansi=0
DBMBSIZMX	<!AD Map block too large>/warning/fao=2!/ansi=0
DBMBSIZMN	<!AD Map block too small>/error/fao=2!/ansi=0
DBMBTNSIZMX	<!AD Map block transaction number too large>/info/fao=2!/ansi=0
DBMBMINCFRE	<!AD Master bit map incorrectly asserts this local map has free space>/warning/fao=2!/ansi=0
DBMBPINCFL	<!AD Master bit map incorrectly marks this local map full>/warning/fao=2!/ansi=0
DBMBPFLDLBM	<!AD Master bit map shows this map full, agreeing with disk local map>/warning/fao=2!/ansi=0
DBMBPFLINT	<!AD Master bit map shows this map full, agreeing with MUPIP INTEG>/warning/fao=2!/ansi=0
DBMBPFLDIS	<!AD Master bit map shows this map full, in disagreement with both disk and INTEG result>/warning/fao=2!/ansi=0
DBMBPFRDLBM	<!AD Master bit map shows this map has space, agreeing with disk local map>/warning/fao=2!/ansi=0
DBMBPFRINT	<!AD Master bit map shows this map has space, agreeing with MUPIP INTEG>/warning/fao=2!/ansi=0
DBMAXKEYEXC	<!AD Maximum key size for database exceeds design maximum>/error/fao=2!/ansi=0
DBMXRSEXCMIN	<!AD Maximum record size for database is less than the design minimum>/error/fao=2!/ansi=0
UNUSEDMSG950	<DBMAXRSEXBL : Last used in V5.5-000>/error/fao=0!/ansi=0
DBREADBM	<!AD Read error on bitmap>/error/fao=2!/ansi=0
DBCOMPTOOLRG	<!AD Record has too large compression count>/error/fao=2!/ansi=0
DBVERPERFWARN2	<Peformance warning: Database !AD is not fully upgraded. Run MUPIP REORG UPGRADE for best overall performance>/warning/fao=2!/ansi=0
DBRBNTOOLRG	<!AD Root block number greater than last block number in file>/error/fao=2!/ansi=0
DBRBNLBMN	<!AD Root block number is a local bit map number>/error/fao=2!/ansi=0
DBRBNNEG	<!AD Root block number negative>/error/fao=2!/ansi=0
DBRLEVTOOHI	<!AD Root level higher than maximum>/error/fao=2!/ansi=0
DBRLEVLTONE	<!AD Root level less than one>/error/fao=2!/ansi=0
DBSVBNMIN	<!AD Start VBN smaller than possible>/error/fao=2!/ansi=0
DBTTLBLK0	<!AD Total blocks equal zero>/error/fao=2!/ansi=0
DBNOTDB		<!AD File does not have a valid GDS file header>/error/fao=2!/ansi=0
DBTOTBLK	<Total blocks should be 0x!XL, is 0x!XL>/error/fao=2!/ansi=0
DBTN		<Block TN is 0x!16@XQ>/info/fao=1!/ansi=0
DBNOREGION	<None of the database regions accessible>/error/fao=0!/ansi=0
DBTNRESETINC	<WARNING: tn_reset for database is incomplete due to integrity errors>/warning/fao=0!/ansi=0
DBTNLTCTN	<Transaction numbers greater than the current transaction were found>/error/fao=0!/ansi=0
DBTNRESET	<Cannot reset transaction number for this region>/error/fao=0!/ansi=0
MUTEXRSRCCLNUP	<Mutex subsystem leftover resource !AD removed>/info/fao=2!/ansi=0
SEMWT2LONG	<Process !UL waited !UL second(s) for the !AD lock for region !AD, lock held by pid !UL>/error/fao=7!/ansi=0
REPLINSTOPEN	<Error opening replication instance file !AD>/error/fao=2!/ansi=0
REPLINSTCLOSE	<Error closing replication instance file !AD>/error/fao=2!/ansi=0
UNUSEDMSG972	<JNLNOTFOUND : Last used in V4.4-000>/error/fao=0!/ansi=0
DBCRERR8	<Database file !AD, cr location 0x!XJ blk = 0x!XL error: !AD was 0x!16@XQ, expecting 0x!16@XQ -- called from module !AD at line !UL> /info /fao=11!/ansi=0
NUMPROCESSORS	<Could not determine number of processors>/warning/fao=0!/ansi=0
DBADDRANGE8	<Database file !AD, element location 0x!XJ: blk = 0x!XL: control 0x!16@XQ was outside !AD range 0x!16@XQ to 0x!16@XQ> /info /fao=9!/ansi=0
RNDWNSEMFAIL	<Attempting to acquire gds_rundown semaphore when it is already owned>/info/fao=0!/ansi=0
GTMSECSHRSHUTDN	<gtmsecshr process has received a shutdown request -- shutting down>/info/fao=0!/ansi=0
NOSPACECRE	<Not enough space to create database file !AD.  !@ZQ blocks are needed, only !@ZQ available.>/error/fao=4!/ansi=0
LOWSPACECRE	<Disk space for database file !AD is not enough for !UL future extensions.  !@ZQ !UL-byte blocks are needed, only !@ZQ available.>/warning/fao=6!/ansi=0
WAITDSKSPACE	<Process 0x!XL will wait !UL seconds for necessary disk space to become available for !AD >/info/fao=4!/ansi=0
OUTOFSPACE	<Database file !AD ran out of disk space.  Detected by process !UL.  !/Exit without clearing shared memory due to the disk space constraints.  !/Make space and then perform mupip rundown to ensure database integrity.>/fatal/fao=3!/ansi=0
JNLPVTINFO	<Pid 0x!XL!/     cycle 0x!XL fd_mismatch 0x!XL     channel 0x!XL sync_io 0x!XL!/ pini_addr 0x!XL  qio_active 0x!XL old_channel 0x!XL>/info/fao=8!/ansi=0
NOSPACEEXT	<Not enough disk space for file !AD to extend.  !UL blocks needed.  !UL blocks available.>/error/fao=4!/ansi=0
WCBLOCKED	<Field !AD is set by process !UL at transaction number 0x!16@XQ for database file !AD>/warning/fao=6!/ansi=0
REPLJNLCLOSED	<Replication in jeopardy as journaling got closed for database file !AD. Current region seqno is !@ZQ [0x!16@XQ] and system seqno is !@ZQ [0x!16@XQ]>/error/fao=6!/ansi=0
RENAMEFAIL	<Rename of file !AD to !AD failed>/warning/fao=4!/ansi=0
FILERENAME	<File !AD is renamed to !AD>/info/fao=4!/ansi=0
JNLBUFINFO	<Pid 0x!XL!/     dsk 0x!XL     free 0x!XL  bytcnt 0x!XL io_in_prog 0x!XL fsync_in_prog 0x!XL!/ dskaddr 0x!XL freeaddr 0x!XL  qiocnt 0x!XL now_writer 0x!XL     fsync_pid 0x!XL!/filesize 0x!XL    cycle 0x!XL  errcnt 0x!XL    wrtsize 0x!XL fsync_dskaddr 0x!XL>/info/fao=16!/ansi=0
UNUSEDMSG989	<JNLQIOLOCKED : Last used in V4.4-000>/error/fao=0!/ansi=0
UNUSEDMSG990	<JNLEOFPREZERO : Last used in V4.4-000>/error/fao=0!/ansi=0
TPNOTACID	<!AD at !AD in a final TP retry violates ACID properties of a TRANSACTION; indefinite RESTARTs may occur !AD !AD>/info/fao=8!/ansi=0
JNLSETDATA2LONG	<SET journal record has data of length !UL.  Target system cannot handle data more than !UL bytes.>/error/fao=2!/ansi=0
JNLNEWREC	<Target system cannot recognize journal record of type !UL, last recognized type is !UL>/error/fao=2!/ansi=0
REPLFTOKSEM	<Error with replication semaphores for instance file !AD>/error/fao=2!/ansi=0
UNUSEDMSG995	<GETCWD : Last used before V4.0-001E>/error/fao=0!/ansi=0
EXTRIOERR	<Error writing extract file !AD>/error/fao=2!/ansi=0
EXTRCLOSEERR	<Error closing extract file !AD>/error/fao=2!/ansi=0
UNUSEDMSG998	<TRUNCATE : Last used in V4.3-001F>/error/fao=0!/ansi=0
REPLEXITERR	<Replication process encountered an error while exiting>/error/fao=0!/ansi=0
MUDESTROYSUC	<Global section (!AD) corresponding to file !AD successfully destroyed>/info/fao=4!/ansi=0
DBRNDWN		<Error during global database rundown for region !AD.!/Notify those responsible for proper database operation.>/error/fao=2!/ansi=0
MUDESTROYFAIL	<Global section (!AD) corresponding to file !AD failed to be destroyed>/info/fao=4!/ansi=0
NOTALLDBOPN	<Not all required database files were opened>/fatal/fao=0!/ansi=0
MUSELFBKUP	<Database file !AD can not be backed upon itself>/error/fao=2!/ansi=0
DBDANGER	<Process !UL [0x!XL] killed while committing update for database file !AD.  Possibility of damage to block 0x!XL.>/warning/fao=5!/ansi=0
TRUNCATEFAIL	<Truncating !AD from 0x!XL VMS blocks to 0x!XL blocks failed>/error/fao=4!/ansi=0
TCGETATTR	<Error while getting terminal attributes on file descriptor !UL>/error/fao=1!/ansi=0
TCSETATTR	<Error while setting terminal attributes on file descriptor !UL>/error/fao=1!/ansi=0
IOWRITERR	<IO Write by pid 0x!XL to blk 0x!XL of database file !AD failed.  Pid 0x!XL retrying the IO.>/error/fao=5!/ansi=0
REPLINSTWRITE	<Error writing [0x!XL] bytes at offset [0x!16@XQ] in replication instance file !AD>/error/fao=4!/ansi=0
DBBADFREEBLKCTR	<Database !AD free blocks counter in file header: 0x!XL appears incorrect, should be 0x!XL.  Auto-corrected.>/warning/fao=4!/ansi=0
REQ2RESUME	<Request to resume suspended processing received from process !UL owned by userid !UL>/info/fao=2!/ansi=0
TIMERHANDLER	<Incorrect SIGALRM handler (0x!XJ) found by !AD>/warning/fao=3!/ansi=0
FREEMEMORY	<Error occurred freeing memory from 0x!XJ>/error/fao=1!/ansi=0
MUREPLSECDEL	<Replication section !AD deleted>/info/fao=2!/ansi=0
MUREPLSECNOTDEL	<Replication section !AD not deleted>/info/fao=2!/ansi=0
MUJPOOLRNDWNSUC	<Jnlpool section (id = !AD) belonging to the replication instance !AD successfully rundown>/info/fao=4!/ansi=0
MURPOOLRNDWNSUC	<Recvpool section (id = !AD) belonging to the replication instance !AD successfully rundown>/info/fao=4!/ansi=0
MUJPOOLRNDWNFL	<Jnlpool section (id = !AD) belonging to the replication instance !AD rundown failed>/error/fao=4!/ansi=0
MURPOOLRNDWNFL	<Recvpool section (id = !AD) belonging to the replication instance !AD rundown failed>/error/fao=4!/ansi=0
MUREPLPOOL	<Error with replpool section !AD>/info/fao=2!/ansi=0
REPLACCSEM	<Error with replication access semaphore (id = !UL) for instance file !AD>/error/fao=3!/ansi=0
JNLFLUSHNOPROG	<No progress while attempting to flush journal file !AD>/warning/fao=2!/ansi=0
REPLINSTCREATE	<Error creating replication instance file !AD>/error/fao=2!/ansi=0
SUSPENDING	<Process Received Signal !UL. Suspending processing on user request or attempt to do terminal I/O while running in the background>/info/fao=1!/ansi=0
SOCKBFNOTEMPTY	<Socket buffer size cannot be set to 0x!XL due to 0x!XL bytes of buffered data.  Read first.>/error/fao=2!/ansi=0
ILLESOCKBFSIZE	<The specified socket buffer size is 0x!XL, which is either 0 or too big>/error/fao=1!/ansi=0
NOSOCKETINDEV	<There is no socket in the current socket device>/error/fao=0!/ansi=0
SETSOCKOPTERR	<Setting the socket attribute !AD failed: (errno == !UL) !AD>/error/fao=5!/ansi=0
GETSOCKOPTERR	<Getting the socket attribute !AD failed: (errno == !UL) !AD>/error/fao=5!/ansi=0
NOSUCHPROC	<Process !UL does not exist - no need to !AD it>/info/fao=3!/ansi=0
DSENOFINISH	<DSE unable to finish all requested actions>/error/fao=0!/ansi=0
LKENOFINISH	<LKE unable to finish all requested actions>/error/fao=0!/ansi=0
NOCHLEFT	<Unhandled condition exception (all handlers exhausted) - process terminating>/fatal/fao=0!/ansi=0
MULOGNAMEDEF	<Logical name !AD, needed to start replication server is already defined for this job.  !/Check for an existing or improperly terminated server.>/error/fao=2!/ansi=0
BUFOWNERSTUCK	<Pid !UL waiting for Pid !UL to finish disk-read of block !UL [0x!XL].!/Been waiting for !UL minutes.  read_in_progress=!UL : rip_latch = !UL.>/error/fao=7!/ansi=0
ACTIVATEFAIL	<Failed to activate passive source server for secondary instance !AD>/error/fao=2!/ansi=0
DBRNDWNWRN	<Global section of database file !AD not rundown successfully by pid !UL [0x!XL].  Global section was not removed.>/warning/fao=4!/ansi=0
DLLNOOPEN	<Failed to load external dynamic library !AD>/error/fao=2!/ansi=0
DLLNORTN	<Failed to look up the location of the symbol !AD>/error/fao=2!/ansi=0
DLLNOCLOSE	<Failed to unload external dynamic library>/error/fao=0!/ansi=0
FILTERNOTALIVE	<Replication server detected that the filter is not alive while attempting to send seqno !16@XQ>/error/fao=1!/ansi=0
FILTERCOMM	<Error communicating seqno !16@XQ with the filter>/error/fao=1!/ansi=0
FILTERBADCONV	<Bad conversion of seqno !16@XQ by filter>/error/fao=1!/ansi=0
PRIMARYISROOT	<Attempted operation not valid on root primary instance !AD>/error/fao=2!/ansi=0
GVQUERYGETFAIL	<Global variable QUERY and GET failed.  Failure code: !AD.>/error/fao=2!/ansi=0
DBCREC2BIGINBLK	<A Record in block 0x!XL has a length greater than the maximum (!UL) in database !AD>/error/fao=4!/ansi=0
MERGEDESC	<Merge operation not possible.  !AD is descendent of !AD.>/error/fao=4!/ansi=19
MERGEINCOMPL	<Error encountered during MERGE; operation may be incomplete>/warning/fao=0!/ansi=0
DBNAMEMISMATCH	<Database file !AD points to shared memory (id = !UL) which in turn points to an inaccessible database file !AZ>/error/fao=4!/ansi=0
DBIDMISMATCH	<Database file !AZ (region !AD) id does not match file id in shared memory (id = !UL).>/error/fao=4!/ansi=0
DEVOPENFAIL	<Error opening !AD>/error/fao=2!/ansi=0
IPCNOTDEL	<!AD : !AD did not delete IPC resources for region !AD>/info/fao=6!/ansi=0
XCVOIDRET	<Attempt to return a value from function !AD, which is declared void in external call table !AD>/error/fao=4!/ansi=0
MURAIMGFAIL	<Mupip recover or rollback failed while processing an after-image journal record.  Failure code: !AD.>/error/fao=2!/ansi=0
REPLINSTUNDEF	<Replication instance environment variable $gtm_repl_instance is undefined>/error/fao=0!/ansi=0
REPLINSTACC	<Error accessing replication instance file !AD>/error/fao=2!/ansi=0
NOJNLPOOL	<No journal pool info found in the replication instance of !AD>/error/fao=2!/ansi=0
NORECVPOOL	<No receiver pool info found in the replication instance of !AD>/error/fao=2!/ansi=0
FTOKERR		<Error getting ftok of the file !AD>/error/fao=2!/ansi=0
REPLREQRUNDOWN	<Error accessing replication instance !AD.  Must be rundown on cluster node !AD.>/error/fao=4!/ansi=0
BLKCNTEDITFAIL	<Mupip recover or rollback failed to correct the block count field in the file header for file !AD>/info/fao=2!/ansi=0
SEMREMOVED	<Semaphore id !UL removed from the system>/info/fao=1!/ansi=0
REPLINSTFMT	<Format error encountered while reading replication instance file !AD. Expected !AD. Found !AD.>/error/fao=6!/ansi=0
SEMKEYINUSE	<Semaphore key 0x!XL is already in use (possibly by an older version)>/error/fao=1!/ansi=0
XTRNTRANSERR	<Error attempting to generate an environment using an external algorithm>/error/fao=0!/ansi=0
XTRNTRANSDLL	<Error during extended reference environment translation.  Check the above message.>/error/fao=0!/ansi=0
XTRNRETVAL	<Length of return value (!SL) from extended reference translation algorithm is not in the range [0,!UL]>/error/fao=2!/ansi=0
XTRNRETSTR	<Return string from extended reference translation algorithm is NULL>/error/fao=0!/ansi=0
INVECODEVAL	<Invalid value for $ECODE (!AD)>/error/fao=2!/ansi=101
SETECODE	<Non-empty value assigned to $ECODE (user-defined error trap)>/error/fao=0!/ansi=0
INVSTACODE	<Invalid value for second parameter of $STACK (!AD)>/error/fao=2!/ansi=0
REPEATERROR	<Repeat previous error>/error/fao=0!/ansi=0
NOCANONICNAME	<Value is not a canonic name (!AD)>/error/fao=2!/ansi=90
NOSUBSCRIPT	<No such subscript found (!SL)>/error/fao=1!/ansi=0
SYSTEMVALUE	<Invalid value for $SYSTEM (!AD)>/error/fao=2!/ansi=0
SIZENOTVALID4	<Size (in bytes) must be either 1, 2, or 4>/error/fao=0!/ansi=0
STRNOTVALID	<Error: cannot convert !AD value to valid value>/error/fao=2!/ansi=0
UNUSEDMSG1079	<RECNOCREJNL : Last used in V4.3-001F>/error/fao=0!/ansi=0
ERRWETRAP	<Error while processing $ETRAP>/error/fao=0!/ansi=0
TRACINGON	<Tracing already turned on>/info/fao=0!/ansi=0
CITABENV	<Environment variable for call-in table !AD not set>/error/fao=2!/ansi=0
CITABOPN	<Unable to open call-in table: !AD>/error/fao=2!/ansi=0
CIENTNAME	<No label reference found for this entry in call-in table>/error/fao=0!/ansi=0
CIRTNTYP	<Invalid return type>/error/fao=0!/ansi=0
CIRCALLNAME	<Call-in routine name expected but not found>/error/fao=0!/ansi=0
CIRPARMNAME	<Invalid parameter specification for call-in table>/error/fao=0!/ansi=0
CIDIRECTIVE	<Invalid directive parameter passing.  Expected I, O or IO.>/error/fao=0!/ansi=0
CIPARTYPE	<Invalid type specification for O/IO directive - expected pointer type>/error/fao=0!/ansi=0
CIUNTYPE	<Unknown parameter type encountered>/error/fao=0!/ansi=0
CINOENTRY	<No entry specified for !AD in the call-in table>/error/fao=2!/ansi=0
JNLINVSWITCHLMT	<Journal AUTOSWITCHLIMIT [!UL] falls outside of allowed limits [!UL] and [!UL]>/error/fao=3!/ansi=0
SETZDIR		<Cannot change working directory to !AD>/error/fao=2!/ansi=0
JOBACTREF	<Actual parameter in job command passed by reference>/error/fao=0!/ansi=40
ECLOSTMID	<$ECODE overflow, the first and last ecodes are retained, but some intervening ecodes have been lost>/warning/fao=0!/ansi=0
ZFF2MANY	<Number of characters specified for ZFF deviceparameter (!UL) is more than the maximum allowed (!UL)>/error/fao=2!/ansi=0
JNLFSYNCLSTCK	<Journaling fsync lock is stuck in journal file !AD>/warning/fao=2!/ansi=0
DELIMWIDTH	<Delimiter length !UL exceeds device width !UL>/error/fao=2!/ansi=0
DBBMLCORRUPT	<Database !AD : Bitmap blk [0x!XL] is corrupt (size = [0x!XL], levl = [0x!XL], tn = [0x!16@XQ]) : Dbtn = [0x!16@XQ] : Database integrity errors likely>/error/fao=7!/ansi=0
DLCKAVOIDANCE	<Possible deadlock detected: Database !AD : Dbtn [0x!16@XQ] : t_tries [0x!XL] : dollar_trestart [0x!XL] : now_crit [0x!XL] : TP transaction restarted>/error/fao=6!/ansi=0
WRITERSTUCK	<Buffer flush stuck waiting for [0x!XL] concurrent writers to finish writing to database file !AD>/error/fao=3!/ansi=0
PATNOTFOUND	<Current pattern table has no characters with pattern code !AD>/error/fao=2!/ansi=0
INVZDIRFORM	<Invalid value (!UL) specified for ZDIR_FORM>/error/fao=1!/ansi=0
ZDIROUTOFSYNC	<$ZDIRECTORY !AD is not the same as its cached value !AD>/warning/fao=4!/ansi=0
GBLNOEXIST	<Global !AD no longer exists>/info/fao=2!/ansi=0
MAXBTLEVEL	<Global !AD reached maximum level>/error/fao=2!/ansi=0
UNUSEDMSG1107	<JNLSTRESTFL : found no evidence it ever was used in a production release>/error/fao=0!/ansi=0
JNLALIGNSZCHG	<Journal ALIGNSIZE is rounded up to !UL blocks (closest next higher power of two)>/info/fao=1!/ansi=0
UNUSEDMSG1109	<MAXTRACELEVEL : last used in V5.4-002B>/error/fao=0!/ansi=0
GVFAILCORE	<A core file is being created for later analysis if necessary>/error/fao=0!/ansi=0
DBCDBNOCERTIFY	<Database !AD HAS NOT been certified due to the preceding errors - rerun DBCERTIFY SCAN>/error/fao=2!/ansi=0
DBFRZRESETSUC	<Freeze released successfully on database file !AD>/info/fao=2!/ansi=0
JNLFILEXTERR	<Error during extension of journal file !AD>/error/fao=2!/ansi=0
JOBEXAMDONE	<GT.M process !UL completed job examine to !AD>/info/fao=3!/ansi=0
JOBEXAMFAIL	<GT.M process !UL executing $ZJOBEXAM function failed with the preceding error message>/error/fao=1!/ansi=0
JOBINTRRQST	<Job interrupt requested>/error/fao=0!/ansi=0
ERRWZINTR	<Error while processing $ZINTERRUPT>/error/fao=0!/ansi=0
CLIERR		<!AD>/error/fao=2!/ansi=0
REPLNOBEFORE	<NOBEFORE option cannot be used when the current replication state is ON for database file !AD>/warning/fao=2!/ansi=0
REPLJNLCNFLCT	<Journaling cannot be turned !AD on database file !AD as the replication state is !AD and must also be turned !AD in the same command>/warning/fao=8!/ansi=0
JNLDISABLE	<Specified journal option(s) cannot take effect as journaling is DISABLED on database file !AD>/warning/fao=2!/ansi=0
FILEEXISTS	<File !AD already exists>/error/fao=2!/ansi=0
JNLSTATE	<Journaling state for !AD !AD is now !AD>/info/fao=6!/ansi=0
REPLSTATE	<Replication state for !AD !AD is now !AD>/info/fao=6!/ansi=0
JNLCREATE	<Journal file !AD created for !AD !AD with !AD>/info/fao=8!/ansi=0
JNLNOCREATE	<Journal file !AD not created>/error/fao=2!/ansi=0
JNLFNF		<Journal file !AD not found>/info/fao=2!/ansi=0
PREVJNLLINKCUT	<Previous journal file name link set to NULL in new journal file !AD created for database file !AD>/info/fao=4!/ansi=0
PREVJNLLINKSET	<Previous generation journal file name is changed from !AD to !AD>/info/fao=4!/ansi=0
FILENAMETOOLONG	<File name too long>/error/fao=0!/ansi=0
REQRECOV	<Error accessing database !AD.  Must be recovered on cluster node !AD.>/error/fao=4!/ansi=0
JNLTRANS2BIG	<Transaction needs an estimated [!UL blocks] in journal file !AD which exceeds the AUTOSWITCHLIMIT of !UL blocks>/error/fao=4!/ansi=0
JNLSWITCHTOOSM	<Journal AUTOSWITCHLIMIT [!UL blocks] is less than Journal ALLOCATION [!UL blocks] for database file !AD>/error/fao=4!/ansi=0
JNLSWITCHSZCHG	<Journal AUTOSWITCHLIMIT [!UL blocks] is rounded down to [!UL blocks] to equal the sum of Journal ALLOCATION [!UL blocks] and a multiple of Journal EXTENSION [!UL blocks] for database file !AD>/info/fao=6!/ansi=0
NOTRNDMACC	<Only random access files are supported as backup files for non-incremental backup>/error/fao=0!/ansi=0
TMPFILENOCRE	<Error in MUPIP BACKUP while trying to create temporary file !AD>/error/fao=2!/ansi=0
SHRMEMEXHAUSTED	<Attempt by process to use more shared memory than currently permitted by VMS>/error/fao=0!/ansi=0
JNLSENDOPER	<pid = 0x!XL : status = 0x!XL : jpc_status = 0x!XL : jpc_status2 = 0x!XL : iosb.cond = 0x!XW>/info/fao=5!/ansi=0
DDPSUBSNUL	<NUL characters in subscripts are not supported by DDP>/error/fao=0!/ansi=0
DDPNOCONNECT	<Named volume set, !AD, is not connected>/error/fao=2!/ansi=0
DDPCONGEST	<Agent congestion>/error/fao=0!/ansi=0
DDPSHUTDOWN	<Server has shut down>/error/fao=0!/ansi=0
DDPTOOMANYPROCS	<Maximum process limit of !UL exceeded>/error/fao=1!/ansi=0
DDPBADRESPONSE	<DDP invalid response code: !XB; message text follows>/error/fao=1!/ansi=0
DDPINVCKT	<Invalid format for CIRCUIT>/error/fao=0!/ansi=0
DDPVOLSETCONFIG	<Volume Set Configuration file error>/error/fao=0!/ansi=0
DDPCONFGOOD	<Volume Set Configuration entry accepted>/success/fao=0!/ansi=0
DDPCONFIGNORE	<Volume Set Configuration line ignored>/success/fao=0!/ansi=0
DDPCONFINCOMPL	<Volume Set Configuration entry incomplete>/warning/fao=0!/ansi=0
DDPCONFBADVOL	<Volume Set Configuration entry : invalid volume>/error/fao=0!/ansi=0
DDPCONFBADUCI	<Volume Set Configuration entry : invalid uci>/error/fao=0!/ansi=0
DDPCONFBADGLD	<Volume Set Configuration entry : invalid global directory>/error/fao=0!/ansi=0
DDPRECSIZNOTNUM	<Maximum record size is not numeric>/error/fao=0!/ansi=0
DDPOUTMSG2BIG	<DDP message too big to be accommodated in outbound buffer>/error/fao=0!/ansi=0
DDPNOSERVER	<DDP Server not running on local node>/error/fao=0!/ansi=0
MUTEXRELEASED	<Process !UL [0x!XL] has released the critical section for database !AD to avoid deadlock. $TLEVEL: !UL  t_tries: !UL>/error/fao=6!/ansi=0
JNLCRESTATUS	<!AD at line !UL for journal file !AD, database file !AD encountered error>/warning/fao=7!/ansi=0
ZBREAKFAIL	<Could not set breakpoint at !AD due to insufficient memory>/info/fao=2!/ansi=0
DLLVERSION	<Routine !AD in library !AD was compiled with an incompatible version of GT.M.  Recompile with the current version and re-link.>/error/fao=4!/ansi=0
INVZROENT	<!AD is neither a directory nor an object library(DLL)>/error/fao=2!/ansi=0
DDPLOGERR	<!AD: !AD>/error/fao=4!/ansi=0
GETSOCKNAMERR	<Getting the socket name failed from getsockname(): (errno==!UL) !AD>/error/fao=3!/ansi=0
INVGTMEXIT	<Inappropriate invocation of gtm_exit. gtm_exit cannot be invoked from external calls.>/error/fao=0!/ansi=0
CIMAXPARAM	<Exceeded maximum number of parameters in the call-in table entry. An M routine cannot accept more than 32 parameters.>/error/fao=0!/ansi=0
CITPNESTED	<Call-ins can not be used inside a TP transaction>/error/fao=0!/ansi=0
CIMAXLEVELS	<Too many nested Call-ins. Nested resources exhausted at level !UL.>/error/fao=1!/ansi=0
JOBINTRRETHROW	<Job interrupt redelivered>/error/fao=0!/ansi=0
STARFILE	<Star(*) argument cannot be specified with !AD>/error/fao=2!/ansi=0
NOSTARFILE	<Only star(*) argument can be specified with !AD>/error/fao=2!/ansi=0
MUJNLSTAT	<!AD at !AD>/info/fao=4!/ansi=0
JNLTPNEST	<Mupip journal command found nested TP transactions for journal file !AD at offset 0x!XL at transaction number 0x!16@XQ>/warning/fao=4!/ansi=0
REPLOFFJNLON	<Replication state for database file !AD is OFF but journaling state is enabled>/error/fao=2!/ansi=0
FILEDELFAIL	<Deletion of file !AD failed>/warning/fao=2!/ansi=0
INVQUALTIME	<Invalid time qualifier value.  Specify as !AD=delta_or_absolute_time.>/error/fao=2!/ansi=0
NOTPOSITIVE	<!AD qualifier must be given a value greater than zero>/error/fao=2!/ansi=0
INVREDIRQUAL	<Invalid REDIRECT qualifier value.  !AD>/error/fao=2!/ansi=0
INVERRORLIM	<Invalid ERROR_LIMIT qualifier value.  Must be at least zero>/error/fao=0!/ansi=0
INVIDQUAL	<Invalid ID qualifier value !AD>/error/fao=2!/ansi=0
INVTRNSQUAL	<Invalid TRANSACTION qualifier.  Specify only one of TRANSACTION=[NO]SET or TRANSACTION=[NO]KILL.>/error/fao=0!/ansi=0
JNLNOBIJBACK	<MUPIP JOURNAL BACKWARD cannot continue as journal file !AD does not have before image journaling>/error/fao=2!/ansi=0
SETREG2RESYNC	<Setting resync sequence number 0x!16@XQ to region sequence number 0x!16@XQ for database !AD>/info/fao=4!/ansi=0
JNLALIGNTOOSM	<Alignsize !UL (bytes) is too small for a block size of !UL (bytes) for !AD !AD.  Using alignsize of !UL (bytes) instead.>/warning/fao=7!/ansi=0
JNLFILEOPNERR	<Error opening journal file !AD>/error/fao=2!/ansi=0
JNLFILECLOSERR	<Error closing journal file !AD>/error/fao=2!/ansi=0
REPLSTATEOFF	<ROLLBACK cannot proceed as database !AD does not have replication ON>/error/fao=2!/ansi=0
MUJNLPREVGEN	<Previous generation journal file !AD included for database file !AD>/info/fao=4!/ansi=0
MUPJNLINTERRUPT	<Database file !AD indicates interrupted MUPIP JOURNAL command.  Restore from backup for forward recovery.>/error/fao=2!/ansi=0
ROLLBKINTERRUPT	<Database file !AD indicates interrupted ROLLBACK.  Reissue the MUPIP JOURNAL ROLLBACK command.>/error/fao=2!/ansi=0
RLBKJNSEQ	<Journal seqno of the instance after rollback is !@ZQ [0x!16@XQ]>/info/fao=2!/ansi=0
REPLRECFMT	<Replication journal record format error encountered>/fatal/fao=0!/ansi=0
PRIMARYNOTROOT	<Attempted operation not valid on non-root primary instance !AD>/error/fao=2!/ansi=0
DBFRZRESETFL	<Freeze release failed on database file !AD>/error/fao=2!/ansi=0
JNLCYCLE	<Journal file !AD causes cycle in the journal file generations of database file !AD>/error/fao=4!/ansi=0
JNLPREVRECOV	<Journal file has nonzero value in prev_recov_end_of_data field>/error/fao=0!/ansi=0
RESOLVESEQNO	<Resolving until sequence number !@ZQ [0x!16@XQ]>/info/fao=2!/ansi=0
BOVTNGTEOVTN	<Journal file !AD has beginning transaction [0x!16@XQ] which is greater than end transaction [0x!16@XQ]>/error/fao=4!/ansi=0
BOVTMGTEOVTM	<Journal file !AD has beginning timestamp [0x!16@XQ] greater than end timestamp [0x!16@XQ]>/error/fao=4!/ansi=0
BEGSEQGTENDSEQ	<Journal file !AD has beginning sequence number [0x!16@XQ] greater than end sequence number [0x!16@XQ]>/error/fao=4!/ansi=0
DBADDRALIGN	<Database file !AD, element location 0x!XJ: blk = 0x!XL: [!AD] control 0x!XJ was unaligned relative to base 0x!XJ and element size 0x!XL>/info/fao=9!/ansi=0
DBWCVERIFYSTART	<Database file !AD, write cache verification started by pid !UL [0x!XL] at transaction number 0x!16@XQ>/info/fao=5!/ansi=0
DBWCVERIFYEND	<Database file !AD, write cache verification finished by pid !UL [0x!XL] at transaction number 0x!16@XQ>/info/fao=5!/ansi=0
MUPIPSIG	<!AD (signal !UL) issued from process !UL [0x!XL] to process !UL [0x!XL]>/info/fao=7!/ansi=0
HTSHRINKFAIL	<Hash table compaction failed to allocate new smaller table due to lack of memory>/warning/fao=0!/ansi=0
STPEXPFAIL	<Stringpool expansion failed. It could not expand to !UL bytes.>/error/fao=1!/ansi=0
DBBTUWRNG	<The blocks-to-upgrade file-header field is incorrect. Expected 0x!XL, found 0x!XL>/warning/fao=2!/ansi=0
DBBTUFIXED	<The blocks-to-upgrade file-header field has been changed to the correct value>/info/fao=0!/ansi=0
DBMAXREC2BIG	<Maximum record size (!UL) is too large for this block size (!UL) - Maximum is !UL>/error/fao=3!/ansi=0
DBCSCNNOTCMPLT	<Specified DBCERTIFY SCAN output file is not complete - Rerun scan>/error/fao=0!/ansi=0
DBCBADFILE	<Source file !AD does not appear to have been generated by DBCERTIFY SCAN - rerun SCAN or specify correct file>/error/fao=2!/ansi=0
DBCNOEXTND	<Unable to extend database !AD>/error/fao=2!/ansi=0
DBCINTEGERR	<Encountered integrity error in database !AD>/error/fao=2!/ansi=0
DBMINRESBYTES	<Minimum RESERVED BYTES value required for certification/upgrade is !UL - Currently is !UL>/error/fao=2!/ansi=0
DBCNOTSAMEDB	<Database has been moved or restored since DBCERTIFY SCAN - Rerun scan>/error/fao=0!/ansi=0
DBCDBCERTIFIED	<Database !AD has been certified for use with !AD>/info/fao=4!/ansi=0
DBCMODBLK2BIG	<Block 0x!XL has been modified since DBCERTIFY SCAN but is still too large or now has an earlier TN than in the scan phase - Rerun scan phase>/error/fao=1!/ansi=0
DBCREC2BIG	<Record with key !AD is length !UL in block 0x!XL which is greater than the maximum length !UL in database !AD>/error/fao=7!/ansi=0
DBCCMDFAIL	<Executed command failed with return code !SL: !AD which executed !AD !AD>/error/fao=7!/ansi=0
DBCKILLIP	<Cannot proceed with kill in progress indicator set for database !AD -- Run MUPIP INTEG first>/error/fao=2!/ansi=0
DBCNOFINISH	<DBCERTIFY unable to finish all requested actions>/error/fao=0!/ansi=0
DYNUPGRDFAIL	<Unable to dynamically upgrade block 0x!XL in database !AD due to lack of free space in block>/error/fao=3!/ansi=0
MMNODYNDWNGRD	<Unable to use dynamic downgrade with MM access method for region !AD. Use BG access method for downgrade>/error/fao=2!/ansi=0
MMNODYNUPGRD	<Unable to use MM access method for region !AD until all database blocks are upgraded>/error/fao=2!/ansi=0
MUDWNGRDNRDY	<Database !AD is not ready to downgrade - still !UL database blocks to downgrade>/error/fao=3!/ansi=0
MUDWNGRDTN	<Transaction number 0x!16@XQ in database !AD is too big for MUPIP [REORG] DOWNGRADE. Renew database with MUPIP INTEG TN_RESET>/error/fao=3!/ansi=0
MUDWNGRDNOTPOS	<Start VBN value is [!UL] while downgraded GT.M version can support only [!UL]. Downgrade not possible>/error/fao=2!/ansi=0
MUUPGRDNRDY	<Database !AD has not been certified as being ready to upgrade to !AD format>/error/fao=4!/ansi=0
TNWARN		<Database file !AD has 0x!16@XQ more transactions to go before reaching the transaction number limit (0x!16@XQ). Renew database with MUPIP INTEG TN_RESET>/warning/fao=4!/ansi=0
TNTOOLARGE	<Database file !AD has reached the transaction number limit (0x!16@XQ). Renew database with MUPIP INTEG TN_RESET>/error/fao=3!/ansi=0
SHMPLRECOV	<Shared memory pool block recovery invoked for region !AD>/info/fao=2!/ansi=0
MUNOSTRMBKUP	<Database !AD has a block size larger than !UL and thus cannot use stream (incremental) backup>/warning/fao=3!/ansi=0
EPOCHTNHI	<At the EPOCH record at offset !UL of !AD transaction number [0x!16@XQ] is higher than database transaction number [0x!16@XQ]>/error/fao=5!/ansi=0
CHNGTPRSLVTM	<Mupip will change tp_resolve_time from !UL to !UL because expected EPOCH or EOF record was not found in Journal File !AD>/info/fao=4!/ansi=0
JNLUNXPCTERR	<Unexpected error encountered for Journal !AD at disk address 0x!XL>/error/fao=3!/ansi=0
OMISERVHANG	<GTCM OMI server is hung>/info/fao=0!/ansi=0
RSVDBYTE2HIGH	<Record size (!UL) is greater than the maximum allowed for region !AD with Block size (!UL) and Reserved bytes (!UL)>/error/fao=5!/ansi=0
BKUPTMPFILOPEN	<Open of backup temporary file !AD failed>/error/fao=2!/ansi=0
BKUPTMPFILWRITE	<Write to backup temporary file !AD failed>/error/fao=2!/ansi=0
VMSMEMORY2	<Central storage exhausted during allocation of dynamic file descriptor with !UL bytes - check page file quota and page file size>/error/fao=1!/ansi=0
LOADBGSZ2	<Load error: BEGIN too large.  No records loaded.>/error/fao=0!/ansi=0
LOADEDSZ2	<Load error: END too large.  No records loaded.>/error/fao=0!/ansi=0
REPLINSTMISMTCH	<Process has replication instance file !AD (jnlpool shmid = !UL) open but database !AD is bound to instance file !AD (jnlpool shmid = !UL)>/error/fao=8!/ansi=0
REPLINSTREAD	<Error reading [0x!XL] bytes at offset [0x!16@XQ] from replication instance file !AD>/error/fao=4!/ansi=0
REPLINSTDBMATCH	<Replication instance file !AD has seqno [0x!16@XQ] while database has a different seqno [0x!16@XQ]>/error/fao=4!/ansi=0
REPLINSTNMSAME	<Primary and Secondary instances have the same replication instance name !AD>/error/fao=2!/ansi=0
REPLINSTNMUNDEF	<Replication instance name not defined>/error/fao=0!/ansi=0
REPLINSTNMLEN	<Replication instance name !AD should be 1 to 15 characters long>/error/fao=2!/ansi=0
REPLINSTNOHIST	<History information for !AD not found in replication instance file !AD>/error/fao=4!/ansi=0
REPLINSTSECLEN	<Secondary replication instance name !AD should be 1 to 15 characters long>/error/fao=2!/ansi=0
REPLINSTSECMTCH	<Secondary replication instance name !AD sent by receiver does not match !AD specified at source server startup>/error/fao=4!/ansi=0
REPLINSTSECNONE	<No information found for secondary instance !AD in instance file !AD>/error/fao=4!/ansi=0
REPLINSTSECUNDF	<Secondary replication instance name not defined>/error/fao=0!/ansi=0
REPLINSTSEQORD	<!AD has seqno [0x!16@XQ] which is less than last record seqno [0x!16@XQ] in replication instance file !AD>/error/fao=6!/ansi=0
REPLINSTSTNDALN	<Could not get exclusive access to replication instance file !AD>/error/fao=2!/ansi=0
REPLREQROLLBACK	<Replication instance file !AD indicates abnormal shutdown or an incomplete ROLLBACK. Run MUPIP JOURNAL ROLLBACK first>/error/fao=2!/ansi=0
REQROLLBACK	<Error accessing database !AD.  Run MUPIP JOURNAL ROLLBACK on cluster node !AD.>/error/fao=4!/ansi=0
UNUSEDMSG1256	<REPLUPGRADESEC : Last used in V5.4-002B>/error/fao=0!/ansi=0
SRCSRVEXISTS	<Source server for secondary instance !AD is already running with pid !UL>/error/fao=3!/ansi=0
SRCSRVNOTEXIST	<Source server for secondary instance !AD is not alive>/error/fao=2!/ansi=0
SRCSRVTOOMANY	<Cannot start more than !UL source servers in replication instance !AD>/error/fao=3!/ansi=0
JNLPOOLBADSLOT	<Source server slot for secondary instance !AD is in an inconsistent state. Pid = [!UL], State = [!UL], SlotIndex = [!UL]>/warning/fao=5!/ansi=0
NOENDIANCVT	<Unable to convert the endian format of file !AD due to !AD>/error/fao=4!/ansi=0
ENDIANCVT	<Converted database file !AD from !AZ endian to !AZ endian on a !AZ endian system>/info/fao=5!/ansi=0
DBENDIAN	<Database file !AD is !AZ endian on a !AZ endian system>/error/fao=4!/ansi=0
BADCHSET	<!AD is not a valid character mapping in this context>/error/fao=2!/ansi=0
BADCASECODE	<!AD is not a valid case conversion code>/error/fao=2!/ansi=0
BADCHAR		<$ZCHAR(!AD) is not a valid character in the !AD encoding form>/error/fao=4!/ansi=0
DLRCILLEGAL	<!_!AD!/!_!_!_Illegal $CHAR() value !UL>/error/fao=3!/ansi=0
NONUTF8LOCALE	<Locale has character encoding (!AD) which is not compatible with UTF-8 character set>/error/fao=2!/ansi=0
INVDLRCVAL	<Invalid $CHAR() value !UL>/error/fao=1!/ansi=0
DBMISALIGN	<Database file !AD has !UL blocks which does not match alignment rules. Reconstruct the database from a backup or extend it by at least !UL blocks.>/error/fao=4!/ansi=0
LOADINVCHSET	<Extract file CHSET (!AD) is incompatible with gtm_chset>/error/fao=2!/ansi=0
DLLCHSETM	<Routine !AD in library !AD was compiled with CHSET=M which is different from $ZCHSET. Recompile with CHSET=UTF-8 and re-link.>/error/fao=4!/ansi=0
DLLCHSETUTF8	<Routine !AD in library !AD was compiled with CHSET=UTF-8 which is different from $ZCHSET. Recompile with CHSET=M and re-link.>/error/fao=4!/ansi=0
BOMMISMATCH	<!AD Byte Order Marker found when !AD character set specified>/error/fao=4!/ansi=0
WIDTHTOOSMALL	<WIDTH should be at least 2 when device ICHSET or OCHSET is UTF-8 or UTF-16>/error/fao=0!/ansi=0
SOCKMAX		<Attempt to exceed maximum sockets (!UL) for the SOCKET device>/error/fao=1!/ansi=80
PADCHARINVALID	<PAD deviceparameter cannot be greater than 127>/error/fao=0!/ansi=0
ZCNOPREALLOUTPAR	<Parameter !UL in external call !AD.!AD is an output only parameter requiring pre-allocation>/error/fao=5!/ansi=0
SVNEXPECTED	<Special variable expected in this context>/error/fao=0!/ansi=0
SVNONEW		<Cannot NEW this special variable>/error/fao=0!/ansi=0
ZINTDIRECT	<Attempt to enter direct mode from $ZINTERRUPT>/error/fao=0!/ansi=0
ZINTRECURSEIO	<Attempt to do IO to the active device in $ZINTERRUPT>/error/fao=0!/ansi=0
MRTMAXEXCEEDED	<Maximum value of !UL for SOCKET device parameter MOREREADTIME exceeded>/error/fao=1!/ansi=0
JNLCLOSED	<Journaling closed for database file !AD at transaction number 0x!16@XQ>/error/fao=3!/ansi=0
RLBKNOBIMG	<ROLLBACK cannot proceed as database !AD has NOBEFORE_IMAGE journaling>/error/fao=2!/ansi=0
RLBKJNLNOBIMG	<Journal file !AD has NOBEFORE_IMAGE journaling>/info/fao=2!/ansi=0
RLBKLOSTTNONLY	<ROLLBACK will only create a lost transaction file (database and journal files will not be modified)>/info/fao=0!/ansi=0
KILLBYSIGSINFO3	<!AD process !UL has been killed by a signal !UL accessing vaddress 0x!XJ>/fatal/fao=5!/ansi=0
GTMSECSHRTMPPATH	<gtmsecshr path is !AD>/info/fao=2!/ansi=0
GTMERREXIT	<GTM image has exited with errors>/error/fao=0!/ansi=0
INVMEMRESRV	<Could not allocate GT.M memory reserve (!AD)>/warning/fao=2!/ansi=0
OPCOMMISSED	<!UL errors and !UL MBFULLs sending prior operator messages>/info/fao=2!/ansi=0
COMMITWAITSTUCK	<Pid !UL timed out after waiting !UL minute(s) for !UL concurrent GT.M process(es) to finish commits in database file !AD>/error/fao=5!/ansi=0
COMMITWAITPID	<Pid !UL waited !UL minute(s) for pid !UL to finish commits to block 0x!XL in database file !AD>/error/fao=6!/ansi=0
UPDREPLSTATEOFF	<Error replicating global ^!AD as it maps to database !AD which has replication turned OFF>/error/fao=4!/ansi=0
LITNONGRAPH	<M standard requires graphics in string literals>/warning/fao=0!/ansi=0
DBFHEADERR8	<Database file !AD: control problem: !AD was 0x!16@XQ expecting 0x!16@XQ> /info /fao=6!/ansi=0
MMBEFOREJNL	<BEFORE image journaling cannot be set with MM access method in database file !AD>/warning/fao=2!/ansi=0
MMNOBFORRPL	<Replication cannot be used in database file !AD which uses MM access method and NOBEFORE image journaling>/warning/fao=2!/ansi=0
KILLABANDONED	<Abandoned kills counter is greater than zero for file !AD, !AD>/warning/fao=4!/ansi=0
BACKUPKILLIP	<Kill in progress indicator is set for file !AD, backup database could have incorrectly marked busy integrity errors>/warning/fao=2!/ansi=0
LOGTOOLONG	<Environment variable !AD is too long. Maximum length allowed is !UL bytes.>/error/fao=3!/ansi=0
NOALIASLIST	<Parenthetical lists of multiple arguments cannot have a preceding alias introducer or include alias (*) forms>/error/fao=0!/ansi=0
ALIASEXPECTED	<Alias or alias container variable expected in this context>/error/fao=0!/ansi=0
VIEWLVN		<Invalid local variable name used with VIEW/$VIEW(): !AD>/error/fao=2!/ansi=0
DZWRNOPAREN	<$ZWRTACxxx is not allowed inside a parenthesized SET target>/error/fao=0!/ansi=0
DZWRNOALIAS	<$ZWRTAC cannot be aliased>/error/fao=0!/ansi=0
FREEZEERR	<Error while trying to !AD region !AD>/error/fao=4!/ansi=0
CLOSEFAIL	<Error while closing file descriptor !SL>/error/fao=1!/ansi=0
CRYPTINIT		<Could not initialize encryption library while opening encrypted file !AD. !AD>/error/fao=4!/ansi=0
CRYPTOPFAILED		<Encrypt/Decrypt operation failed for file !AD. !AD>/error/fao=4!/ansi=0
CRYPTDLNOOPEN		<Could not load encryption library while opening encrypted file !AD. !AD>/error/fao=4!/ansi=0
CRYPTNOV4		<!AD is an encrypted database. Cannot downgrade(to V4) with Encryption option enabled.>/error/fao=2!/ansi=0
CRYPTNOMM		<!AD is an encrypted database. Cannot support MM access method.>/error/fao=2!/ansi=0
CRYPTJNLWRONGHASH	<Encryption key hash mismatch between journal file !AD and corresponding database file !AD>/error/fao=4!/ansi=0
CRYPTKEYFETCHFAILED	<Could not retrieve encryption key corresponding to file !AD. !AD>/error/fao=4!/ansi=0
CRYPTKEYFETCHFAILEDNF	<Could not retrieve encryption key during !AD operation key. !AD>/error/fao=4!/ansi=0
CRYPTHASHGENFAILED	<Could not generate cryptographic hash for symmetric key corresponding to file !AD. !AD>/error/fao=4!/ansi=0
CRYPTNOPSWDINTP	<Cannot prompt for password inside a TP transaction.>/error/fao=0!/ansi=0
BADTAG		<Unable to use file !AD (CCSID !UL) with CCSID !UL>/error/fao=4!/ansi=0
ICUVERLT36	<!AD !UL.!UL. ICU version greater than or equal to 3.6 should be used>/error/fao=4!/ansi=0
ICUSYMNOTFOUND	<Symbol !AD not found in the ICU libraries. ICU needs to be built with symbol-renaming disabled or gtm_icu_version environment variable needs to be properly specified>/error/fao=2!/ansi=0
STUCKACT	<Process stuck script invoked: !AD : !AD>/info/fao=4!/ansi=0
CALLINAFTERXIT	<After a gtm_exit, a process can never create a valid GT.M context>/error/fao=0!/ansi=0
LOCKSPACEFULL	<No more room for LOCK slots on database file !AD>/error/fao=2!/ansi=0
IOERROR		<Error occured while doing !AD in !AD operation -- called from module !AD at line !UL>/error/fao=7!/ansi=0
MAXSSREACHED	<Maximum snapshots - !UL - for region !AD reached. Please wait for the existing snapshots to complete before starting a new one.>/error/fao=3!/ansi=0
SNAPSHOTNOV4	<Cannot downgrade (to V4) while snapshots are in progress. Currently !UL snapshots are in progress for region !AD.>/error/fao=3!/ansi=0
SSV4NOALLOW	<Database snapshots are supported only on fully upgraded V5 databases. !AD has V4 format blocks.>/error/fao=2!/ansi=0
SSTMPDIRSTAT	<Cannot access temporary directory !AD>/error/fao=2!/ansi=0
SSTMPCREATE	<Cannot create the temporary file in directory !AD for the requested snapshot>/error/fao=2!/ansi=0
JNLFILEDUP	<Journal files !AD and !AD are the same>/error/fao=4!/ansi=0
SSPREMATEOF	<Premature end of file while reading block !UL of size: !UL bytes at offset: !UL from !AD>/error/fao=5!/ansi=0
SSFILOPERR	<Error while doing !AD operation on file !AD>/error/fao=4!/ansi=0
REGSSFAIL	<Process !UL encountered error contributing to the snapshot for region !AD - the snapshot is no longer valid>/error/fao=3!/ansi=0
SSSHMCLNUPFAIL	<Error while doing snapshot shared memory cleanup. Operation -- !AD. Identifier -- !UL>/error/fao=3!/ansi=0
SSFILCLNUPFAIL	<Error while unlinking snapshot file -- !AD>/error/fao=2!/ansi=0
SETINTRIGONLY	<ISV !AD cannot be modified outside of the trigger environment>/error/fao=2!/ansi=0
MAXTRIGNEST	<Maximum trigger nesting level (!UL) exceeded>/error/fao=1!/ansi=0
TRIGCOMPFAIL	<Compilation of database trigger named !AD failed>/error/fao=2!/ansi=0
NOZTRAPINTRIG	<Use of $ZTRAP in a database trigger environment ($ZTLEVEL greater than 0) is not supported>/error/fao=0!/ansi=0
ZTWORMHOLE2BIG	<String length of !UL bytes exceeds maximum length of !UL bytes for $ZTWORMHOLE>/error/fao=2!/ansi=0
JNLENDIANLITTLE	<Journal file !AD is LITTLE endian on a BIG endian system>/error/fao=2!/ansi=0
JNLENDIANBIG	<Journal file !AD is BIG endian on a LITTLE endian system>/error/fao=2!/ansi=0
TRIGINVCHSET	<Trigger !AD for global ^!AD was created with CHSET=!AD which is different from the current $ZCHSET of this process>/error/fao=6!/ansi=0
TRIGREPLSTATE	<Trigger cannot update replicated database file !AD since triggering update was not replicated>/error/fao=2!/ansi=0
GVDATAGETFAIL	<Global variable DATAGET sub-operation (in KILL function) failed. Failure code: !AD.>/error/fao=2!/ansi=0
TRIG2NOTRIG	<Sending transaction sequence number 0x!16@XQ which used triggers to a replicator that does not support triggers>/warning/fao=1!/ansi=0
ZGOTOINVLVL	<ZGOTO in a trigger running in !AD cannot ZGOTO level !UL>/error/fao=3!/ansi=0
TRIGTCOMMIT	<TCOMMIT at $ZTLEVEL=!UL not allowed as corresponding TSTART was done at lower $ZTLEVEL=!UL>/error/fao=2!/ansi=0
TRIGTLVLCHNG	<Detected a net transaction level ($TLEVEL) change during trigger !AD. Transaction level must be the same at exit as when the trigger started>/error/fao=2!/ansi=0
TRIGNAMEUNIQ	<Unable to make trigger name !AD unique beyond !UL versions already loaded>/error/fao=3!/ansi=0
ZTRIGINVACT	<Missing or invalid parameter in position !UL given to $ZTRIGGER()>/error/fao=1!/ansi=0
INDRCOMPFAIL	<Compilation of indirection failed>/error/fao=0!/ansi=0
QUITALSINV	<QUIT * return when the extrinsic was not invoked with SET *>/error/fao=0!/ansi=0
PROCTERM	<!AD process termination due to !AD (return code !UL) from !AD>/warning/fao=7!/ansi=0
SRCLNNTDSP	<Source lines exceeding !UL character width are not displayed>/info/fao=1!/ansi=0
ARROWNTDSP	<Unable to display ^----- due to length of source line>/info/fao=0!/ansi=0
TRIGDEFBAD	<Trigger initialization failed for global ^!AD. Error while processing ^#t("!AD",!AD)>/error/fao=6!/ansi=0
TRIGSUBSCRANGE	<Trigger definition for global ^!AD has one or more invalid subscript range(s) : !AD>/error/fao=4!/ansi=0
TRIGDATAIGNORE	<Ignoring trigger data !AD. Use MUPIP TRIGGER to load trigger definitions>/info/fao=2!/ansi=0
TRIGIS		<!_!_Trigger name: !AD>/info/fao=2!/ansi=0
TCOMMITDISALLOW	<TROLLBACK required after an unhandled error in trigger context>/error/fao=0!/ansi=0
SSATTACHSHM	<Error while attaching to shared memory identifier !UL>/error/fao=1!/ansi=0
TRIGDEFNOSYNC	<Global ^!AD has triggers defined on the !AD instance but none on the !AD instance. Current journal sequence number is 0x!16@XQ>/warning/fao=7!/ansi=0
TRESTMAX	<TRESTART not allowed in a final TP retry more than once>/error/fao=0!/ansi=0
UNUSEDMSG1367	<TPLOCKRESTMAX : Last used in V5.5-000>/error/fao=0!/ansi=0
GBLEXPECTED	<Global variable reference expected in this context>/error/fao=0!/ansi=0
GVZTRIGFAIL	<ZTRIGGER of a global variable failed.  Failure code: !AD.>/error/fao=2!/ansi=0
MUUSERLBK	<Abnormal shutdown of replication-enabled database !AD detected>/error/fao=2!/ansi=0
SETINSETTRIGONLY	<ISV !AD can only be modified in a 'SET' type trigger>/error/fao=2!/ansi=0
DZTRIGINTRIG	<$ZTRIGGER() is not allowed inside trigger context. Trigger name: !AD>/error/fao=2!/ansi=0
SECNODZTRIGINTP	<Sequence number 0x!16@XQ contains $ZTRIGGER() updates made inside a transaction which the current replicating instance does not support. The replicating instance must be upgraded to at least V5.4-002 to support this type of transaction. Cannot continue>/error/fao=1!/ansi=0
BOOLSIDEFFECT	<Extrinsic ($$), External call ($&) or $INCREMENT() with potential side effects in Boolean expression>/warning/fao=0!/ansi=0
DBBADUPGRDSTATE	<Correcting conflicting values for fields describing database version upgrade state in the file header for region !AD (!AD) - make fresh backups with new journal files immediately.>/warning/fao=4!/ansi=0
WRITEWAITPID	<PID !UL waited !UL minute(s) for PID !UL to finish writing block 0x!XL in database file !AD>/error/fao=6!/ansi=0
ZGOCALLOUTIN	<ZGOTO level 0 with entry ref not valid when using call-ins>/error/fao=0!/ansi=0
REPLNOXENDIAN	<!AD side is running on a GT.M version that does not support cross-endian replication. Upgrade the !AD side to at least V5.3-003 to support cross-endian replication. Cannot continue>/error/fao=4!/ansi=0
REPLXENDIANFAIL	<!AD side encountered error while doing endian conversion at journal sequence number 0x!16@XQ>/error/fao=3!/ansi=0
ZGOTOINVLVL2	<ZGOTO 0:entryref is not valid on VMS (UNLINK is a UNIX only feature)>/error/fao=0!/ansi=0
GTMSECSHRCHDIRF	<gtmsecshr unable to chdir to its temporary directory (!AD)>/error/fao=2!/ansi=0
JNLORDBFLU	<Error flushing database blocks to !AD. See related messages in the operator log>/error/fao=2!/ansi=0
ZCCLNUPRTNMISNG	<External call: Cleanup routine name missing. Cannot continue>/error/fao=0!/ansi=0
ZCINVALIDKEYWORD	<External call: Invalid keyword found. Cannot continue>/error/fao=0!/ansi=0
REPLNOMULTILINETRG	<Sequence number 0x!16@XQ contains a trigger definition too large for transmission to the current replicating instance, which does not support multi-line triggers - stopping replication>/error/fao=1!/ansi=0
DBSHMNAMEDIFF	<Database file !AD points to shared memory (id = !UL) which points to a different database file !AZ>/error/fao=4!/ansi=0
SHMREMOVED	<Removed Shared Memory id !UL corresponding to file !AD>/info/fao=3!/ansi=0
DEVICEWRITEONLY	<Cannot read from a write-only device>/error/fao=0!/ansi=0
ICUERROR	<ICU returned status !UL which is either unrecognized or inconsistent with the operating context>/error/fao=1!/ansi=0
ZDATEBADDATE	<$ZDATE() date argument !AD is less than -365 (the $HOROLOG value for 01-JAN-1840) or greater than 364570088 (the $HOROLOG value for 31-DEC-999999)>/error/fao=2!/ansi=0
ZDATEBADTIME	<$ZDATE() time argument !AD is less than 0 or greater than 86399 (the $HOROLOG value for a second before midnight)>/error/fao=2!/ansi=0
COREINPROGRESS	<Previous core attempt failed; core generation bypassed>/error/fao=0!/ansi=0
MAXSEMGETRETRY	<Failed to get ftok semaphore after !UL tries because it is being continually deleted>/error/fao=1!/ansi=0
JNLNOREPL	<Replication not enabled for journal file !AD (database file !AD)>/error/fao=4!/ansi=0
JNLRECINCMPL	<Incomplete journal record at disk address 0x!XL for file !AD while attempting to read seqno 0x!16@XQ>/error/fao=4!/ansi=0
JNLALLOCGROW	<Increased Journal ALLOCATION from [!UL blocks] to [!UL blocks] to match AUTOSWITCHLIMIT for !AZ !AD>/info/fao=5!/ansi=0
INVTRCGRP	<Invalid trace group specified in $gtm_trace_groups: !AD>/error/fao=2!/ansi=0
MUINFOUINT6	<!AD : !UL [0x!XL] ; $H=!UL,!UL>/info/fao=6!/ansi=0
NOLOCKMATCH	<No matching locks were found in !AD>/info/fao=2!/ansi=0
BADREGION	<Region is not BG, MM, or CM>/error/fao=0!/ansi=0
LOCKSPACEUSE	<Estimated free lock space: !UL% of !UL pages>/info/fao=2!/ansi=0
JIUNHNDINT	<An error during $ZINTERRUPT processing was not handled: !AD>/error/fao=2!/ansi=0
GTMASSERT2	<!AD - Assert failed !AD line !UL for expression (!AD)>/fatal/fao=7!/ansi=0
ZTRIGNOTRW	<ZTRIGGER cannot operate on read-only region !AD>/error/fao=2!/ansi=0
TRIGMODREGNOTRW	<Trigger(s) cannot be added/changed/deleted because region !AD is read-only>/error/fao=2!/ansi=0
INSNOTJOINED	<Replicating Instance !AD is not a member of the same Group as Instance !AD>/error/fao=4!/ansi=0
INSROLECHANGE	<Supplementary Instance !AD and non-Supplementary Instance !AD belong to the same Group>/error/fao=4!/ansi=0
INSUNKNOWN	<Supplementary Instance !AD has no instance definition for non-Supplementary Instance !AD>/error/fao=4!/ansi=0
NORESYNCSUPPLONLY	<NORESYNC only supported for Supplementary Instances>/error/fao=0!/ansi=0
NORESYNCUPDATERONLY	<NORESYNC qualifier only allowed on a Supplementary Instance which allows local updates>/error/fao=0!/ansi=0
NOSUPPLSUPPL	<Instance !AD is configured to perform local updates so it cannot receive from Supplementary Instance !AD>/error/fao=4!/ansi=0
REPL2OLD	<Instance !AD uses a GT.M version that does not support connection with the current version on instance !AD>/error/fao=4!/ansi=0
EXTRFILEXISTS	<Error opening output file: !AD -- File exists>/error/fao=2!/ansi=0
MUUSERECOV	<Abnormal shutdown of journaled database !AD detected>/error/fao=2!/ansi=0
SECNOTSUPPLEMENTARY	<!AD is a Supplementary Instance and so cannot act as a source to non-Supplementary Instance !AD >/error/fao=4!/ansi=0
SUPRCVRNEEDSSUPSRC	<Instance !AD is not configured to perform local updates so it cannot act as a receiver for non-Supplementary Instance !AD>/error/fao=4!/ansi=0
UNUSEDMSG1417	<SYNCTOSAMETYPE: Never used before so slot free for reuse>/info/fao=0!/ansi=0
UNUSEDMSG1418	<TARGINSRUNNING: Never used before so slot free for reuse>/info/fao=0!/ansi=0
UPDSYNC2MTINS	<Can only UPDATERESYNC with an empty instance file>/error/fao=0!/ansi=0
UPDSYNCINSTFILE	<Error with instance file name specified in UPDATERESYNC qualifier>/error/fao=0!/ansi=0
REUSEINSTNAME	<Error with instance name specified in REUSE qualifier>/error/fao=0!/ansi=0
RCVRMANYSTRMS	<Receiver server now connecting to source stream [!2UL] but had previously connected to a different stream [!2UL]>/error/fao=2!/ansi=0
RSYNCSTRMVAL	<RSYNC_STRM qualifier can only take on a value from 0 to 15>/error/fao=0!/ansi=0
RLBKSTRMSEQ	<Stream journal seqno of the instance after rollback is Stream !2UL : Seqno !@ZQ [0x!16@XQ]>/info/fao=3!/ansi=0
RESOLVESEQSTRM	<Resolving until stream sequence number Stream !2UL : Seqno !@ZQ [0x!16@XQ]>/info/fao=3!/ansi=0
REPLINSTDBSTRM	<Replication instance file !AD has seqno [0x!16@XQ] for Stream !2UL while database has a different seqno [0x!16@XQ]>/error/fao=5!/ansi=0
RESUMESTRMNUM	<Error with stream number specified in RESUME qualifier>/error/fao=0!/ansi=0
ORLBKSTART	<ONLINE ROLLBACK started on instance !AD corresponding to !AD>/info/fao=4!/ansi=0
ORLBKTERMNTD	<ONLINE ROLLBACK terminated on instance !AD corresponding to !AD with the above errors>/error/fao=4!/ansi=0
ORLBKCMPLT	<ONLINE ROLLBACK completed successfully on instance !AD corresponding to !AD>/info/fao=4!/ansi=0
ORLBKNOSTP	<ONLINE ROLLBACK proceeding with database updates. MUPIP STOP will no longer be allowed>/info/fao=0!/ansi=0
ORLBKFRZPROG	<!AD : waiting for FREEZE on region !AD (!AD) to clear>/info/fao=6!/ansi=0
ORLBKFRZOVER	<!AD : FREEZE on region !AD (!AD) cleared>/info/fao=6!/ansi=0
ORLBKNOV4BLK	<Region !AD (!AD) has V4 format blocks. Database upgrade required. ONLINE ROLLBACK cannot continue>/error/fao=4!/ansi=0
DBROLLEDBACK	<Concurrent ONLINE ROLLBACK detected on one or more regions. The current operation is no longer valid>/error/fao=0!/ansi=0
DSEWCREINIT	<Database cache reinitialized by DSE for region !AD>/info/fao=2!/ansi=0
MURNDWNOVRD	<OVERRIDE qualifier used with MUPIP RUNDOWN on database file !AD>/info/fao=2!/ansi=0
! The following error message is NOT issued to the user and is only internal to GT.M
REPLONLNRLBK	<ONLINE ROLLBACK detected. Starting afresh>/error/fao=0!/ansi=0
!
SRVLCKWT2LNG	<PID !UL is holding the source server lock. Waited for !UL minute(s). Now exiting>/error/fao=2!/ansi=0
IGNBMPMRKFREE	<Ignoring bitmap free-up operation for region !AD (!AD) due to concurrent ONLINE ROLLBACK>/info/fao=4!/ansi=0
PERMGENFAIL	<Failed to determine access permissions to use for creation of !AD for file !AD>/error/fao=4!/ansi=0
PERMGENDIAG	<Permissions: Proc(uid:!UL,gid:!UL), DB File(uid:!UL,gid:!UL,perm:!AD), Lib File(gid:!UL,perm:!AD), Group Mem(opener:!UL,owner:!UL)>/info/fao=11!/ansi=0
MUTRUNC1ATIME	<Process with PID !UL already performing truncate in region !AD>/info/fao=3!/ansi=0
MUTRUNCBACKINPROG	<Truncate detected concurrent backup in progress for region !AD>/info/fao=2!/ansi=0
MUTRUNCERROR	<Truncate of region !AD encountered service error !AD>/error/fao=4!/ansi=0
MUTRUNCFAIL	<Truncate failed after reorg>/error/fao=0!/ansi=0
MUTRUNCNOSPACE	<Region !AD has insufficient space to meet truncate target percentage of !UL>/info/fao=3!/ansi=0
MUTRUNCNOTBG	<Region !AD does not have access method BG >/error/fao=2!/ansi=0
MUTRUNCNOV4	<Region !AD is not fully upgraded from V4 format.>/error/fao=2!/ansi=0
MUTRUNCPERCENT	<Truncate threshold percentage should be from 0 to 99>/error/fao=0!/ansi=0
MUTRUNCSSINPROG	<Truncate detected concurrent snapshot in progress for region !AD>/info/fao=2!/ansi=0
MUTRUNCSUCCESS	<Database file !AD truncated from 0x!XL blocks to 0x!XL at transaction 0x!16@XQ>/info/fao=5!/ansi=0
RSYNCSTRMSUPPLONLY	<RSYNC_STRM qualifier only supported for Supplementary Instances>/error/fao=0!/ansi=0
STRMNUMIS	<Stream # is !2UL>/info/fao=1!/ansi=0
STRMNUMMISMTCH1	<Stream !2UL exists on the receiver instance file but is unknown on the source instance>/error/fao=1!/ansi=0
STRMNUMMISMTCH2	<Stream !2UL exists on the source instance file but is unknown on the receiver instance>/error/fao=1!/ansi=0
STRMSEQMISMTCH	<Unable to play update on Stream !2UL with seqno [0x!16@XQ] as receiving instance has a different stream seqno [0x!16@XQ]>/error/fao=3!/ansi=0
LOCKSPACEINFO	<Region: !AD: processes on queue: !UL/!UL; LOCK slots in use: !UL/!UL; name space!ADfull>/info/fao=8!/ansi=0
JRTNULLFAIL	<Applying NULL journal record failed.  Failure code: !AD.>/error/fao=2!/ansi=0
LOCKSUB2LONG	<Following subscript is !UL bytes long which exceeds 255 byte limit.>/error/fao=1!/ansi=0
RESRCWAIT	<Waiting briefly for the !AD semaphore for region !AD (!AD) was held by PID !UL (Sem. ID: !UL).>/info/fao=8!/ansi=0
RESRCINTRLCKBYPAS	<!AD with PID !UL bypassing the !AD semaphore for region !AD (!AD) was held by PID !UL.>/info/fao=10!/ansi=0
DBFHEADERRANY	<Database file !AD: control problem: !AD was 0x!XJ expecting 0x!XJ> /info /fao=6!/ansi=0
REPLINSTFROZEN	<Instance !AZ is now Frozen>/error/fao=1!/ansi=0
REPLINSTFREEZECOMMENT	<Freeze Comment: !AZ>/info/fao=1!/ansi=0
REPLINSTUNFROZEN	<Instance !AZ is now Unfrozen>/info/fao=1!/ansi=0
DSKNOSPCAVAIL	<Attempted write to file !AD failed due to lack of disk space. Retrying indefinitely.>/info/fao=2!/ansi=0
DSKNOSPCBLOCKED	<Retry of write to file !AD suspended due to new instance freeze. Waiting for instance to be unfrozen.>/error/fao=2!/ansi=0
DSKSPCAVAILABLE	<Write to file !AD succeeded after out-of-space condition cleared>/info/fao=2!/ansi=0
ENOSPCQIODEFER	<Write to file !AD deferred due to lack of disk space>/info/fao=2!/ansi=0
CUSTOMFILOPERR	<Error while doing !AD operation on file !AD>/error/fao=4!/ansi=0
CUSTERRNOTFND	<Error mnemonic !AD specified in custom errors file is not valid for this version of GT.M>/error/fao=2!/ansi=0
CUSTERRSYNTAX	<Syntax error in file !AD at line number !UL>/error/fao=3!/ansi=0
ORLBKINPROG	<Online ROLLBACK in progress by PID !UL in region !AD>/info/fao=3!/ansi=0
DBSPANGLOINCMP	<!AD Spanning node is missing. Block no !UL of spanning node is missing>/error/fao=3!/ansi=0
DBSPANCHUNKORD	<!AD Chunk of !UL blocks is out of order>/error/fao=3!/ansi=0
DBDATAMX	<!AD Record too large>/error/fao=2!/ansi=0
DBIOERR		<Error while doing write operation on region !AD (!AD)>/error/fao=4!/ansi=0
INITORRESUME	<UPDATERESYNC on a Supplementary Instance must additionally specify INITIALIZE or RESUME>/error/fao=0!/ansi=0
GTMSECSHRNOARG0	<gtmsecshr cannot identify its origin - argv[0] is null>/error/fao=0!/ansi=0
GTMSECSHRISNOT	<gtmsecshr is not running as gtmsecshr but !AD - must be gtmsecshr>/error/fao=2!/ansi=0
GTMSECSHRBADDIR	<gtmsecshr is not running from $gtm_dist/gtmsecshrdir or $gtm_dist cannot be determined>/error/fao=0!/ansi=0
JNLBUFFREGUPD	<Journal file buffer size for region !AD has been adjusted from !UL to !UL.>/warning/fao=4!/ansi=0
JNLBUFFDBUPD	<Journal file buffer size for database file !AD has been adjusted from !UL to !UL.>/warning/fao=4!/ansi=0
LOCKINCR2HIGH	<Attempt to increment a LOCK more than !UL times>/error/fao=1!/ansi=0
LOCKIS		<!_!_Resource name: !AD>/info/fao=2!/ansi=0
LDSPANGLOINCMP	<Incomplete spanning node found during load>/error/fao=0!/ansi=0
MUFILRNDWNFL2	<Database section (id = !UL) belonging to database file !AD rundown failed>/error/fao=3!/ansi=0
MUINSTFROZEN	<!AD : Instance !AZ is frozen. Waiting for instance to be unfrozen before proceeding with writes to database file !AD>/info/fao=5!/ansi=0
MUINSTUNFROZEN	<!AD : Instance !AZ is now Unfrozen. Continuing with writes to database file !AD>/info/fao=5!/ansi=0
GTMEISDIR	<!AD : Is a directory>/error/fao=2!/ansi=0
SPCLZMSG	<The following error message cannot be driven through ZMESSAGE>/error/fao=0!/ansi=0
MUNOTALLINTEG	<At least one region skipped. See the earlier messages>/warning/fao=0!/ansi=0
BKUPRUNNING	<Process !UL is currently backing up region !AD. Cannot start another backup.>/error/fao=3!/ansi=0
MUSIZEINVARG	<MUPIP SIZE : Invalid parameter value for: !AD>/error/fao=2!/ansi=0
MUSIZEFAIL	<MUPIP SIZE : failed.  Failure code: !AD.>/error/fao=2!/ansi=0
SIDEEFFECTEVAL	<Extrinsic ($$), External call ($&) or $INCREMENT() with potential side effects in actuallist, function arguments, non-Boolean binary operands or subscripts>/warning/fao=0!/ansi=0
CRYPTINIT2	<Could not initialize encryption library !AD. !AD>/error/fao=4!/ansi=0
CRYPTDLNOOPEN2	<Could not load encryption library !AD. !AD>/error/fao=4!/ansi=0
CRYPTBADCONFIG	<Could not retrieve data from encrypted file !AD due to bad encryption configuration. !AD>/error/fao=4!/ansi=0
DBCOLLREQ	<JOURNAL EXTRACT proceeding without collation information for globals in database. !AD !AD>/warning/fao=4!/ansi=0
SETEXTRENV	<Database files are missing or Instance is frozen; supply the database files, wait for the freeze to lift or define gtm_extract_nocol to extract possibly incorrect collation>/error/fao=0!/ansi=0
NOTALLDBRNDWN	<Not all regions were successfully rundown>/error/fao=0!/ansi=0
TPRESTNESTERR	<TP restart signaled while handing error - treated as nested error - Use TROLLBACK in error handler to avoid this>/error/fao=0!/ansi=0
JNLFILRDOPN	<Error opening journal file !AD for read for database file !AD>/error/fao=4!/ansi=0
SEQNUMSEARCHTIMEOUT	<Timed out trying to find sequence number !@ZQ [0x!16@XQ] in Journal File(s). See above messages for details. Source server exiting>/error/fao=2!/ansi=0
FTOKKEY		<FTOK key 0x!XL>/info/fao=1!/ansi=0
SEMID		<Semaphore id !UL>/info/fao=1!/ansi=0
JNLQIOSALVAGE	<Journal IO lock salvaged>/info/fao=0!/ansi=0
FAKENOSPCLEARED	<DEBUG: All fake ENOSPC flags were cleared !UL heartbeats ago>/info/fao=1!/ansi=0
MMFILETOOLARGE	<Size of !AD region (!AD) is larger than maximum size supported for memory mapped I/O on this platform>/error/fao=4!/ansi=0
BADZPEEKARG	<Missing, invalid or surplus !AD parameter for $ZPEEK()>/error/fao=2!/ansi=0
BADZPEEKRANGE	<Access exception raised in memory range given to $ZPEEK()>/error/fao=0!/ansi=0
BADZPEEKFMT	<$ZPEEK() value length inappropriate for selected format>/error/fao=0!/ansi=0
DBMBMINCFREFIXED	<Master bitmap incorrectly marks local bitmap 0x!XL as free. Auto-corrected>/warning/fao=1!/ansi=0
NULLENTRYREF	<JOB command did not specify entryref>/error/fao=0!/ansi=0
ZPEEKNORPLINFO	<$ZPEEK() unable to access requested replication structure>/error/fao=0!/ansi=0
MMREGNOACCESS	<Region !AD (!AD) is no longer accessible. See prior error messages in the operator and application error logs>/error/fao=4!/ansi=0
MALLOCMAXUNIX	<Exceeded maximum allocation defined by $gtm_max_storalloc>/error/fao=0!/ansi=0
MALLOCMAXVMS	<Exceeded maximum allocation defined by GTM_MAX_STORALLOC>/error/fao=0!/ansi=0
HOSTCONFLICT	<Host !AD could not open database file !AD because it is marked as already open on node !AD>/error/fao=6!/ansi=0
GETADDRINFO	<Error in getting address info>/error/fao=0!/ansi=0
GETNAMEINFO	<Error in getting name info>/error/fao=0!/ansi=0
SOCKBIND	<Error in binding TCP socket>/error/fao=0!/ansi=0
INSTFRZDEFER	<Instance Freeze initiated by !AD error on region !AD deferred due to critical resource conflict>/info/fao=4!/ansi=0
REGOPENRETRY	<Attempt to open region !AD (!AD) using startup shortcut failed due to conflicting database shutdown. Retrying...>/info/fao=4!/ansi=0
REGOPENFAIL	<Failed to open region !AD (!AD) due to conflicting database shutdown activity>/error/fao=4!/ansi=0
!
! If there are UNUSEDMSG* lines unused for more than one year and at least two non-patch releases, use them before adding new lines.
!
	.end
