/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	This facility loads a pattern table from the file specified.
	Returns success as a Boolean.  A sample pattern table definition
	file follows:

	PATSTART
	   PATTABLE EDM
	      PATCODE c
		 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,-
		 24,25,26,27,28,29,30,31,127,128,129,130,131,132,133,134,135,136,-
		 137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,-
		 153,154,155,156,157,158,159,255
	      PATCODE n
		 48,49,50,51,52,53,54,55,56,57
	      PATCODE u
		 65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,-
		 86,87,88,89,90,192,193,194,195,196,197,198,199,200,201,202,203,-
		 204,205,206,207,209,210,211,212,213,214,215,216,217,218,219,220,-
		 221
	      PATCODE K
		 66,67,68,70,71,72,74,75,76,77,78,80,81,82,83,84,86,87,88,89,90,-
		 98,99,100,102,103,104,106,107,108,109,110,112,113,114,115,116,-
		 118,119,120,121,122
	      PATCODE l
		 97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,-
		 114,115,116,117,118,119,120,121,122,170,186,223,224,225,226,227,-
		 228,229,230,231,232,233,234,235,236,237,238,239,241,242,243,244,-
		 245,246,247,248,249,250,251,252,253
	      PATCODE p
		 32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,58,59,60,61,62,-
		 63,64,91,92,93,94,95,96,123,124,125,126,160,161,162,163,164,165,-
		 166,167,168,169,171,172,173,174,175,176,177,178,179,180,181,182,-
		 183,184,185,187,188,189,190,191,208,222,240,254
	      PATCODE V
		 65,69,73,79,85,89,-
		 97,101,105,111,117,121
	PATEND

	Note that this table does not include a definition for pattern codes A and E.
	A is implicitly defined as the union of L and U, and E is implicitly defined as the union of all other classes.

	;  This is a test of the GT.M/I18N user-definable pattern-
	;  match table definition.
	;
	patsTaRt
			   PattaBLE Example
		PATCODE A		; WARNING: patcodes A and E cannot be re-defined
		42, 46,	43, 75 -	; comments after continuation
		, 5, -
		63, 91, 92, 93		; comments at end of directive
		PATcode u		; This is an explicit U code definition
		92, 127, 128, 255
		PaTcOdE V		; GT.M specific user-defined code
		102, 104, 109, 121
		patcode YcntY
		65, 69, 73, 79, 85	; ANSI user-defined patcode
	PaTeNd
*/

#include "mdef.h"

#include "gtm_string.h"

#ifdef VMS
#include <lnmdef.h>
#include <fab.h>
#include <rab.h>
#include <rmsdef.h>

#include "vmsdtype.h"

#else
#include "gtm_stdio.h"
#endif

#include "patcode.h"
#include "iosp.h"
#include "io.h"
#ifdef UNIX
#include "eintr_wrappers.h"
#endif
#include "util.h"
#include "trans_log_name.h"
#include "gtm_logicals.h"

#define	MAXPATNAM	256
#define	MAX_FILE	256

enum {T_EOF = 256, T_NL, T_SYNTAX, T_NUMBER, T_IDENT, K_PATSTART, K_PATEND, K_PATTABLE, K_PATCODE};

GBLREF	uint4		mapbit[];
GBLREF	uint4		pat_allmaskbits;
LITREF	uint4		typemask[PATENTS];

GBLREF	uint4		*pattern_typemask;
GBLREF	pattern		*pattern_list;
GBLREF	pattern		*curr_pattern;
GBLREF	pattern		mumps_pattern;

LITREF unsigned char lower_to_upper_table[];

static	int		pat_linenum = 0;
static	int		token;
static	unsigned char	ident[MAXPATNAM + 1];
static	int		idlen;
static	int		number, max_patents;
static	char		*ch = NULL;
static	char		patline[MAXPATNAM + 2];

#ifdef VMS
static struct FAB	fab;
static struct RAB	rab;
#else
static FILE		*patfile;
#endif

#ifdef DEBUG
void dump_tables(void);
#endif

static	void	close_patfile(void);
static	int	getaline(void);
static	int	open_patfile(int name_len, char *file_name);
static	int	pat_lex(void);
static	int	patcmp(unsigned char *str1, unsigned char *str2);
static	void	pattab_error(int name_len, char *file_name, int linenum);

static void close_patfile(void)
{
#ifdef VMS
	sys$close(&fab);
#else
	int fclose_res;
	FCLOSE(patfile, fclose_res);
#endif
}

#ifdef DEBUG
void dump_tables(void)
{
	int	mx;
	char	mout;
	pattern	**patp;

	for (patp = &pattern_list; NULL != *patp; patp = &(*patp)->flink)
	{
		util_out_print("!/Pattern Table \"!AD\":!/", TRUE, LEN_AND_STR((*patp)->name));
		for (mx = 0; mx < max_patents; mx++)
		{
			if (mx >= 32 && mx < 127)
			{
				mout = mx;
				util_out_print("!3UL:  !XL  ('!AD')!/", TRUE, mx, (*patp)->typemask[mx], 1, &mout);
			} else
			{
				util_out_print("!3UL:  !XL!/", TRUE, mx, (*patp)->typemask[mx]);
			}
		}
	}
}
#endif

static int getaline(void)
{
	int		status;

#ifdef VMS
	status = sys$get(&rab);
	if (RMS$_EOF == status)
		return 0;
	patline[rab.rab$w_rsz] = '\n';
	patline[rab.rab$w_rsz + 1] = '\0';
#else
	char		*fgets_res;
	if (NULL == FGETS(patline, SIZEOF(patline), patfile, fgets_res))
		return 0;
#endif
	return 1;
}

int getpattabnam(mstr *outname)
{
	outname->addr = curr_pattern->name;
	outname->len = curr_pattern->namlen;
	return 1;
}

int initialize_pattern_table(void)
{
	char	buffer[MAX_TRANS_NAME_LEN];
	int	status, letter;
	mstr	patname, transnam;
	static MSTR_CONST(pat_file,  PAT_FILE);
	static MSTR_CONST(pat_table, PAT_TABLE);

	/* Initialize the pattern/typemask table size. Note that in UTF-8 mode, we
	 * only use the lower half of the table (0 - 127). Although we do not extend
	 * user-defined patcodes for multi-byte characters, we still need to allow
	 * user defined patcodes for the entire ASCII charset (0 - 127) in UTF-8 mode.
	 */
	max_patents = (gtm_utf8_mode ? PATENTS_UTF8 : PATENTS);
	/* Initialize pattern/typemask structures and pat_allmaskbits for default typemask */
	curr_pattern = pattern_list = &mumps_pattern;
	pattern_typemask = mumps_pattern.typemask = (uint4 *)&(typemask[0]);
	for (pat_allmaskbits = 0, letter = 0; letter < max_patents; letter++)
		pat_allmaskbits |= pattern_typemask[letter];	/* used in do_patfixed/do_pattern */
	/* Locate default pattern file and load it. */
        status = TRANS_LOG_NAME(&pat_file, &transnam, buffer, SIZEOF(buffer), do_sendmsg_on_log2long);
	if (SS_NORMAL != status)
		return 0;
	if (!load_pattern_table(transnam.len, transnam.addr))
		return 0;
	/* Establish default pattern table. */
	status = TRANS_LOG_NAME(&pat_table,&transnam,buffer, SIZEOF(buffer), do_sendmsg_on_log2long);
	if (SS_NORMAL != status)
		return 0;
	patname.len = transnam.len;
	patname.addr = transnam.addr;
	return setpattab(&patname);
}

int load_pattern_table(int name_len,char *file_name)
{
	unsigned char	newtabnam[MAXPATNAM + 1], newYZnam[PAT_YZMAXNUM][PAT_YZMAXLEN];
	int		code, cmp, cnt, newtable[PATENTS], newnamlen, newYZlen[PAT_YZMAXNUM];
	int		newYZnum = -1;	/* number of ANSI user-defined patcodes */
	pattern		*newpat, **patp ;

	if (!open_patfile(name_len, file_name))
		return 0;
	pat_linenum = 1;
	while (T_NL == (token = pat_lex()))
		;
	if (K_PATSTART == token)
	{
		if (T_NL != (token = pat_lex()))
		{
			util_out_print("Unrecognized text at end of line", TRUE);
			pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
		}
		while (T_NL == (token = pat_lex()))
			;
		while (K_PATTABLE == token)
		{	/* Set up a pattern table record. */
			if (T_IDENT != (token = pat_lex()))
			{
				util_out_print("Identifier expected, found !AD", TRUE, idlen, ident);
				pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
			}
			newnamlen = idlen;
			memcpy(newtabnam, ident, newnamlen + 1);
			if (T_NL != (token = pat_lex()))
			{
				util_out_print("Unrecognized text at end of line", TRUE);
				pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
			}
			while (T_NL == (token = pat_lex()))
				;
			/* Process PATCODE directives */
			memset(&newtable[0], 0, max_patents * SIZEOF(newtable[0]));
			for (cnt = 0; cnt < PAT_YZMAXNUM; cnt++)
				newYZlen[cnt] = 0;
			newYZnum = -1;
			while (K_PATCODE == token)
			{
				if (T_IDENT != (token = pat_lex()))
				{
					util_out_print("Identifier expected, found !AD", TRUE, idlen, ident);
					pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
				}
				code = lower_to_upper_table[ident[0]];
				if (idlen > 1)
				{
					if (((code != 'Y') && (code != 'Z')) || (ident[0] != ident[idlen - 1]))
					{
						util_out_print("User-defined pattern code (!AD) not delimited by Y or Z",
							TRUE, idlen, ident);
						pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
					}
					if (idlen > PAT_YZMAXLEN)
					{
						util_out_print("Length of pattern code name (!AD) longer than maximum !UL",
									TRUE, idlen, ident, PAT_YZMAXLEN);
						pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
					}
					newYZnum++;
					if (newYZnum >= PAT_YZMAXNUM)
					{
						util_out_print("Number of user-defined patcodes exceeds maximum (!UL)",
													TRUE, PAT_YZMAXNUM);
						pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
					}
					newYZlen[newYZnum] = idlen;
					memcpy(newYZnam[newYZnum], ident, idlen);
					code = newYZnum + 'Y';
					util_out_print("WARNING: Pattern code !AD not yet implemented", TRUE, idlen, ident);
				} else
				{
					if (code > 'X')
					{
						util_out_print("Invalid pattern letter (!AD)", TRUE, idlen, ident);
						pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
					}
					if ((code == 'E') || (code == 'A'))
					{
						util_out_print("Attempt to redefine pattern code !AD", TRUE, idlen, ident);
						pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
					}
				}
				code = code - 'A';
				if (T_NL != (token = pat_lex()))
				{
					util_out_print("Unrecognized text at end of line", TRUE);
					pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
				}
				while (T_NL == (token = pat_lex()))
					;
				/* Process character list setting the code's flag into the typemask */
				if (T_NUMBER == token)
				{
					if (number >= max_patents)
					{
						util_out_print("Character code greater than !UL encountered (!UL)",
							TRUE, max_patents - 1, number);
						pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
					}
					newtable[number] |= mapbit[code];
					while (',' == (token = pat_lex()))
					{
						if (T_NUMBER != (token = pat_lex()))
						{
							util_out_print("Numeric character code expected, found !AD",
								TRUE, idlen, ident);
							pattab_error(name_len, file_name, pat_linenum); /* error does not return */
						}
						if (number >= max_patents)
						{
							util_out_print("Character code greater than !UL encountered (!UL)",
								TRUE, max_patents - 1, number);
							pattab_error(name_len, file_name, pat_linenum); /* error does not return */
						}
						newtable[number] |= mapbit[code];
					}
					if (T_NL != token)
					{
						util_out_print("Unrecognized text at end of line", TRUE);
						pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
					}
					while (T_NL == (token = pat_lex()))
						;
				}
			}
			for (patp = &pattern_list; (*patp) ; patp = &(*patp)->flink)
			{
				cmp = patcmp(newtabnam, (uchar_ptr_t)(*patp)->name);
				if (0 == cmp)
				{	/* don't read in same table name twice */
					util_out_print("Cannot load table !AD twice", TRUE, newnamlen, newtabnam);
					pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
				} else if (cmp < 0)
					break;
			}
			newpat = (pattern *) malloc(SIZEOF(pattern) + newnamlen);
			newpat->flink = (*patp);
			newpat->namlen = newnamlen;
			memcpy(newpat->name, newtabnam, newnamlen + 1);
			newpat->typemask = (uint4 *) malloc(max_patents * SIZEOF(typemask[0]));
			memcpy(newpat->typemask, newtable, max_patents * SIZEOF(typemask[0]));
			newpat->patYZnam = (unsigned char *) malloc(SIZEOF(newYZnam));
			memcpy(newpat->patYZnam, newYZnam, SIZEOF(newYZnam));
			newpat->patYZlen = (int *) malloc(SIZEOF(newYZlen));
			memcpy(newpat->patYZlen, newYZlen, SIZEOF(newYZlen));
			newpat->patYZnum = newYZnum;
			(*patp) = newpat;
		}
		if (K_PATEND != token)
		{
			util_out_print("End of definition marker (PATEND) expected", TRUE);
			pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
		}
		while (T_NL == (token = pat_lex()))
			;
		if (T_EOF != token)
		{
			util_out_print("Unrecognized text following end of definitions", TRUE);
			pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */
		}
		close_patfile();
		return 1;
	} else
		pattab_error(name_len, file_name, pat_linenum); /* error trap does not return */

	return -1; /* This will never get executed, added to make compiler happy */
}

static int open_patfile(int name_len, char *file_name)
{
	int		status;
	unsigned char	*name_copy;

#	ifdef VMS
	fab = cc$rms_fab;
	fab.fab$l_fna = file_name;
	fab.fab$b_fns = name_len;
	status = sys$open(&fab);
	if (!(status & 1))
		return 0;
	rab = cc$rms_rab;
	rab.rab$l_fab = &fab;
	rab.rab$l_ubf = patline;
	rab.rab$w_usz = SIZEOF(patline);
	status = sys$connect(&rab);
	if (RMS$_NORMAL != status)
		return 0;
#	else
	name_copy = malloc(name_len + 1);
	memcpy(name_copy, file_name, name_len);
	name_copy[name_len] = '\0';
	patfile = Fopen((const char *)name_copy, "r");
	free(name_copy);
	if (NULL == patfile)
		return 0;
#	endif
	if (getaline())
		ch = patline;
	return 1;
}

static int pat_lex(void)
{
	int	continuation = 0;
	char	*id;

	if (NULL == ch)
		return T_EOF;	/* EOF already seen */

	/* process whitespace */
skip_whitespace:
	while ((' ' >= *ch) || (';' == *ch))
	{
		if (('\n' == *ch) || (';' == *ch))
		{
			ch = getaline() ? patline : NULL;
			pat_linenum++;
			if (!continuation)
				return T_NL;
			continuation = 0;
		} else
			ch++;
	}
	if (continuation)
		return continuation;
	/* process lexeme */
	switch (*ch) {
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
	case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
	case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
	case 'Y': case 'Z':
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
	case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
	case 's': case 't': case 'u': case 'v': case 'w': case 'x':
	case 'y': case 'z':
		id = (char *)ident;
		idlen = 0;
		do {
			*id++ = *ch++;
			idlen++;
		} while (typemask[*ch] & (PATM_A | PATM_N));
		*id++ = '\0';
		if (patcmp(ident, (uchar_ptr_t)"PATCODE") == 0)
			return K_PATCODE;
		if (patcmp(ident, (uchar_ptr_t)"PATTABLE") == 0)
			return K_PATTABLE;
		if (patcmp(ident, (uchar_ptr_t)"PATSTART") == 0)
			return K_PATSTART;
		if (patcmp(ident, (uchar_ptr_t)"PATEND") == 0)
			return K_PATEND;
		return T_IDENT;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		number = *ch++ - '0';
		while (typemask[*ch] & PATM_N)
			number = 10 * number + *ch++ - '0';
		return T_NUMBER;
		break;
	case '-':
		continuation = '-';
		ch++;
		goto skip_whitespace;
	default:
		return *ch++;
	}
}

static int patcmp(unsigned char *str1,unsigned char *str2)
{
	int		cmp;

	while ('\0' != *str2)
	{
		cmp = lower_to_upper_table[*str1++] - lower_to_upper_table[*str2++];
		if (0 != cmp)
			return cmp;
	}
	return *str1;
}

static void pattab_error(int name_len,char *file_name,int linenum)
{
	error_def	(ERR_PATTABSYNTAX);

	close_patfile();
	rts_error(VARLSTCNT(5) ERR_PATTABSYNTAX, 3, name_len, file_name, linenum);
}

int setpattab(mstr *table_name)
{
	int		letter;
	pattern		**patp;
	unsigned char	ptnam[MAXPATNAM + 1];

	if (table_name->len <= MAXPATNAM)
	{	/* null-terminate the pattern table name. */
		if (table_name->len)
		{
			memcpy(ptnam, table_name->addr, table_name->len);
			ptnam[table_name->len] = '\0';
		} else
		{	/* Default table name */
			ptnam[0] = 'M';
			ptnam[1] = 0;
		}
		for (patp = &pattern_list; NULL != *patp; patp = &(*patp)->flink)
		{
			if (0 == patcmp(ptnam, (unsigned char *)((*patp)->name)))
			{
				pattern_typemask = (*patp)->typemask;
				curr_pattern = (*patp);
				/* reset pat_allmaskbits to correspond to the currently active pattern_typemask */
				for (pat_allmaskbits = 0, letter = 0; letter < max_patents; letter++)
					pat_allmaskbits |= pattern_typemask[letter];
				return TRUE;
			}
		}
	}
	return FALSE;
}
