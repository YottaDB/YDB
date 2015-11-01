/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

	;  This is a test of the GT.M/I18N user-definable pattern-
	;  match table definition.
	;
	patsTaRt
			   PattaBLE Example
		PATCODE A
		42, 46,	43, 75 -	; comments after continuation
		, 5, -
		63, 91, 92, 93		; comments at end of directive
		PATcode e		; This is an explicit E code defn
		92, 127, 128, 255
	PaTeNd
*/

#include "mdef.h"

#ifdef VMS
/* #include <descrip.h> */
#include <lnmdef.h>
/* #include <ssdef.h> */
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


#define	MAXPATNAM	256

#ifndef NULL
#	define NULL		((void *) 0)
#endif

#define	MAX_FILE	256

#ifdef VMS
#define PAT_FILE	"GTM_PATTERN_FILE"
#define	PAT_TABLE	"GTM_PATTERN_TABLE"
#else
#define PAT_FILE	"$gtm_pattern_file"
#define	PAT_TABLE	"$gtm_pattern_table"

#endif

enum {T_EOF = 256, T_NL, T_SYNTAX, T_NUMBER, T_IDENT, K_PATSTART, K_PATEND, K_PATTABLE, K_PATCODE};

LITREF uint4 typemask[PATENTS];

/* This table holds the current pattern-matching attributes of each
   ASCII character.  Bits 0..21 of each entry correspond with the
   pattern-match characters, A..U.
*/
static int		pat_linenum = 0;
GBLDEF uint4		*pattern_typemask;

/* Standard MUMPS pattern-match table.
*/
GBLDEF pattern mumps_pattern = {
	(void *) 0,		/* flink */
	(void *) 0,		/* typemask */
	1,			/* namlen */
	{'M', '\0'}		/* name */
};

static readonly uint4	mapbit[] =
{	PATM_A, PATM_B, PATM_C, PATM_D, PATM_E, PATM_F, PATM_G, PATM_H,
	PATM_I, PATM_J, PATM_K, PATM_L, PATM_M, PATM_N, PATM_O, PATM_P,
	PATM_Q, PATM_R, PATM_S, PATM_T, PATM_U
};

GBLDEF pattern *pattern_list;
GBLDEF pattern *curr_pattern;

#ifdef VMS
static struct FAB	fab;
static struct RAB	rab;
#else
static FILE		*patfile;
#endif

static int		token;
static unsigned char	ident[MAXPATNAM + 1];
static int		idlen;
static int		number;

static char		*ch = NULL;
static char		patline[MAXPATNAM + 2];

LITREF unsigned char lower_to_upper_table[];

#ifdef DEBUG
void dump_tables(void);
#endif

static void close_patfile(void);
static int getaline(void);
static int open_patfile(int name_len,char *file_name);
static int pat_lex(void);
static int patcmp(unsigned char *str1, unsigned char *str2);
static void pattab_error(int name_len,char *file_name,int linenum);


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

	for (patp = &pattern_list; *patp != NULL; patp = &(*patp)->flink)
	{
		util_out_print("!/Pattern Table \"!AD\":!/", TRUE, LEN_AND_STR((*patp)->name));
		for (mx = 0; mx < PATENTS; mx++)
		{
			if (mx >= 32 && mx < 127)
			{
				mout = mx;
				util_out_print("!3UL:  !8XL  ('!AD')!/", TRUE, mx, (*patp)->typemask[mx], 1, &mout);
			} else
			{
				util_out_print("!3UL:  !8XL!/", TRUE, mx, (*patp)->typemask[mx]);
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
	if (status == RMS$_EOF) return 0;
	patline[rab.rab$w_rsz] = '\n';
	patline[rab.rab$w_rsz + 1] = '\0';
#else
	char		*fgets_res;
	if (FGETS(patline, sizeof(patline), patfile, fgets_res) == NULL) return 0;
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
	char			buffer[MAX_TRANS_NAME_LEN];
	int			status;
	static MSTR_CONST(pat_file,	PAT_FILE);
	static MSTR_CONST(pat_table,	PAT_TABLE);
	mstr			transnam;
	mstr			patname;
	int			result;

	/* Initialize pattern/typemask structures. */
	curr_pattern = pattern_list = &mumps_pattern;
	pattern_typemask = mumps_pattern.typemask = &(typemask[0]);

	/* Locate default pattern file and load it. */
        status = trans_log_name(&pat_file,&transnam,buffer);
	if (status != SS_NORMAL) return 0;
	if (!load_pattern_table(transnam.len, transnam.addr)) return 0;

	/* Establish default pattern table. */
	status = trans_log_name(&pat_table,&transnam,buffer);

	if (status != SS_NORMAL) return 0;
	patname.len = transnam.len;
	patname.addr = transnam.addr;
	setpattab(&patname, &result);
	return result;
}

int load_pattern_table(int name_len,char *file_name)
{
	int		code, cmp;
	int		mx, newtable[PATENTS], newnamlen;
	unsigned char	newtabnam[MAXPATNAM + 1];
	pattern		*newpat, **patp ;
	error_def	(ERR_ILLPATCODEREDEF);

	if (!open_patfile(name_len, file_name)) return 0;
	pat_linenum = 1;
	while ((token = pat_lex()) == T_NL);
	if (token == K_PATSTART)
	{
		if ((token = pat_lex()) != T_NL)
		{
			pattab_error(name_len, file_name, pat_linenum);
			return 0;
		}
		while ((token = pat_lex()) == T_NL);
		while (token == K_PATTABLE)
		{
			/* Set up a pattern table record. */
			if ((token = pat_lex()) != T_IDENT)
			{
				pattab_error(name_len, file_name, pat_linenum);
				return 0;
			}
			newnamlen = idlen;
			memcpy(newtabnam, ident, newnamlen + 1);
			if ((token = pat_lex()) != T_NL)
			{
				pattab_error(name_len, file_name, pat_linenum);
				return 0;
			}
			while ((token = pat_lex()) == T_NL);

			/* Process PATCODE directives */
			memset(&newtable[0], 0, sizeof(newtable));
			while (token == K_PATCODE)
			{
				if ((token = pat_lex()) != T_IDENT || idlen != 1)
				{
					pattab_error(name_len, file_name, pat_linenum);
					return 0;
				}
				code = lower_to_upper_table[ident[0]] - 'A';
				if (code > ('U' - 'A'))
				{
					pattab_error(name_len, file_name, pat_linenum);
					return 0;
				}
				if (code == ('E' - 'A') || code == ('A' - 'A'))
				{
					rts_error(VARLSTCNT(4) ERR_ILLPATCODEREDEF, 2, name_len, file_name);
					close_patfile();
					return 0;
				}
				if ((token = pat_lex()) != T_NL)
				{
					pattab_error(name_len, file_name, pat_linenum);
					return 0;
				}
				while ((token = pat_lex()) == T_NL);

				/* Process character list setting the code's flag into the typemask */
				if (token == T_NUMBER)
				{
					if (number >= PATENTS)
					{
						pattab_error(name_len, file_name, pat_linenum);
						return 0;
					}
					newtable[number] |= mapbit[code];
					while ((token = pat_lex()) == ',')
					{
						if ((token = pat_lex()) != T_NUMBER)
						{
							pattab_error(name_len, file_name, pat_linenum);
							return 0;
						}
						if (number >= PATENTS)
						{
							pattab_error(name_len, file_name, pat_linenum);
							return 0;
						}
						newtable[number] |= mapbit[code];
					}
					if (token != T_NL)
					{
						pattab_error(name_len, file_name, pat_linenum);
						return 0;
					}
					while ((token = pat_lex()) == T_NL);
				}
			}

			for (patp = &pattern_list; (*patp) ; patp = &(*patp)->flink)
			{	cmp = patcmp(newtabnam, (uchar_ptr_t)(*patp)->name);
				if (cmp == 0)
				{		/* don't read in same table name twice */
					pattab_error(name_len, file_name, pat_linenum);
					return 0;
				}
				else if (cmp < 0)
				{	break;
				}
			}
			newpat = (pattern *) malloc(sizeof(pattern) + newnamlen);
			newpat->flink = (*patp);
			newpat->namlen = newnamlen;
			memcpy(newpat->name, newtabnam, newnamlen + 1);
			newpat->typemask = (uint4 *) malloc(sizeof(typemask));
			memcpy(newpat->typemask, newtable, sizeof(typemask));
			(*patp) = newpat;
		}
		if (token != K_PATEND)
		{
			pattab_error(name_len, file_name, pat_linenum);
			return 0;
		}
		while ((token = pat_lex()) == T_NL);
		if (token != T_EOF)
		{
			pattab_error(name_len, file_name, pat_linenum);
			return 0;
		}
		close_patfile();
		return 1;
	}
	else
	{
		pattab_error(name_len, file_name, pat_linenum);
		return 0;
	}
}

static int open_patfile(int name_len,char *file_name)
{
	int	status;

#ifdef VMS
	fab = cc$rms_fab;
	fab.fab$l_fna = file_name;
	fab.fab$b_fns = name_len;
	status = sys$open(&fab);
	if (!(status & 1)) return 0;
	rab = cc$rms_rab;
	rab.rab$l_fab = &fab;
	rab.rab$l_ubf = patline;
	rab.rab$w_usz = sizeof(patline);
	status = sys$connect(&rab);
	if (status != RMS$_NORMAL) return 0;
#else
	patfile = Fopen(file_name, "r");
	if (patfile == NULL) return 0;
#endif

	if (getaline()) ch = patline;
	return 1;
}

static int pat_lex(void)
{
	int	continuation = 0;
	char	*id;

	/* Has EOF already been encountered?
	*/
	if (ch == NULL) return T_EOF;

	/* Process whitespace.
	*/
skip_whitespace:
	while (*ch <= ' ' || *ch == ';')
	{
		if (*ch == '\n' || *ch == ';')
		{
			if (!getaline()) ch = NULL;
			else ch = patline;
			pat_linenum++;
			if (!continuation) return T_NL;
			continuation = 0;
		} else ch++;
	}

	if (continuation) return continuation;

	/* Process lexeme.
	*/
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
		if (patcmp(ident, (uchar_ptr_t)"PATCODE") == 0) return K_PATCODE;
		if (patcmp(ident, (uchar_ptr_t)"PATTABLE") == 0) return K_PATTABLE;
		if (patcmp(ident, (uchar_ptr_t)"PATSTART") == 0) return K_PATSTART;
		if (patcmp(ident, (uchar_ptr_t)"PATEND") == 0) return K_PATEND;
		return T_IDENT;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		number = *ch++ - '0';
		while (typemask[*ch] & PATM_N)
		{
			number = 10 * number + *ch++ - '0';
		}
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

	while (*str2 != '\0')
	{
		cmp = lower_to_upper_table[*str1++] - lower_to_upper_table[*str2++];
		if (cmp != 0) return cmp;
	}
	return *str1;
}

static void pattab_error(int name_len,char *file_name,int linenum)
{
	error_def	(ERR_PATTABSYNTAX);

	rts_error(VARLSTCNT(5) ERR_PATTABSYNTAX, 3, name_len, file_name, linenum);
}

void setpattab(mstr *table_name,int *result)
{
	int		mx;
	pattern		**patp;
	unsigned char	ptnam[MAXPATNAM + 1];

	/* Set to No match at start. */
	*result = 0;

	if (table_name->len <= MAXPATNAM)
	{
		/* Null-terminate the pattern table name. */
		if (table_name->len)
		{	memcpy(ptnam, table_name->addr, table_name->len);
			ptnam[table_name->len] = '\0';
		}
		else
		{	/* Default table name */
			ptnam[0] = 'M';
			ptnam[1] = 0;
		}

		for (patp = &pattern_list; *patp != NULL; patp = &(*patp)->flink)
		{
			if (patcmp(ptnam, (unsigned char *)((*patp)->name)) == 0)
			{
				pattern_typemask = (*patp)->typemask;
				curr_pattern = (*patp);
				*result = 1;
				break;
			}
		}
	}
}
