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

#ifndef PATCODE_H_DEFINED
#define PATCODE_H_DEFINED
/* pattern operator constants */

#define PATM_N			(1 << 0)
#define PATM_P			(1 << 1)
#define PATM_L			(1 << 2)
#define PATM_U			(1 << 3)
#define PATM_C			(1 << 4)
#define PATM_UTF8_ALPHABET	(1 << 5) /* Unicode characters that are alphabets but not cased (i.e. neither L nor U) */
#define PATM_UTF8_NONBASIC	(1 << 6) /* Currently not used, but retained in case needed in the future */

#define	PAT_BASIC_CLASSES	7	/* the 5 canonic classes (defined by the M standard) plus the two PATM_UTF8_* classes */

#define PATM_STRLIT		(1 << 7)

#define PATM_A (PATM_L | PATM_U | PATM_UTF8_ALPHABET)
#define PATM_E 0x1FFFF7F	/* E matches **any** pattern code... */

/* Additional user-definable codes for Internationalization */
/*      PATM_A              already used for Alphabetic characters */
#define PATM_B (1 <<  8)
/*      PATM_C              already used for Control characters */
#define PATM_D (1 <<  9)
/*      PATM_E              already used for Entire set of characters */
#define PATM_F (1 << 10)
#define PATM_G (1 << 11)
#define PATM_H (1 << 12)
#define PATM_I (1 << 13)
#define PATM_J (1 << 14)
#define PATM_K (1 << 15)
/*      PATM_L              already used for Lower-case characters */
#define PATM_M (1 << 16)
/*      PATM_N              already used for Numeric characters */
#define PATM_O (1 << 17)
/*      PATM_P              already used for Punctuation characters */
#define PATM_Q (1 << 18)
#define PATM_R (1 << 19)
#define PATM_S (1 << 20)
#define PATM_T (1 << 21)
/*      PATM_U              already used for Upper-case characters */
#define PATM_V (1 << 22)
#define PATM_W (1 << 23)
#define PATM_X (1 << 24)
/*      PATM_Y              reserved for YanythingY extension */
/*      PATM_Z              reserved for ZanythingZ extension */

#define PATM_ALT (1 << 25)

#define PATM_YZ1 (1 << 26)
#define PATM_YZ2 (1 << 27)
#define PATM_YZ3 (1 << 28)
#define PATM_YZ4 (1 << 29)

#define DFABIT	(1 << 30)
#define	PAT_MAX_BITS	32

#define PAT_YZMAXNUM	4	/* maximum number of YxxxY and ZxxxZ pattern codes */
#define PAT_YZMAXLEN	8	/* maximum number of characters in name of YxxxY pattern code */

/* PATM_UTF8_ALPHABET and PATM_UTF8_NONBASIC are assigned an external name of 0 and 1 below as we have run out of alphabets */
#define PATM_CODELIST	"NPLUCBDFGHIJKMOQRSTVWX01"
#define PATM_DFA	254		/* Deterministic Finite Automaton */
#define PATM_ACS	255
#define PATM_SHORTFLAGS	0x7F		/* original 5 flags + the two recently introduced PATM_UTF8_* flags */
#define PATM_LONGFLAGS	0x1FFFF7F	/* 24 flags (including the two recently introduced PATM_UTF8_* flags) */
#define PATM_I18NFLAGS	0x1FFFF00	/* flags introduced with Internationalization */

#define MAX_DFA_SPACE 170

#define PAT_MAX_REPEAT		MAX_STRLEN
#define MAX_PATTERN_ATOMS	50
#define	MAX_PATOBJ_LENGTH	2048
#define	MAX_PATTERN_OVERHEAD	((3 * MAX_PATTERN_ATOMS) + 3) * 4/* maximum length (in integers) of compiled pattern code
								  * excluding count,tot_min,tot_max and min,max,size arrays
								  * that come at the tail. 3 * MAX_PATTERN_ATOMS is for
								  * min, max, size arrays and 3 is for count, tot_min, tot_max
								  * 4 is to convert the size into bytes.
								  */
#define MAX_PATTERN_LENGTH	(MAX_PATOBJ_LENGTH - MAX_PATTERN_OVERHEAD)
#define PATENTS		256	/* Size of the builtin pattern/typemask table in M mode */
#define PATENTS_UTF8	128	/* Size of the builtin pattern/typemask table in UTF-8 mode */
#define CHAR_CLASSES 24
#define	PAT_STRLIT_PADDING	3	/* # of int4s for storing bytelen, charlen, flags in PATM_STRLIT type */

#define MAX_SYM	16
#define FST	0
#define LST	1

#define MAX_DFA_STRLEN 64	/* needs to be > PAT_BASIC_CLASSES */
#define MAX_DFA_REP    10

/* The macro to perform 64-bit multiplication without losing precision due to overflow with 32-bit
 * multiplication. With the increase of PAT_MAX_REPEAT from 32K to 1MB, the values of result and
 * the expression value can potentially be as large as (1MB * 1MB).
 * NOTE: The macro expects result to be declared with type gtm_uint64_t */
#define BOUND_MULTIPLY(x, y, result) 	(((result = ((gtm_uint64_t)(x) * (y))) >= PAT_MAX_REPEAT) ? PAT_MAX_REPEAT : (int)(result))

/*  Compiled Pattern
 *  -----------------
 *	0x00000000	<-- fixed (1 if fixed length, 0 if not fixed length)
 *	0x00000027	<-- length of pattern stream (inclusive of itself)
 *	0x00000008	<-- begin of pattern mask
 *	...
 *	0x00000002	<-- count
 *	0x00000001	<-- total_min
 *	0x00007fff	<-- total_max
 *	0x00000000	<-- min[0]	<-- Begin of min[2] array
 *	0x00000001	<-- min[1]
 *	0x00007fff	<-- max[0]	<-- Begin of max[2] array
 *	0x00000001	<-- max[1]
 *	0x00000001	<-- size[0]	<-- Begin of size[2] array
 *	0x00000001	<-- size[1]
 */
#define	PAT_LEN_OFFSET		1		/* relative to beginning of compiled pattern which starts with "fixed" */
#define	PAT_MASK_BEGIN_OFFSET	2		/* start of actual stream of pattern masks */
#define PAT_COUNT_OFFSET(len)	(1 + len)	/* including "fixed", "len" (of pattern stream) */
#define	PAT_TOT_MIN_OFFSET(len)	(1 + len + 1)	/* including "fixed", "len" (of pattern stream) and "count" */
#define PAT_TOT_MAX_OFFSET(len)	(1 + len + 2)	/* including "fixed", "len" (of pattern stream), "count", "tot_min" */

/* Note that if fixed length pattern, max[0] is same as min[0], hence the (1 - fixed) calculations below */
#define	PAT_MIN_BEGIN_OFFSET(fixed,len,count)	(1 + len + 3)					/* offset of min[0] */
#define	PAT_MAX_BEGIN_OFFSET(fixed,len,count)	(1 + len + 3 + (count * (1 - fixed)))		/* offset of max[0] */
#define	PAT_SIZE_BEGIN_OFFSET(fixed,len,count)	(PAT_MAX_BEGIN_OFFSET(fixed,len,count) + count)	/* offset of size[0] */

typedef struct pattern_struct
{
	struct pattern_struct	*flink;
	uint4			*typemask;
	unsigned char		*patYZnam;
	int			*patYZlen;
	int			patYZnum;
	int			namlen;
	char			name[2]; /* must be last entity in structure */
} pattern;

struct leaf
{
	boolean_t	nullable[MAX_SYM];
	int4		letter[MAX_SYM][MAX_DFA_STRLEN + 1];
};

struct node
{
	boolean_t	nullable[MAX_SYM];
	boolean_t	last[MAX_SYM][MAX_SYM];
};

struct e_table
{
	int	meta_c[CHAR_CLASSES][26],
		num_e[CHAR_CLASSES];
};

struct c_trns_tb
{
	int	c[2 * MAX_SYM];
	int4	p_msk[2 * MAX_SYM][CHAR_CLASSES];
	int	trns[2 * MAX_SYM][CHAR_CLASSES];
};

typedef struct ptstr_struct {
	int4	len;
	uint4	buff[MAX_PATOBJ_LENGTH];
} ptstr;

/* The following flags are the attributes of the pattern string literal (PATM_STRLIT) */
#define PATM_STRLIT_NONASCII	(1 << 0) /* whether the string contains non-ASCII (multi-byte) characters */
#define PATM_STRLIT_BADCHAR	(1 << 1) /* whether the string contains malformed UTF8 byte sequences */

typedef struct patstrlit_struct
{
	int4		bytelen;	/* number of bytes in the buffer */
	int4		charlen;	/* number of characters (single or multi-byte) in the PATM_STRLIT */
	uint4		flags;		/* various attributes about the pattern string literal */
	unsigned char	buff[MAX_PATTERN_LENGTH - 2];
} pat_strlit;

/* The following structure caches the evaluation status of whether a pattern string matches a given <strptr, strlen>.
 * This remembering enables us to avoid recomputation (and hence a lot of CPU cycles) in case the same need arises again.
 * Least frequently used pte_csh structures (i.e. having the smallest "count" field) will be preempted for new entries.
 * More comments in gbldefs.c above the pte_csh* structure GBLDEFs.
 */
typedef struct pte_csh_struct {
	char		*patptr;
	char		*strptr;
	int4		charlen;
	int		repcnt;	/* recursion level */
	uint4		count;	/* indicates frequency of access. The least "count" valued entry gets pre-empted */
	boolean_t	match;
} pte_csh;

#define	PTE_BEGIN_ENTRIES	1024	/* initial pte_csh_array size. if too high, causes us initializing overhead.
					 * if too less, doesn't maintain most frequently accessed patterns.
					 */
#define	PTE_MAX_ENTRIES		16384	/* maximum pte_csh array size */
#define	PTE_STRLEN_CUTOFF	48	/* strlen value >= PTE_STRLEN_CUTOFF use up the tail of the pt_csh array */
#define	PTE_FILL_RATIO_NUM	3	/* fill ratio is percentage of pte_csh_array that has 1-1 correspondence with "strlen" */
#define	PTE_FILL_RATIO_DEN	4	/* this is expressed as the fraction PTE_FILL_RATIO_NUM/PTE_FILL_RATIO_DEN */

#define	PTE_NOT_FOUND		-1

#define	MIN_SPLIT_N_MATCH_COUNT	3	/* need at least 3 pattern atoms to try splitting the pattern and matching subpatterns */
#define	DO_PATSPLIT_FAIL	-1	/* attempt to split (at fixed pattern) and match left and right subpatterns failed */

#define	PTE_CSH_MISS_FACTOR	8	/* i.e. we allow 1/8 to be the maximum cache miss percent */
#define	PTE_MAX_CURALT_DEPTH	2	/* max. number of levels of alternation nesting for which we maintain a pte_csh array */

#define	UPDATE_CUR_PTE_CSH_MINUS_ARRAY(cur_pte_csh_size, cur_pte_csh_entries_per_len, cur_pte_csh_tail_count)	\
{														\
	cur_pte_csh_size = pte_csh_cur_size[curalt_depth];							\
	cur_pte_csh_entries_per_len = pte_csh_entries_per_len[curalt_depth];					\
	cur_pte_csh_tail_count = pte_csh_tail_count[curalt_depth];						\
}

#define	UPDATE_CUR_PTE_CSH(cur_pte_csh_array, cur_pte_csh_size, cur_pte_csh_entries_per_len, cur_pte_csh_tail_count)	\
{															\
	cur_pte_csh_array = pte_csh_array[curalt_depth];								\
	UPDATE_CUR_PTE_CSH_MINUS_ARRAY(cur_pte_csh_size, cur_pte_csh_entries_per_len, cur_pte_csh_tail_count)		\
}

#define	PTE_CSH_INCR_CURALT_DEPTH(curalt_depth)											\
{																\
	curalt_depth++;														\
	assert(0 <= curalt_depth);												\
	if (PTE_MAX_CURALT_DEPTH > curalt_depth)										\
	{															\
		if (NULL == pte_csh_array[curalt_depth])									\
		{														\
			pte_csh_array[curalt_depth] = malloc(SIZEOF(pte_csh) * PTE_BEGIN_ENTRIES);				\
			pte_csh_alloc_size[curalt_depth] = PTE_BEGIN_ENTRIES;							\
		}														\
		pte_csh_entries_per_len[curalt_depth] = ((PTE_BEGIN_ENTRIES / PTE_FILL_RATIO_DEN) * PTE_FILL_RATIO_NUM)		\
										/ PTE_STRLEN_CUTOFF;				\
		pte_csh_tail_count[curalt_depth] = (PTE_BEGIN_ENTRIES / PTE_FILL_RATIO_DEN) *					\
								(PTE_FILL_RATIO_DEN - PTE_FILL_RATIO_NUM);			\
		pte_csh_cur_size[curalt_depth] = PTE_BEGIN_ENTRIES;								\
		assert(pte_csh_cur_size[curalt_depth] <= pte_csh_alloc_size[curalt_depth]);					\
		memset(pte_csh_array[curalt_depth], 0, SIZEOF(pte_csh) * PTE_BEGIN_ENTRIES);					\
		/* reset cur_pte_csh* globals to point to new curalt_depth */							\
		UPDATE_CUR_PTE_CSH(cur_pte_csh_array, cur_pte_csh_size, cur_pte_csh_entries_per_len, cur_pte_csh_tail_count); 	\
		do_patalt_hits[curalt_depth] = do_patalt_calls[curalt_depth] = 1;						\
	}															\
}

#define	PTE_CSH_DECR_CURALT_DEPTH(curalt_depth)											\
{																\
	assert(0 <= curalt_depth);												\
	curalt_depth--;														\
	if ((PTE_MAX_CURALT_DEPTH > curalt_depth) && (0 <= curalt_depth))							\
	{	/* reset cur_pte_csh* globals to point to new curalt_depth */							\
		UPDATE_CUR_PTE_CSH(cur_pte_csh_array, cur_pte_csh_size, cur_pte_csh_entries_per_len, cur_pte_csh_tail_count);	\
	}															\
}

int	do_patalt(
		uint4		*firstalt,
		unsigned char	*strptr,
		unsigned char	*strtop,
		int4		repmin,
		int4		repmax,
		int		totchar,
		int		repcnt,
		int4		min_incr,
		int4		max_incr);

int	do_patfixed(mval *str, mval *pat);
int	do_pattern(mval *str, mval *pat);
int	do_patsplit(mval *str, mval *pat);
void	genpat(mstr *input, mval *patbuf);
int	getpattabnam(mstr *outname);
int	initialize_pattern_table(void);
int	load_pattern_table(int name_len, char *file_name);
int	patmaskseq(uint4 number);
int	setpattab(mstr *table_name);

int	patstr(mstr *instr, ptstr *obj, unsigned char **relay);

int	dfa_calc(
		struct leaf	*leaves,
		int		leaf_num,
		struct e_table	*expand,
		uint4 		**fstchar_ptr,
		uint4 		**outchar_ptr);

boolean_t pat_unwind(
		int		*count,
		struct leaf	*leaves,
		int		leaf_num,
		int		*total_min,
		int		*total_max,
		int		min[],
		int		max[],
		int		size[],
		int		altmin,
		int		altmax,
		boolean_t	*last_infinite_ptr,
		uint4		**fstchar_ptr,
		uint4		**outchar_ptr,
		uint4		**lastpatptr_ptr);

boolean_t add_atom(
		int		*count,
		uint4		pattern_mask,
		pat_strlit	*strlit_buff,
		boolean_t	infinite,
		int		*min,
		int		*max,
		int		*size,
		int		*total_min,
		int		*total_max,
		int		lower_bound,
		int		upper_bound,
		int		altmin,
		int		altmax,
		boolean_t	*last_infinite_ptr,
		uint4		**fstchar_ptr,
		uint4		**outchar_ptr,
		uint4		**lastpatptr_ptr);

int pat_compress(
		uint4		pattern_mask,
		pat_strlit	*strlit_buff,
		boolean_t	infinite,
		boolean_t	last_infinite,
		uint4		*lastpatptr);
#endif
