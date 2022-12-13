/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries.*
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

LITDEF	err_msg gdeerrors[] = {
	{ "BLKSIZ512", "Block size !AD rounds to !AD", 4, 0 },
	{ "EXECOM", "Executing command file !AD", 2, 0 },
	{ "FILENOTFND", "File !AD not found", 2, 0 },
	{ "GDCREATE", "Creating Global Directory file !/	!AD", 2, 0 },
	{ "GDECHECK", "Internal GDE consistency check", 0, 0 },
	{ "GDUNKNFMT", "!AD !/	is not formatted as a Global Directory", 2, 0 },
	{ "GDUPDATE", "Updating Global Directory file !/	!AD", 2, 0 },
	{ "GDUSEDEFS", "Using defaults for Global Directory !/	!AD", 2, 0 },
	{ "ILLCHAR", "!AD is not a legal character in this context", 2, 0 },
	{ "INPINTEG", "Input integrity error -- aborting load", 0, 0 },
	{ "KEYTOOBIG", "But record size !AD can only support key size !AD", 4, 0 },
	{ "KEYSIZIS", "Key size is !AD", 2, 0 },
	{ "KEYWRDAMB", "!AD is ambiguous for !AD", 4, 0 },
	{ "KEYWRDBAD", "!AD is not a valid !AD in this context", 4, 0 },
	{ "LOADGD", "Loading Global Directory file !/	!AD", 2, 0 },
	{ "LOGOFF", "No longer logging to file !AD", 2, 0 },
	{ "LOGON", "Logging to file !AD", 2, 0 },
	{ "LVSTARALON", "The * name cannot be deleted or renamed", 0, 0 },
	{ "MAPBAD", "!AD !AD for !AD !AD does not exist", 8, 0 },
	{ "MAPDUP", "!AD !AD and !AD both map to !AD !AD", 10, 0 },
	{ "NAMENDBAD", "Subscripted name !AD must end with right parenthesis", 2, 0 },
	{ "NOACTION", "Not updating Global Directory !AD", 2, 0 },
	{ "RPAREN", "List must end with right parenthesis or continue with comma", 0, 0 },
	{ "NOEXIT", "Cannot exit because of verification failure", 0, 0 },
	{ "NOLOG", "Logging is currently disabled!/ Log file is !AD.", 2, 0 },
	{ "NOVALUE", "Qualifier !AD does not take a value", 2, 0 },
	{ "NONEGATE", "Qualifier !AD cannot be negated", 2, 0 },
	{ "OBJDUP", "!AD !AD already exists", 4, 0 },
	{ "OBJNOTADD", "Not adding !AD !AD", 4, 0 },
	{ "OBJNOTCHG", "Not changing !AD !AD", 4, 0 },
	{ "OBJNOTFND", "!AD !AD does not exist", 4, 0 },
	{ "OBJREQD", "!AD required", 2, 0 },
	{ "PREFIXBAD", "!AD - !AD !AD must start with an alphabetic character", 6, 0 },
	{ "QUALBAD", "!AD is not a valid qualifier", 2, 0 },
	{ "QUALDUP", "!AD qualifier appears more than once in the list", 2, 0 },
	{ "QUALREQD", "!AD required", 2, 0 },
	{ "RECTOOBIG", "Block size !AD and !AD reserved bytes limit record size to !AD", 6, 0 },
	{ "RECSIZIS", "Record size is !AD", 2, 0 },
	{ "REGIS", "in region !AD", 2, 0 },
	{ "SEGIS", "in !AD segment !AD", 4, 0 },
	{ "VALTOOBIG", "!AD is larger than the maximum of !AD for a !AD", 6, 0 },
	{ "VALTOOLONG", "!AD exceeds the maximum length of !AD for a !AD", 6, 0 },
	{ "VALTOOSMALL", "!AD is less than the minimum of !AD for a !AD", 6, 0 },
	{ "VALUEBAD", "!AD is not a valid !AD", 4, 0 },
	{ "VALUEREQD", "Qualifier !AD requires a value", 2, 0 },
	{ "VERIFY", "Verification !AD", 2, 0 },
	{ "BUFSIZIS", "Journal Buffer size is !AD", 2, 0 },
	{ "BUFTOOSMALL", "But block size !AD requires buffer size !AD", 4, 0 },
	{ "MMNOBEFORIMG", "MM segments do not support before image jounaling", 0, 0 },
	{ "NOJNL", "!AD segments do not support journaling", 2, 0 },
	{ "GDREADERR", "Error reading Global Directory: !AD", 2, 0 },
	{ "GDNOTSET", "Global Directory not changed because the current GD cannot be written", 0, 0 },
	{ "INVGBLDIR", "Invalid Global Directory spec: !AD.!/Continuing with !AD", 4, 0 },
	{ "WRITEERROR", "Cannot exit because of write failure.  Reason for failure: !AD", 2, 0 },
	{ "NONASCII", "!AD is illegal for a !AD as it contains non-ASCII characters", 4, 0 },
	{ "GDECRYPTNOMM", "!AD segment has encryption turned on. Cannot support MM access method.", 2, 0 },
	{ "JNLALLOCGROW", "Increased Journal ALLOCATION from [!AD blocks] to [!AD blocks] to match AUTOSWITCHLIMIT for !AD !AD", 8, 0 },
	{ "KEYFORBLK", "But block size !AD and reserved bytes !AD limit key size to !AD", 6, 0 },
	{ "STRMISSQUOTE", "Missing double-quote at end of string specification !AD", 2, 0 },
	{ "GBLNAMEIS", "in gblname !AD", 2, 0 },
	{ "NAMSUBSEMPTY", "Subscript #!UL is empty in name specification", 1, 0 },
	{ "NAMSUBSBAD", "Subscript #!UL with value !AD in name specification is an invalid number or string", 3, 0 },
	{ "NAMNUMSUBSOFLOW", "Subscript #!UL with value !AD in name specification has a numeric overflow", 3, 0 },
	{ "NAMNUMSUBNOTEXACT", "Subscript #!UL with value !AD in name specification is not an exact GT.M number", 3, 0 },
	{ "MISSINGDELIM", "Delimiter !AD expected before !AD !AD", 6, 0 },
	{ "NAMRANGELASTSUB", "Ranges in name specification !AD are allowed only in the last subscript", 2, 0 },
	{ "NAMSTARSUBSMIX", "Name specification !AD cannot contain * and subscripts at the same time", 2, 0 },
	{ "NAMLPARENNOTBEG", "Subscripted Name specification !AD needs to have a left parenthesis at the beginning of subscripts", 2, 0 },
	{ "NAMRPARENNOTEND", "Subscripted Name specification !AD cannot have anything following the right parenthesis at the end of subscripts", 2, 0 },
	{ "NAMONECOLON", "Subscripted Name specification !AD must have at most one colon (range) specification", 2, 0 },
	{ "NAMRPARENMISSING", "Subscripted Name specification !AD is missing one or more right parentheses at the end of subscripts", 2, 0 },
	{ "NAMGVSUBSMAX", "Subscripted Name specification !AD has more than the maximum # of subscripts (!UL)", 3, 0 },
	{ "NAMNOTSTRSUBS", "Subscript #!UL with value !AD in name specification is not a properly formatted string subscript", 3, 0 },
	{ "NAMSTRSUBSFUN", "Subscript #!UL with value !AD in name specification uses function other than $C/$CHAR/$ZCH/$ZCHAR", 3, 0 },
	{ "NAMSTRSUBSLPAREN", "Subscript #!UL with value !AD in name specification does not have left parenthesis following $ specification", 3, 0 },
	{ "NAMSTRSUBSCHINT", "Subscript #!UL with value !AD in name specification does not have a positive integer inside $C/$CHAR/$ZCH/$ZCHAR", 3, 0 },
	{ "NAMSTRSUBSCHARG", "Subscript #!UL with value !AD in name specification specifies a $C/$ZCH with number !UL that is invalid in the current $zchset", 4, 0 },
	{ "GBLNAMCOLLUNDEF", "Error opening shared library of collation sequence #!UL for GBLNAME !AD", 3, 0 },
	{ "NAMRANGEORDER", "Range in name specification !AD specifies out-of-order subscripts using collation sequence #!UL", 3, 0 },
	{ "NAMRANGEOVERLAP", "Range in name specifications !AD and !AD overlap using collation sequence #!UL", 5, 0 },
	{ "NAMGVSUBOFLOW", "Subscripted name !AD...!AD is too long to represent in the database using collation value #!UL", 5, 0 },
	{ "GBLNAMCOLLRANGE", "Collation sequence #!UL is out of range (0 thru 255)", 1, 0 },
	{ "STDNULLCOLLREQ", "Region !AD needs Standard Null Collation enabled because global !AD spans through it", 4, 0 },
	{ "GBLNAMCOLLVER", "Global directory indicates GBLNAME !AD has collation sequence #!UL with a version #!UL but shared library reports different version #!UL", 5, 0 },
	{ "GDEASYNCIONOMM", "!AD segment has ASYNCIO turned on. Cannot support MM access method.", 2, 0 },
<<<<<<< HEAD:sr_port/gdeerrors_ctl.c
	{ "NOPERCENTY", "^%Y* is a reserved global name in YottaDB", 0, 0 },
};

=======
	{ "NOPERCENTY", "^%Y* is a reserved global name in GT.M", 0, 0 },
	{ "GDELOGFAIL", "GDE failed to log command. Check operator log for more information", 0, 0 },
};

LITDEF	int GDE_BLKSIZ512 = 150503435;
LITDEF	int GDE_EXECOM = 150503443;
LITDEF	int GDE_FILENOTFND = 150503450;
LITDEF	int GDE_GDCREATE = 150503459;
LITDEF	int GDE_GDECHECK = 150503467;
LITDEF	int GDE_GDUNKNFMT = 150503475;
LITDEF	int GDE_GDUPDATE = 150503483;
LITDEF	int GDE_GDUSEDEFS = 150503491;
LITDEF	int GDE_ILLCHAR = 150503498;
LITDEF	int GDE_INPINTEG = 150503508;
LITDEF	int GDE_KEYTOOBIG = 150503515;
LITDEF	int GDE_KEYSIZIS = 150503523;
LITDEF	int GDE_KEYWRDAMB = 150503530;
LITDEF	int GDE_KEYWRDBAD = 150503538;
LITDEF	int GDE_LOADGD = 150503547;
LITDEF	int GDE_LOGOFF = 150503555;
LITDEF	int GDE_LOGON = 150503563;
LITDEF	int GDE_LVSTARALON = 150503570;
LITDEF	int GDE_MAPBAD = 150503579;
LITDEF	int GDE_MAPDUP = 150503587;
LITDEF	int GDE_NAMENDBAD = 150503594;
LITDEF	int GDE_NOACTION = 150503603;
LITDEF	int GDE_RPAREN = 150503610;
LITDEF	int GDE_NOEXIT = 150503619;
LITDEF	int GDE_NOLOG = 150503627;
LITDEF	int GDE_NOVALUE = 150503634;
LITDEF	int GDE_NONEGATE = 150503642;
LITDEF	int GDE_OBJDUP = 150503650;
LITDEF	int GDE_OBJNOTADD = 150503658;
LITDEF	int GDE_OBJNOTCHG = 150503666;
LITDEF	int GDE_OBJNOTFND = 150503674;
LITDEF	int GDE_OBJREQD = 150503682;
LITDEF	int GDE_PREFIXBAD = 150503690;
LITDEF	int GDE_QUALBAD = 150503698;
LITDEF	int GDE_QUALDUP = 150503706;
LITDEF	int GDE_QUALREQD = 150503714;
LITDEF	int GDE_RECTOOBIG = 150503723;
LITDEF	int GDE_RECSIZIS = 150503731;
LITDEF	int GDE_REGIS = 150503739;
LITDEF	int GDE_SEGIS = 150503747;
LITDEF	int GDE_VALTOOBIG = 150503755;
LITDEF	int GDE_VALTOOLONG = 150503762;
LITDEF	int GDE_VALTOOSMALL = 150503771;
LITDEF	int GDE_VALUEBAD = 150503778;
LITDEF	int GDE_VALUEREQD = 150503786;
LITDEF	int GDE_VERIFY = 150503795;
LITDEF	int GDE_BUFSIZIS = 150503803;
LITDEF	int GDE_BUFTOOSMALL = 150503811;
LITDEF	int GDE_MMNOBEFORIMG = 150503819;
LITDEF	int GDE_NOJNL = 150503827;
LITDEF	int GDE_GDREADERR = 150503835;
LITDEF	int GDE_GDNOTSET = 150503843;
LITDEF	int GDE_INVGBLDIR = 150503851;
LITDEF	int GDE_WRITEERROR = 150503859;
LITDEF	int GDE_NONASCII = 150503866;
LITDEF	int GDE_GDECRYPTNOMM = 150503874;
LITDEF	int GDE_JNLALLOCGROW = 150503883;
LITDEF	int GDE_KEYFORBLK = 150503891;
LITDEF	int GDE_STRMISSQUOTE = 150503898;
LITDEF	int GDE_GBLNAMEIS = 150503907;
LITDEF	int GDE_NAMSUBSEMPTY = 150503914;
LITDEF	int GDE_NAMSUBSBAD = 150503922;
LITDEF	int GDE_NAMNUMSUBSOFLOW = 150503930;
LITDEF	int GDE_NAMNUMSUBNOTEXACT = 150503938;
LITDEF	int GDE_MISSINGDELIM = 150503946;
LITDEF	int GDE_NAMRANGELASTSUB = 150503954;
LITDEF	int GDE_NAMSTARSUBSMIX = 150503962;
LITDEF	int GDE_NAMLPARENNOTBEG = 150503970;
LITDEF	int GDE_NAMRPARENNOTEND = 150503978;
LITDEF	int GDE_NAMONECOLON = 150503986;
LITDEF	int GDE_NAMRPARENMISSING = 150503994;
LITDEF	int GDE_NAMGVSUBSMAX = 150504002;
LITDEF	int GDE_NAMNOTSTRSUBS = 150504010;
LITDEF	int GDE_NAMSTRSUBSFUN = 150504018;
LITDEF	int GDE_NAMSTRSUBSLPAREN = 150504026;
LITDEF	int GDE_NAMSTRSUBSCHINT = 150504034;
LITDEF	int GDE_NAMSTRSUBSCHARG = 150504042;
LITDEF	int GDE_GBLNAMCOLLUNDEF = 150504050;
LITDEF	int GDE_NAMRANGEORDER = 150504058;
LITDEF	int GDE_NAMRANGEOVERLAP = 150504066;
LITDEF	int GDE_NAMGVSUBOFLOW = 150504074;
LITDEF	int GDE_GBLNAMCOLLRANGE = 150504082;
LITDEF	int GDE_STDNULLCOLLREQ = 150504091;
LITDEF	int GDE_GBLNAMCOLLVER = 150504098;
LITDEF	int GDE_GDEASYNCIONOMM = 150504106;
LITDEF	int GDE_NOPERCENTY = 150504114;
LITDEF	int GDE_GDELOGFAIL = 150504122;
>>>>>>> 732d6f04 (GT.M V7.0-005):sr_x86_64/gdeerrors_ctl.c


LITDEF	int gdeerrors_undocarr[] = {
};


GBLDEF	err_ctl gdeerrors_ctl = {
	248,
	"GDE",
	&gdeerrors[0],
	87,
	&gdeerrors_undocarr[0],
	0
};
