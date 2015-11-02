/****************************************************************
 *								*
 *	Copyright 2001,2013 Fidelity Information Services, Inc	*
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
	"BLKSIZ512", "Block size !AD rounds to !AD", 4,
	"EXECOM", "Executing command file !AD", 2,
	"FILENOTFND", "File !AD not found", 2,
	"GDCREATE", "Creating Global Directory file !/	!AD", 2,
	"GDECHECK", "Internal GDE consistency check", 0,
	"GDUNKNFMT", "!AD !/	is not formatted as a Global Directory", 2,
	"GDUPDATE", "Updating Global Directory file !/	!AD", 2,
	"GDUSEDEFS", "Using defaults for Global Directory !/	!AD", 2,
	"ILLCHAR", "!AD is not a legal character in this context", 2,
	"INPINTEG", "Input integrity error -- aborting load", 0,
	"KEYTOOBIG", "But record size !AD can only support key size !AD", 4,
	"KEYSIZIS", "Key size is !AD", 2,
	"KEYWRDAMB", "!AD is ambiguous for !AD", 4,
	"KEYWRDBAD", "!AD is not a valid !AD", 4,
	"LOADGD", "Loading Global Directory file !/	!AD", 2,
	"LOGOFF", "No longer logging to file !AD", 2,
	"LOGON", "Logging to file !AD", 2,
	"LVSTARALON", "The * name cannot be deleted or renamed", 0,
	"MAPBAD", "!AD !AD for !AD !AD does not exist", 8,
	"MAPDUP", "!AD !AD and !AD both map to !AD !AD", 10,
	"NAMSTARTBAD", "!AD must start with '%' or an alphabetic character", 2,
	"NOACTION", "Not updating Global Directory !AD", 2,
	"RPAREN", "List must end with right parenthesis or continue with comma", 0,
	"NOEXIT", "Cannot exit because of verification failure", 0,
	"NOLOG", "Logging is currently disabled!/ Log file is !AD.", 2,
	"NOVALUE", "Qualifier !AD does not take a value", 2,
	"NONEGATE", "Qualifier !AD cannot be negated", 2,
	"OBJDUP", "!AD !AD already exists", 4,
	"OBJNOTADD", "Not adding !AD !AD", 4,
	"OBJNOTCHG", "Not changing !AD !AD", 4,
	"OBJNOTFND", "!AD !AD does not exist", 4,
	"OBJREQD", "!AD required", 2,
	"PREFIXBAD", "!AD must start with an alphabetic character to be a !AD", 4,
	"QUALBAD", "!AD is not a valid qualifier", 2,
	"QUALDUP", "!AD qualifier appears more than once in the list", 2,
	"QUALREQD", "!AD required", 2,
	"RECTOOBIG", "Block size !AD and !AD reserved bytes limit record size to !AD", 6,
	"RECSIZIS", "Record size is !AD", 2,
	"REGIS", "in region !AD", 2,
	"SEGIS", "in !AD segment !AD", 4,
	"VALTOOBIG", "!AD is larger than the maximum of !AD for a !AD", 6,
	"VALTOOLONG", "!AD exceeds the maximum length of !AD for a !AD", 6,
	"VALTOOSMALL", "!AD is less than the minimum of !AD for a !AD", 6,
	"VALUEBAD", "!AD is not a valid !AD", 4,
	"VALUEREQD", "Qualifier !AD requires a value", 2,
	"VERIFY", "Verification !AD", 2,
	"BUFSIZIS", "Journal Buffer size is !AD", 2,
	"BUFTOOSMALL", "But block size !AD requires buffer size !AD", 4,
	"MMNOBEFORIMG", "MM segments do not support before image jounaling", 0,
	"NOJNL", "!AD segments do not support journaling", 2,
	"GDREADERR", "Error reading Global Directory: !AD", 2,
	"GDNOTSET", "Global Directory not changed because the current GD cannot be written", 0,
	"INVGBLDIR", "Invalid Global Directory spec: !AD.!/Continuing with !AD", 4,
	"WRITEERROR", "Cannot exit because of write failure.  Reason for failure: !AD", 2,
	"NONASCII", "!AD is illegal for a !AD as it contains non-ASCII characters", 4,
	"CRYPTNOMM", "!AD is an encrypted database. Cannot support MM access method.", 2,
	"JNLALLOCGROW", "Increased Journal ALLOCATION from [!AD blocks] to [!AD blocks] to match AUTOSWITCHLIMIT for !AD !AD", 8,
	"KEYFORBLK", "But block size !AD can only support key size !AD", 4,
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
LITDEF	int GDE_NAMSTARTBAD = 150503594;
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
LITDEF	int GDE_CRYPTNOMM = 150503874;
LITDEF	int GDE_JNLALLOCGROW = 150503883;
LITDEF	int GDE_KEYFORBLK = 150503891;

GBLDEF	err_ctl gdeerrors_ctl = {
	248,
	"GDE",
	&gdeerrors[0],
	58};
