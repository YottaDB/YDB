/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"
#include "gtm_string.h"
#include "cli.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "gtm_trigger.h"
#include "trigger.h"
#include "trigger_parse_protos.h"
#include "trigger_scan_string.h"
#include "zshow.h"		/* for zwr2format() prototype */
#include "min_max.h"
#include "util.h"
#include "subscript.h"
#include "cli_parse.h"
#include "compiler.h"
#include "gtm_utf8.h"
#include "gtmctype.h"

GBLREF CLI_ENTRY                *cmd_ary;
GBLREF	gd_region		*gv_cur_region;
GBLREF CLI_ENTRY                trigger_cmd_ary[];

#define BITS_PER_INT		(SIZEOF(uint4) * 8)	/* Number of bits in an integer */
#define MAX_PIECE_VALUE		(BITS_PER_INT * 1024)	/* Largest value allowed in -pieces string */
#define MAX_PIECE_INT		(MAX_PIECE_VALUE / 32)	/* Number of integers it takes to hold MAX_PIECE_VALUE bits */
#define	MAX_PIECE_CHARS		(MAX_PIECE_INT * 4)	/* Number of 8-bit bytes in MAX_PIECE_INT integers */
#define MAX_LVN_COUNT		MAX_GVSUBSCRIPTS	/* Maximum number of "lvn=" in trigger subscript */
#define MAX_OPTIONS_LEN		1024			/* Maximum size of the "options" string */
#define MAX_DCHAR_LEN		1024			/* Maximum size of $C or $ZCH string */
#define MAX_DELIM_LEN		1024			/* Maximum size of the string for a delimiter - $C, $ZCH, "x", ... */
#define MAX_ERROR_MSG_LEN	72
#define TRIGR_PRECOMP_RTNNAME	"trigcomptest#"

#define CONV_CH(PTR, STR, LEN)								\
{											\
	if (PRINTABLE(*PTR))								\
	{										\
		STR[0] = '"';								\
		STR[1] = *PTR;								\
		STR[2] = '"';								\
		LEN = 3;								\
	} else										\
		format2zwr((sm_uc_ptr_t)PTR, 1, STR, &LEN);				\
}

#define ERROR_MSG_RETURN(STR, LEN, PTR)							\
{											\
	CONV_STR_AND_PRINT(STR, LEN, PTR);						\
	return FALSE;									\
}

#define ERROR_STR_RETURN(STR)								\
{											\
	util_out_print_gtmio(STR, FLUSH);						\
	return FALSE;									\
}

#define CONV_CH_AND_PRINT(STR, PTR)							\
{											\
	int		out_len;							\
	unsigned char	out_str[64];		/* Plenty of room for $ZCH(xxx) */ 	\
											\
	CONV_CH(PTR, out_str, out_len);							\
	util_out_print_gtmio(STR, FLUSH, out_len, out_str);				\
}

#define CONV_CH_AND_STR_AND_PRINT(STR, PTR1, LEN, PTR2)					\
{											\
	int		out_len1, out_len2;						\
	unsigned char	out_str1[64];		/* Plenty of room for $ZCH(xxx) */ 	\
	unsigned char	out_str2[MAX_ZWR_EXP_RATIO * OUT_BUFF_SIZE];			\
											\
	CONV_CH(PTR1, out_str1, out_len1);						\
	CONV_TO_ZWR(LEN, PTR2, out_len2, out_str2);					\
	util_out_print_gtmio(STR, FLUSH, out_len1, out_str1, out_len2, out_str2);	\
}

#define CONV_NUM_AND_STR_AND_PRINT(STR, NUM, LEN, PTR)					\
{											\
	int		out_len;							\
	unsigned char	out_str[MAX_ZWR_EXP_RATIO * OUT_BUFF_SIZE];			\
											\
	CONV_TO_ZWR(LEN, PTR, out_len, out_str);					\
	util_out_print_gtmio(STR, FLUSH, NUM, out_len, out_str);			\
}

#define UPDATE_DST(PTR, LEN, HAVE_STAR, DST_PTR, DST_LEN, MAX_LEN)			\
{											\
	if (!HAVE_STAR)									\
	{										\
		if (MAX_LEN < ++DST_LEN)						\
		{									\
			util_out_print_gtmio("Trigger definition too long", FLUSH);	\
			return FALSE;							\
		}									\
		*DST_PTR++ = *PTR++;							\
	}										\
	else PTR++;									\
	LEN--;										\
}
#define UPDATE_TRIG_PTRS(VAL, NEXT_VAL, LEN, MAX_OUTPUT_LEN)				\
{											\
	NEXT_VAL = VAL + LEN;								\
	if (0 > --MAX_OUTPUT_LEN)							\
	{										\
		util_out_print_gtmio("Trigger definition too long", FLUSH);		\
		return FALSE;								\
	}										\
	*NEXT_VAL++ = '\0';								\
}

#define PROCESS_NUMERIC(PTR, LEN, HAVE_STAR, DST_PTR, DST_LEN, MAX_LEN)			\
{											\
	UPDATE_DST(PTR, LEN, HAVE_STAR, DST_PTR, DST_LEN, MAX_LEN);			\
	while (ISDIGIT_ASCII(*PTR))							\
	{										\
		if (!HAVE_STAR)								\
		{									\
			if (MAX_LEN < ++DST_LEN)					\
			{								\
				util_out_print_gtmio("Subscript too long", FLUSH);	\
				return FALSE;						\
			}								\
			*DST_PTR++ = *PTR++;						\
		} else									\
			PTR++;								\
		LEN--;									\
	}										\
}
#define PROCESS_STRING(PTR, LEN, HAVE_STAR, DST_PTR, DST_LEN, MAX_LEN)			\
{											\
	char		*ptr1;								\
	int		add_len;							\
											\
	ptr1 = scan_to_end_quote(PTR, LEN, MAX_LEN);					\
	if (NULL == ptr1)								\
	{										\
		util_out_print_gtmio("Invalid string", FLUSH);				\
		return FALSE;								\
	}										\
	add_len = (int)(ptr1 - PTR);   							\
	LEN -= add_len;									\
	if (!HAVE_STAR)									\
	{										\
		if (MAX_LEN < (DST_LEN + add_len))					\
		{									\
			util_out_print_gtmio("Trigger definition too long", FLUSH);	\
			return FALSE;							\
		}									\
		memcpy(DST_PTR, PTR, add_len);						\
		DST_PTR += add_len;							\
		DST_LEN += add_len;							\
	}										\
	PTR = ptr1;									\
}
#define PROCESS_AND_GET_NUMERIC(PTR, LEN, HAVE_STAR, DST_PTR, DST_LEN, NUM, MAX_LEN)	\
{											\
	char		*ptr1;								\
	int		add_len;							\
											\
	ptr1 = PTR;									\
	A2I(ptr1, PTR + LEN, NUM);							\
	add_len = (int)(ptr1 - PTR);							\
	LEN -= add_len;									\
	if (!HAVE_STAR)									\
	{										\
		if (MAX_LEN < (DST_LEN + add_len))					\
		{									\
			util_out_print_gtmio("Subscript too long", FLUSH);		\
			return FALSE;							\
		}									\
		memcpy(DST_PTR, PTR, add_len);						\
		DST_PTR += add_len;							\
		DST_LEN += add_len;							\
	}										\
	PTR = ptr1;									\
}

#define GET_CLI_STR(QUAL, MAX_OUTPUT_LEN, VALUE, RES)					\
{											\
	unsigned short		qual_len;						\
											\
	qual_len = (unsigned short)MAX_OUTPUT_LEN;					\
	assert(MAX_OUTPUT_LEN == qual_len); /* make sure it fits in short */		\
	if (FALSE == cli_get_str(QUAL, VALUE, &qual_len))				\
		return FALSE;								\
	RES = qual_len;									\
}

#define GET_CLI_STR_AND_CHECK(QUAL, QUAL_PRESENT, MAX_OUTPUT_LEN, CHECK_FN, VAL, VAL_LEN, NEXT_VAL, ERR_MSG) \
{											\
	unsigned short		qual_str_len;						\
											\
	if (CLI_PRESENT == (QUAL_PRESENT = cli_present(QUAL)))				\
	{										\
		GET_CLI_STR(QUAL, MAX_OUTPUT_LEN, VAL, qual_str_len); 			\
		VAL_LEN = qual_str_len;							\
		if (!CHECK_FN(VAL, &VAL_LEN)) 						\
		{									\
			if (0 == STR_LIT_LEN(ERR_MSG))					\
				return FALSE;						\
			else								\
			{								\
				ERROR_MSG_RETURN(ERR_MSG, VAL_LEN, VAL);		\
			}								\
		}									\
	} else										\
		VAL_LEN = 0;								\
	/* cli_get_str() will put the string in the buffer (based on max_output_len)	\
	 * but the check_*() function might take another pass over the string in the	\
	 * output buffer and consolidate it -- for example piece can reduce 2;3;4;5	\
	 * to 2:5.  So don't adjust max_output_len until we know the actual length.	\
	 */										\
	MAX_OUTPUT_LEN -= VAL_LEN;							\
	UPDATE_TRIG_PTRS(VAL, NEXT_VAL, VAL_LEN, MAX_OUTPUT_LEN);			\
}

GBLREF	char			cli_err_str[];
GBLREF	boolean_t		gtm_cli_interpret_string;

LITREF	unsigned char		lower_to_upper_table[];
LITREF	mval			gvtr_cmd_mval[GVTR_CMDTYPES];

error_def(ERR_INVSTRLEN);

STATICFNDEF char *scan_to_end_quote(char *ptr, int len, int max_len)
{
	int			str_len = 0;

	if ((1 >= len) || ('"' != *ptr))
		return NULL;	/* Invalid string - it needs at least "" */
	if (max_len < ++str_len)
	{
		util_out_print_gtmio("String too long", FLUSH);
		return NULL;
	}
	ptr++;
	len--;
	for ( ; 0 < len; len--, ptr++)
	{	/* Scan until the closing quote */
		if ('"' == *ptr)
		{
			if (1 == len)
				break;
			if ('"' == *(ptr + 1))
			{
				if (max_len < ++str_len)
				{
					util_out_print_gtmio("String too long", FLUSH);
					return NULL;
				}
				ptr++;
				len--;
			} else
				break;
		}
		if (max_len < ++str_len)
		{
			util_out_print_gtmio("String too long", FLUSH);
			return NULL;
		}
	}
	return (('"' == *ptr) ? ptr + 1 : NULL);
}

STATICFNDEF boolean_t process_dollar_char(char **src_ptr, int *src_len, boolean_t have_star, char **d_ptr, int *dst_len)
{
	int		char_count;
	char		*char_ptr;
	int		len;
	char		*dst_ptr;
	char		dst_string[MAX_DCHAR_LEN];
	int		lcl_dst_len;
	mstr		m_dst;
	mstr		m_src;
	char		*ptr;
	int		q_len;
	char		*tmp_dst_ptr;

	tmp_dst_ptr = dst_ptr = *d_ptr;
	ptr = *src_ptr;
	len = *src_len;
	lcl_dst_len = *dst_len;
	assert('$' == *ptr);
 	UPDATE_DST(ptr, len, have_star, dst_ptr, lcl_dst_len, MAX_GVSUBS_LEN);
	if (0 == len)
		return FALSE;
	switch (*ptr)
	{
		case 'c':
		case 'C':
			UPDATE_DST(ptr, len, have_star, dst_ptr, lcl_dst_len, MAX_GVSUBS_LEN);
			if ((0 < len) && ('(' == *ptr))
				break;
			else if ((3 < len) && ('H' == lower_to_upper_table[*ptr])
					&& ('A' == lower_to_upper_table[*(ptr + 1)])
					&& ('R' == lower_to_upper_table[*(ptr + 2)]) && ('(' == *(ptr + 3)))
			{
				ptr += 3;
				len -= 3;
				break;
			}
			else
				return FALSE;
			break;
		case 'z':
		case 'Z':
			UPDATE_DST(ptr, len, have_star, dst_ptr, lcl_dst_len, MAX_GVSUBS_LEN);
			if ((2 < len) && ('C' == lower_to_upper_table[*ptr])
					&& ('H' == lower_to_upper_table[*(ptr + 1)]) && ('(' == *(ptr + 2)))
			{
				ptr += 2;
				len -= 2;
			}
			else if ((4 < len) && ('C' == lower_to_upper_table[*ptr])
					&& ('H' == lower_to_upper_table[*(ptr + 1)])
					&& ('A' == lower_to_upper_table[*(ptr + 2)])
					&& ('R' == lower_to_upper_table[*(ptr + 3)]) && ('(' == *(ptr + 4)))
			{
				ptr += 4;
				len -= 4;
			}
			else
				return FALSE;
			if (MAX_GVSUBS_LEN < lcl_dst_len + 2)
			{
				util_out_print_gtmio("Subscript too long", FLUSH);
				return FALSE;
			}
			MEMCPY_LIT(dst_ptr, "CH");
			dst_ptr += 2;
			lcl_dst_len += 2;
			break;
		default:
			return FALSE;
	}
	assert('(' == *ptr);
	UPDATE_DST(ptr, len, have_star, dst_ptr, lcl_dst_len, MAX_GVSUBS_LEN);
	while ((0 < len) && (')' != *ptr))
	{
		UPDATE_DST(ptr, len, have_star, dst_ptr, lcl_dst_len, MAX_GVSUBS_LEN);
	}
	q_len = 0;
	if (!have_star)
	{
		if (MAX_GVSUBS_LEN < ++lcl_dst_len)
		{
			util_out_print_gtmio("$[Z]CHAR expression too long", FLUSH);
			return FALSE;
		}
		*dst_ptr++ = *ptr++;
		*dst_ptr = '\0';
		m_src.addr = tmp_dst_ptr;
		m_src.len = (mstr_len_t)(dst_ptr - tmp_dst_ptr);
		m_dst.addr = dst_string;
		m_dst.len = 0;
		if (!zwr2format(&m_src, &m_dst))
			return FALSE;
		lcl_dst_len = *dst_len;		/* Reset length because we're creating the final version now */
		if (MAX_GVSUBS_LEN < ++lcl_dst_len)
		{
			util_out_print_gtmio("Subscript too long", FLUSH);
			return FALSE;
		}
		*tmp_dst_ptr++ = '"';
		char_ptr = m_dst.addr;
		if (MAX_GVSUBS_LEN < (lcl_dst_len + m_dst.len))
		{
			util_out_print_gtmio("Subscript too long", FLUSH);
			return FALSE;
		}
		for (char_count = 0; m_dst.len > char_count; char_count++)
		{
			if ('"' == *char_ptr)
			{
				if (MAX_GVSUBS_LEN < ++lcl_dst_len)
				{
					util_out_print_gtmio("Subscript too long", FLUSH);
					return FALSE;
				}
				*tmp_dst_ptr++ = '"';
				q_len++;
			}
			*tmp_dst_ptr++ = *char_ptr++;
			lcl_dst_len++;
		}
		if (MAX_GVSUBS_LEN < ++lcl_dst_len)
		{
			util_out_print_gtmio("Subscript too long", FLUSH);
			return FALSE;
		}
		*tmp_dst_ptr++ = '"';
		dst_ptr = tmp_dst_ptr;
	}
	else
		ptr++;
	assert(!have_star || ((dst_ptr == *d_ptr) && (lcl_dst_len == *dst_len)));
	*src_ptr = ptr;
	*src_len = len + 2 + q_len;	/* Allow for the open and close quotes and any internal quotes */
	*d_ptr = dst_ptr;
	*dst_len = lcl_dst_len;
	return TRUE;
}

STATICFNDEF boolean_t process_delim(char *delim_str, uint4 *delim_len)
{
	int		char_count;
	mstr		dst;
	int		dst_len;
	char		*dst_ptr;
	char		dst_string[MAX_DELIM_LEN + 1];
	uint4		len;
	char		*ptr;
	char		*ptr1;
	int		q_len;
	char		src_string[MAX_DELIM_LEN + 1];
	mstr		src;
	char		*src_ptr;

	if (MAX_DELIM_LEN < *delim_len)
	{
		util_out_print_gtmio("Delimiter too long", FLUSH);
		return FALSE;
	}
	ptr = delim_str;
	len = *delim_len;
	src_ptr = src_string;
	dst_len = 0;
	/* If ", scan to end quote
	 * If $, look for char --> c or zchar --> zch
	 * If _, leave it
	 */
	while (0 < len)
	{
		switch (*ptr)
		{
			case '"':
				PROCESS_STRING(ptr, len, FALSE, src_ptr, dst_len, MAX_DELIM_LEN);
				break;
			case '$':
				UPDATE_DST(ptr, len, FALSE, src_ptr, dst_len, MAX_DELIM_LEN);
				if (0 == len)
				{
					util_out_print_gtmio("Invalid entry in delimiter", FLUSH);
					return FALSE;
				}
				if ((3 < len) && ('C' == lower_to_upper_table[*ptr])
						&& ('H' == lower_to_upper_table[*(ptr + 1)])
						&& ('A' == lower_to_upper_table[*(ptr + 2)])
						&& ('R' == lower_to_upper_table[*(ptr + 3)]))
				{
					if (MAX_DELIM_LEN < ++dst_len)
					{
						util_out_print_gtmio("Trigger definition too long", FLUSH);
						return FALSE;
					}
					*src_ptr++ = 'C';
					ptr += 4;
					len -= 4;
				} else if ((4 < len) && ('Z' == lower_to_upper_table[*ptr])
						&& ('C' == lower_to_upper_table[*(ptr + 1)])
						&& ('H' == lower_to_upper_table[*(ptr + 2)])
						&& ('A' == lower_to_upper_table[*(ptr + 3)])
						&& ('R' == lower_to_upper_table[*(ptr + 4)]))
				{
					if (MAX_DELIM_LEN < (dst_len + 3))
					{
						util_out_print_gtmio("Trigger definition too long", FLUSH);
						return FALSE;
					}
					MEMCPY_LIT(src_ptr, "ZCH");
					src_ptr += 3;
					dst_len += 3;
					ptr += 5;
					len -= 5;
				} else
				{
					UPDATE_DST(ptr, len, FALSE, src_ptr, dst_len, MAX_DELIM_LEN);
				}
				break;
			default:
				UPDATE_DST(ptr, len, FALSE, src_ptr, dst_len, MAX_DELIM_LEN);
				break;
		}
	}
	*src_ptr = '\0';
	src.addr = src_string;
	src.len = (mstr_len_t)(src_ptr - src_string);
	dst.addr = dst_string;
	dst.len = 0;
	if (!zwr2format(&src, &dst))
	{
		util_out_print_gtmio("Invalid delimiter", FLUSH);
		return FALSE;
	}
	if (MAX_DELIM_LEN < dst.len)
	{
		util_out_print_gtmio("Delimiter too long", FLUSH);
		return FALSE;
	}
	memcpy(delim_str, dst_string, dst.len);
	*delim_len = dst.len;
	return TRUE;
}

boolean_t check_trigger_name(char *name_str, uint4 *name_len)
{
	uint4		len, lcl_name_len;
	unsigned char	*ptr;

	/* To conform to the other uses of the check_* functions and their use in the GET_CLI_STR_AND_CHECK macro
	 * name_len is passed in as a uint4 *, but in this case a uint4 would suffice.  To mitigate the dereferencing
	 * issues, a local copy is used.
	 */
	lcl_name_len = *name_len;
	if (MAX_USER_TRIGNAME_LEN < lcl_name_len)
		return FALSE;
	ptr = (unsigned char *)name_str;
	if (('%' != *ptr) && !ISALPHA_ASCII(*ptr))
		return FALSE;
	for (len = lcl_name_len - 1, ptr++; (0 < len) && ISALNUM_ASCII(*ptr); ptr++, len--)
	    	;
	return (0 == len);
}

STATICFNDEF boolean_t process_options(char *option_str, uint4 option_len, boolean_t *isolation, boolean_t *noisolation,
			       boolean_t *consistency, boolean_t *noconsistency)
{
	char		local_options[MAX_OPTIONS_LEN];
	char		*ptr;

	if (MAX_OPTIONS_LEN < option_len)
	{
		util_out_print_gtmio("Too many options", FLUSH);
		return FALSE;
	}
	memcpy(local_options, option_str, option_len);
	*isolation = *noisolation = *consistency = *noconsistency = FALSE;
	ptr = local_options;
	for ( ; 0 < option_len; ptr++, option_len--)
		*ptr = TOUPPER(*ptr);
	ptr = strtok(local_options, ",");
	do
	{
		switch (*ptr)
		{
			case 'C':
				if (1 == STRLEN(ptr))
					*consistency = TRUE;
				else
				{
					assert(0 == MEMCMP_LIT(ptr, HASHT_OPT_CONSISTENCY));
					*consistency = TRUE;
				}
				break;
			case 'I':
				if (1 == STRLEN(ptr))
					*isolation = TRUE;
				else
				{
					assert(0 == MEMCMP_LIT(ptr, HASHT_OPT_ISOLATION));
					*isolation = TRUE;
				}
				break;
			case 'N':
				assert('O' == *(ptr + 1));
				if ('C' == *(ptr + 2))
				{
					assert(0 == MEMCMP_LIT(ptr, HASHT_OPT_NOCONSISTENCY));
					*noconsistency = TRUE;
				} else
				{
					assert('I' == *(ptr + 2));
					assert(0 == MEMCMP_LIT(ptr, HASHT_OPT_NOISOLATION));
					*noisolation = TRUE;
				}
				break;
			default:
				GTMASSERT;	/* Parsing should have found invalid command */
				break;
		}
	} while (ptr = strtok(NULL, ","));
	return !((*isolation && *noisolation) || (*consistency && *noconsistency));
}

STATICFNDEF boolean_t process_subscripts(char *subscr_str, uint4 *subscr_len, char **next_str, char *out_str, int4 *out_max)
{
	boolean_t	alternation;
	char		dst[MAX_GVSUBS_LEN];
	int		dst_len;
	char		*dst_ptr;
	int		len;
	boolean_t	have_pattern;
	boolean_t	have_range;
	boolean_t	have_star;
	boolean_t	have_dot;
	int		i;
	int		lvn_count;
	uint4		lvn_len[MAX_LVN_COUNT];
	char		*lvn_start;
	char		*lvn_str[MAX_LVN_COUNT];
	int		multiplier;
	boolean_t	newsub;
	int		num1;
	int		num2;
	char		*ptr;
	char		*ptr1;
	char		*save_dst_ptr;
	int		save_len;
	char		*start_dst_ptr;
	uint4		start_len;
	uint4		subsc_count;
	uint4		tmp_len;
	boolean_t	valid_sub;

	have_pattern = have_range = valid_sub = have_star = FALSE;
	ptr = subscr_str;
	start_len = len = *subscr_len;
	dst_len = 0;
	newsub = TRUE;
	subsc_count = lvn_count = 0;
	start_dst_ptr = dst_ptr = dst;
	while ((0 < len) && (')' != *ptr))
	{
		if (ISDIGIT_ASCII(*ptr) || ('-' == *ptr))
		{
			PROCESS_NUMERIC(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
			newsub = FALSE;
			valid_sub = TRUE;
		}
		else if (ISALPHA_ASCII(*ptr) || ('%' == *ptr))
		{
			if (!newsub)
			{
				util_out_print_gtmio("Invalid subscript", FLUSH);
				return FALSE;
			}
			lvn_start = ptr;
			tmp_len = 0;
			do
			{
				if (MAX_MIDENT_LEN < ++tmp_len)
				{
					util_out_print_gtmio("Variable name too long", FLUSH);
					return FALSE;
				}
				if (MAX_GVSUBS_LEN < ++dst_len)
				{
					util_out_print_gtmio("Subscript too long", FLUSH);
					return FALSE;
				}
				*dst_ptr++ = *ptr++;
				len--;
			} while (ISALNUM_ASCII(*ptr));
			if ('=' != *ptr)
			{
				util_out_print_gtmio("Invalid variable name in subscript", FLUSH);
				return FALSE;
			}
			for (i = 0; i < lvn_count; i++)
			{
				if ((lvn_len[i] == tmp_len) && (0 == strncmp(lvn_str[i], lvn_start, tmp_len)))
				{
					util_out_print_gtmio("Duplicate variable name in subscript", FLUSH);
					return FALSE;
				}
			}
			lvn_str[lvn_count] = lvn_start;
			lvn_len[lvn_count] = tmp_len;
			lvn_count++;
			if (MAX_GVSUBS_LEN < ++dst_len)
			{
				util_out_print_gtmio("Subscript too long", FLUSH);
				return FALSE;
			}
			*dst_ptr++ = *ptr++;		/* move past = */
			start_dst_ptr = dst_ptr;
			len--;
			if ((0 < len) && ((',' == *ptr) || (')' == *ptr)))
			{
				util_out_print_gtmio("Variable name not followed by valid subscript", FLUSH);
				return FALSE;
			}
			continue;
		} else
		{
			switch (*ptr)
			{
				case '"':
					PROCESS_STRING(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
					valid_sub = TRUE;
					newsub = FALSE;
					break;
				case '$':
					if (!process_dollar_char(&ptr, &len, have_star, &dst_ptr, &dst_len))
					{
						util_out_print_gtmio("Invalid subscript", FLUSH);
						return FALSE;
					}
					newsub = FALSE;
					valid_sub = TRUE;
					break;
				case '?':
					if (have_range)
					{
						util_out_print_gtmio("Range and pattern match not valid in same subscript", FLUSH);
						return FALSE;
					}
					UPDATE_DST(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
					ptr1 = ptr;
					alternation = FALSE;
					while ((0 < len) && ((',' != *ptr) || alternation) && ((')' != *ptr) || alternation)
					       && (';' != *ptr))
					{
						num1 = num2 = -1;
						have_dot = FALSE;
						if (ISDIGIT_ASCII(*ptr))
						{
							PROCESS_AND_GET_NUMERIC(ptr, len, have_star, dst_ptr, dst_len, num1,
										MAX_GVSUBS_LEN);
						}
						if ('.' == *ptr)
						{
							have_dot = TRUE;
							if (MAX_GVSUBS_LEN < ++dst_len)
							{
								util_out_print_gtmio("Subscript too long", FLUSH);
								return FALSE;
							}
							*dst_ptr++ = *ptr++;
							len--;
							if (ISDIGIT_ASCII(*ptr))
							{
								PROCESS_AND_GET_NUMERIC(ptr, len, have_star, dst_ptr, dst_len, num2,
									MAX_GVSUBS_LEN);
							}
							if (-1 == num1)
								num1 = 0;
						}
						switch (*ptr)
						{
							case '(':
								if (!alternation && (0 <= num1))
								{
									UPDATE_DST(ptr, len, have_star, dst_ptr, dst_len,
										   MAX_GVSUBS_LEN);
									alternation = TRUE;
									continue;
								}
								break;
							case ',':
								if (alternation && (-1 == num1) && (-1 == num2))
								{
									UPDATE_DST(ptr, len, have_star, dst_ptr, dst_len,
										   MAX_GVSUBS_LEN);
									continue;
								}
								break;
							case ')':
								if (alternation && (-1 == num1) && (-1 == num2))
								{
									UPDATE_DST(ptr, len, have_star, dst_ptr, dst_len,
										   MAX_GVSUBS_LEN);
									alternation = FALSE;
									continue;
								}
								break;
							default:
								break;
						}
						if ((0 <= num1) && (0 <= num2) && (num2 < num1))
						{
							util_out_print_gtmio("Invalid pattern match range", FLUSH);
							return FALSE;
						}
						switch (toupper(*ptr))
						{
							case 'E':
								if (have_dot && (0 >= num1) && (-1 == num2))
								{ /* Treat ?.E the same as * */
									have_star = TRUE;
									dst_ptr = start_dst_ptr;
									if (MAX_GVSUBS_LEN < ++dst_len)
									{
										util_out_print_gtmio("Subscript too long", FLUSH);
										return FALSE;
									}
									*dst_ptr++ = '*';
								}
								/* Note: we're dropping into the following case */
							case 'A':
							case 'C':
							case 'K':
							case 'L':
							case 'N':
							case 'P':
							case 'U':
							case 'V':

								UPDATE_DST(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
								break;
							case '"':
								PROCESS_STRING(ptr, len, have_star, dst_ptr, dst_len,
									       MAX_GVSUBS_LEN);
								break;
								/* Note: we're dropping into the default/error case */
							default:
								CONV_CH_AND_PRINT("Unexpected character !AD in pattern code", ptr);
								return FALSE;
						}
					}
					have_pattern = TRUE;
					valid_sub = TRUE;
					newsub = FALSE;
					break;
				case ':':
					if (have_range)
					{
						util_out_print_gtmio("Range within a range not allowed", FLUSH);
						return FALSE;
					}
					if (have_pattern)
					{
						util_out_print_gtmio("Pattern not allowed as a range", FLUSH);
						return FALSE;
					}
					UPDATE_DST(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
					if (ISDIGIT_ASCII(*ptr) || ('-' == *ptr))
					{
						PROCESS_NUMERIC(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
					}
					else if ('"' == *ptr)
					{
						PROCESS_STRING(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
					} else if ('$' == *ptr)
					{
						if (!process_dollar_char(&ptr, &len, have_star, &dst_ptr, &dst_len))
						{
							util_out_print_gtmio("Invalid range value", FLUSH);
							return FALSE;
						}
					} else if ((0 < len) && (',' != *ptr) && (';' != *ptr) && (')' != *ptr))
					{
						util_out_print_gtmio("Invalid string range", FLUSH);
						return FALSE;
					} else
					{	/* A range with no lower end - just scan the numeric or string */
						ptr1 = ptr;
						if (ISDIGIT_ASCII(*ptr) || ('-' == *ptr))
						{
							PROCESS_NUMERIC(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
						} else if ('"' == *ptr1)
						{
							PROCESS_STRING(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
						} else if ((0 < len) && ((',' != *ptr) && (';' != *ptr) && (')' != *ptr)))
						{
							util_out_print_gtmio("Range value must be integer or string", FLUSH);
							return FALSE;
						} else if (!valid_sub)
						{ /* this is a single ":" - treat it just like a * */
							have_star = TRUE;
							dst_ptr = start_dst_ptr;
							if (MAX_GVSUBS_LEN < ++dst_len)
							{
								util_out_print_gtmio("Subscript too long", FLUSH);
								return FALSE;
							}
							*dst_ptr++ = '*';
						}
					}
					valid_sub = TRUE;
					newsub = FALSE;
					have_range = TRUE;
					break;
				case '*':
					if (!have_star)
					{
						have_star = TRUE;
						dst_ptr = start_dst_ptr;
						if (MAX_GVSUBS_LEN < ++dst_len)
						{
							util_out_print_gtmio("Subscript too long", FLUSH);
							return FALSE;
						}
						*dst_ptr++ = '*';
					}
					ptr++;
					len--;
					if ((0 < len) && (',' != *ptr) && (';' != *ptr) && (')' != *ptr))
					{
						util_out_print_gtmio("Invalid use of *", FLUSH);
						return FALSE;
					} else
						valid_sub = TRUE;
					newsub = FALSE;
					break;
				case ';':
					UPDATE_DST(ptr, len, have_star, dst_ptr, dst_len, MAX_GVSUBS_LEN);
					/* Delete extraneous ; in the subscript */
					if ((!have_star) && (newsub || ((0 < len)
						&& ((',' == *ptr) || (';' == *ptr) || ISALPHA_ASCII(*ptr)
							|| ('%' == *ptr) || (')' == *ptr)))))
						dst_ptr--;
					valid_sub = FALSE;
					have_pattern = have_range = FALSE;
					break;
				case ',':
					if (newsub)
					{
						util_out_print_gtmio("Empty subscript not allowed", FLUSH);
						return FALSE;
					}
					if (MAX_GVSUBSCRIPTS <= ++subsc_count)
					{
						util_out_print_gtmio("Too many subscripts", FLUSH);
						return FALSE;
					}
					if (MAX_GVSUBS_LEN < ++dst_len)
					{
						util_out_print_gtmio("Subscript too long", FLUSH);
						return FALSE;
					}
					*dst_ptr++ = *ptr++;
					len--;
					start_dst_ptr = dst_ptr;
					newsub = TRUE;
					have_range = have_pattern = valid_sub = have_star = FALSE;
					break;
				default:
					CONV_CH_AND_PRINT("Invalid character !AD in subscript", ptr);
					return FALSE;
			}
		}
	}
	if ((0 == len) && (')' != *ptr))
	{
		util_out_print_gtmio("Missing \")\" after global subscript", FLUSH);
		return FALSE;
	}
	if ((start_len == len) || newsub)
	{
		util_out_print_gtmio("Empty subscript not allowed", FLUSH);
		return FALSE;
	}
	if ((')' == *ptr) && (MAX_GVSUBSCRIPTS <= ++subsc_count))
	{
		util_out_print_gtmio("Too many subscripts", FLUSH);
		return FALSE;
	}
	CHECK_FOR_ROOM_IN_OUTPUT_AND_COPY(dst, out_str, (int)(dst_ptr - dst), *out_max);
	*subscr_len = (uint4)(dst_ptr - dst);
	*next_str = ptr + 1;
	return TRUE;
}

STATICFNDEF boolean_t process_pieces(char *piece_str, uint4 *piece_len)
{
	uint		bit;
	uint4		bitmap[MAX_PIECE_INT];
	uint		bitval;
	uint		bitword;
	boolean_t	have_low_num;
	boolean_t	have_num;
	boolean_t	have_num_in_str;
	uint		i;
	uint4		len;
	int		low;
	int		num;
	char		*ptr;
	char		*ptr1;
	boolean_t	have_error = FALSE;
	int		num_len;

	memset((void *)bitmap, 0, MAX_PIECE_CHARS);
	ptr = piece_str;
	len = *piece_len;
	have_num = have_low_num = FALSE;
	if ('"' == *ptr)
	{
		ptr++;
		len--;
	}
	while ((0 < len) && !have_error)
	{
		switch (*ptr)
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				ptr1 = ptr;
				A2I(ptr1, ptr + len, num);
				if ((0 == num) || (MAX_PIECE_VALUE < num))
				{
					CONV_NUM_AND_STR_AND_PRINT("Invalid value !UL in PIECES - !AD", num, *piece_len, piece_str);
					have_error = TRUE;
					break;
				}
				len -= (int)(ptr1 - ptr);
				ptr = ptr1;
				bitmap[num / BITS_PER_INT] |= 1 << (num % BITS_PER_INT);
				have_num = TRUE;
				have_low_num = TRUE;
				break;
			case ';':
				if (!have_num)
				{
					CONV_STR_AND_PRINT("Expected an integer in PIECES - ", *piece_len, piece_str);
					have_error = TRUE;
					break;
				}
				have_num = FALSE;
				ptr++;
				len--;
				break;
			case ':':
				if (!have_low_num)
				{
					CONV_STR_AND_PRINT("Expected an integer for lower value in range in PIECES - ",
						*piece_len, piece_str);
					have_error = TRUE;
					break;
				}
				ptr++;
				len--;
				low = num;
				if (!ISDIGIT_ASCII(*ptr))
				{
					CONV_STR_AND_PRINT("Expected an integer for upper value in range in PIECES - ",
						*piece_len, piece_str);
					have_error = TRUE;
					break;
				}
				ptr1 = ptr;
				A2I(ptr1, ptr + len, num);
				if (MAX_PIECE_VALUE < num)
				{
					CONV_NUM_AND_STR_AND_PRINT("Invalid value \"!UL\" in PIECES - !AD", num,
								   *piece_len, piece_str);
					have_error = TRUE;
					break;
				}
				if (num <= low)
				{
					CONV_STR_AND_PRINT("Low value of range not less than high value in PIECES - ",
						*piece_len, piece_str);
					have_error = TRUE;
					break;
				}
				len -= (int)(ptr1 - ptr);
				ptr = ptr1;
				for (i = low + 1; i <= num; i++)
					bitmap[i / BITS_PER_INT] |= 1 << (i % BITS_PER_INT);
				have_num = TRUE;
				have_low_num = FALSE;
				break;
			case '"':
				if (1 == len)
				{
					len--;
					break;
				}
			default:
				CONV_CH_AND_STR_AND_PRINT("Invalid character !AD in PIECES - !AD", ptr, *piece_len, piece_str);
				have_error = TRUE;
				break;
		}
	}
	if ((0 != len) || !have_num || have_error)
	{
		if (!have_error)
			CONV_STR_AND_PRINT("Invalid PIECE entry - ", *piece_len, piece_str);
		return FALSE;
	}
	have_num = have_low_num = have_num_in_str = FALSE;
	ptr = piece_str;
	len = 0;
	for (i = 0; i < MAX_PIECE_INT; i++)
	{
		if ((0 == bitmap[i] && !have_low_num))
			continue;
		bitword = bitmap[i];
		bit = 0;
		while ((0 != bitword) || have_low_num)
		{
			bitval = bitword & 1;
			if (!have_low_num)
			{
				if (0 == bitval)
				{
					bitword = bitword >> 1;
					bit++;
					continue;
				}
				have_num = TRUE;
				have_low_num = TRUE;
				low = i * BITS_PER_INT + bit;
				if (have_num_in_str)
				{
					*ptr++ = ';';
					len++;
				}
				num_len = 0;
				I2A(ptr, num_len, low);
				len += num_len;
				ptr += num_len;
				have_num_in_str = TRUE;
			}
			while ((1 == bitval) && (BITS_PER_INT > bit))
			{
				bitword = bitword >> 1;
				bitval = bitword & 1;
				bit++;
			}
			if (BITS_PER_INT == bit)
				break;
			num = (0 == bit) ? i * BITS_PER_INT - 1 : i * BITS_PER_INT + bit - 1;
			have_low_num = FALSE;
			if (num == low)
				continue;
			*ptr++ = ':';
			len++;
			num_len = 0;
			I2A(ptr, num_len, num);
			len += num_len;
			ptr += num_len;
		}
	}
	*ptr = '\0';
	*piece_len = len;
	return TRUE;
}

STATICFNDEF boolean_t process_xecute(char *xecute_str, uint4 *xecute_len, boolean_t multi_line)
{
	uint4		dst_len;
	char		dst_string[MAX_SRCLINE];
	uint4		src_len;
	gv_trigger_t	trigdsc;

	src_len = *xecute_len;
	if ((src_len == XTENDED_START_LEN) && (0 == memcmp(XTENDED_START, xecute_str, XTENDED_START_LEN)))
		return TRUE;
	if (!multi_line && (NULL != memchr(xecute_str, '\n', src_len)))
	{
		util_out_print_gtmio("Newline not allowed in XECUTE string", FLUSH);
		return FALSE;
	}
	if (!multi_line && MAX_SRCLINE < src_len + 1)	/* allow room for leading blank */
	{
		return FALSE;
	}
	if (0 == src_len)
	{
		util_out_print_gtmio("Empty XECUTE string is invalid", FLUSH);
		return FALSE;
	}
	if (!multi_line)
	{
		dst_string[0] = ' ';
		if (!trigger_scan_string(xecute_str, &src_len, dst_string + 1, &dst_len) || (1 != src_len))
		{
			util_out_print_gtmio("Invalid XECUTE string", FLUSH);
			return FALSE;
		}
		memcpy(xecute_str, dst_string, ++dst_len);
		*xecute_len = dst_len;
		trigdsc.xecute_str.str.addr = dst_string;
		trigdsc.xecute_str.str.len = dst_len;
	} else
	{
		trigdsc.xecute_str.str.addr = xecute_str;
		trigdsc.xecute_str.str.len = src_len;
	}
	/* Test compile the string - first build up the parm list to trigger compile routine */
	trigdsc.rtn_desc.rt_name.addr = TRIGR_PRECOMP_RTNNAME;
	trigdsc.rtn_desc.rt_name.len = SIZEOF(TRIGR_PRECOMP_RTNNAME) - 1;
	trigdsc.rtn_desc.rt_adr = NULL;
	return (0 == gtm_trigger_complink(&trigdsc, FALSE));
}

boolean_t trigger_parse(char *input, uint4 input_len, char *trigvn, char **values, uint4 *value_len, int4 *max_len,
			boolean_t *multi_line_xecute)
{
	boolean_t	cli_status;
	int		cmd_ind;
	int		count;
	int		eof;
	int		extra;
	boolean_t	found_zkill;
	boolean_t	found_kill;
	uint4		len;
	int4		max_output_len;
	int		parse_ret;
	boolean_t	delim_present, name_present, pieces_present, xecute_present, zdelim_present;
	char		*ptr;
	char		*ptr1;
	char		*ptr2;
	int4		rec_num;
	CLI_ENTRY       *save_cmd_ary;
	boolean_t	set_present;
	int		trigvn_len;
	boolean_t	in_multi_line_xecute, out_multi_line_xecute;

	max_output_len = *max_len;
	in_multi_line_xecute = out_multi_line_xecute = *multi_line_xecute;
	if (in_multi_line_xecute)
	{
		if ((XTENDED_STOP_LEN != input_len) || (0 != memcmp(XTENDED_STOP, input, XTENDED_STOP_LEN)))
		{
			ptr1 = values[XECUTE_SUB] + value_len[XECUTE_SUB];
			if (0 < (max_output_len - input_len - 1))
			{
				memcpy(ptr1, input, input_len);
				ptr1 += input_len;
				*ptr1 = '\n';
				max_output_len -= (input_len + 1);
				value_len[XECUTE_SUB] += input_len + 1;
				*max_len = max_output_len;
				return TRIG_SUCCESS;
			} else
			{
				*multi_line_xecute = FALSE;
				util_out_print_gtmio("Trigger definition too long", FLUSH);
				return FALSE;
			}
		}
		out_multi_line_xecute = FALSE;
		if (!process_xecute(values[XECUTE_SUB], &value_len[XECUTE_SUB], TRUE))
		{
			*multi_line_xecute = FALSE;
			ERROR_MSG_RETURN("Error parsing XECUTE string: ", value_len[XECUTE_SUB], values[XECUTE_SUB]);
		}
		*multi_line_xecute = out_multi_line_xecute;
		*max_len = max_output_len;
		return TRIG_SUCCESS;
	}
	ptr1 = input;
	len = (uint4)input_len;
	if ('^' != *ptr1++)
	{
		ERROR_MSG_RETURN("Missing global name:\n", input_len, input);
	}
	ptr = ptr1;
	len--;
	if (('%' != *ptr1) && !ISALPHA_ASCII(*ptr1))
	{
		ERROR_MSG_RETURN("Invalid global name:\n", input_len, input);
	}
	ptr1++;
	while (ISALNUM_ASCII(*ptr1))
		ptr1++;
	if (('(' != *ptr1) && !ISSPACE_ASCII(*ptr1))
	{
		ERROR_MSG_RETURN("Invalid global name:\n", input_len, input);
	}
	trigvn_len = (int)(ptr1 - ptr);
	if (MAX_MIDENT_LEN < trigvn_len)
	{
		trigvn_len = MAX_MIDENT_LEN;
		util_out_print_gtmio("Warning: global name truncated to ^!AD", FLUSH, trigvn_len, ptr);
	}
	memcpy(trigvn, ptr, trigvn_len);
	trigvn[trigvn_len] = '\0';
	/* lookup global to get collation info */
	len -= (uint4)trigvn_len;
	ptr2 = ptr1;
	if ('(' == *ptr1)
	{
		ptr1++;
		len--;
		if (!process_subscripts(ptr1, &len, &ptr2, values[GVSUBS_SUB], &max_output_len))
		{
			ERROR_MSG_RETURN("", input_len, input);
		}
	} else
		len = 0;
	if (0 > --max_output_len)
	{
		util_out_print_gtmio("Trigger definition too long", FLUSH);
		return TRIG_FAILURE;
	}
	values[GVSUBS_SUB][len] = '\0';
	value_len[GVSUBS_SUB] = (uint4)len;
	save_cmd_ary = cmd_ary;
	cmd_ary = &trigger_cmd_ary[0];
	gtm_cli_interpret_string = FALSE;
	cli_str_setup(input_len - (uint4)(ptr2 - input), ptr2);
	parse_ret = parse_triggerfile_cmd();
	gtm_cli_interpret_string = TRUE;
	cmd_ary = save_cmd_ary;
	if (parse_ret)
	{
		ERROR_MSG_RETURN("Parse error in input:\n", input_len, input);
	}
	values[CMD_SUB] = values[GVSUBS_SUB] + value_len[GVSUBS_SUB] + 1;
	if (CLI_PRESENT == cli_present("COMMANDS"))
	{
		GET_CLI_STR("COMMANDS", max_output_len, values[CMD_SUB], value_len[CMD_SUB]);
		ptr = values[CMD_SUB];
		count = 0;
		found_zkill = found_kill = FALSE;
		/* The following order (set, kill, zkill, ztkill) needs to be maintained to match
		 * the corresponding bit value orders in gv_trig_cmd_table.h
		 */
		if (CLI_PRESENT == (set_present = cli_present("COMMANDS.SET")))
		{
			ADD_STRING(count, ptr, gvtr_cmd_mval[GVTR_CMDTYPE_SET].str.len, gvtr_cmd_mval[GVTR_CMDTYPE_SET].str.addr,
				max_output_len);
		}
		if (CLI_PRESENT == cli_present("COMMANDS.KILL"))
		{
			found_kill = TRUE;
			ADD_COMMA_IF_NEEDED(count, ptr, max_output_len);
			ADD_STRING(count, ptr, gvtr_cmd_mval[GVTR_CMDTYPE_KILL].str.len, gvtr_cmd_mval[GVTR_CMDTYPE_KILL].str.addr,
				max_output_len);
		}
		if (CLI_PRESENT == cli_present("COMMANDS.ZKILL"))
		{
			found_zkill = TRUE;
			ADD_COMMA_IF_NEEDED(count, ptr, max_output_len);
			ADD_STRING(count, ptr, gvtr_cmd_mval[GVTR_CMDTYPE_ZKILL].str.len,
				   gvtr_cmd_mval[GVTR_CMDTYPE_ZKILL].str.addr, max_output_len);
		}
		if (CLI_PRESENT == cli_present("COMMANDS.ZWITHDRAW") && !found_zkill)
		{
			ADD_COMMA_IF_NEEDED(count, ptr, max_output_len);
			ADD_STRING(count, ptr, gvtr_cmd_mval[GVTR_CMDTYPE_ZKILL].str.len,
				gvtr_cmd_mval[GVTR_CMDTYPE_ZKILL].str.addr, max_output_len);
		}
		if (CLI_PRESENT == cli_present("COMMANDS.ZTKILL"))
		{
			if (found_kill)
				ERROR_MSG_RETURN("KILL and ZTKILL incompatible: ", value_len[CMD_SUB], values[CMD_SUB]);
			ADD_COMMA_IF_NEEDED(count, ptr, max_output_len);
			ADD_STRING(count, ptr, gvtr_cmd_mval[GVTR_CMDTYPE_ZTKILL].str.len,
				gvtr_cmd_mval[GVTR_CMDTYPE_ZTKILL].str.addr, max_output_len);
		}
		if (CLI_PRESENT == cli_present("COMMANDS.ZTRIGGER"))
		{
			ADD_COMMA_IF_NEEDED(count, ptr, max_output_len);
			ADD_STRING(count, ptr, gvtr_cmd_mval[GVTR_CMDTYPE_ZTRIGGER].str.len,
				gvtr_cmd_mval[GVTR_CMDTYPE_ZTRIGGER].str.addr, max_output_len);
		}
		*ptr = '\0';
		value_len[CMD_SUB] = STRLEN(values[CMD_SUB]);
	} else
		value_len[CMD_SUB] = 0;
	UPDATE_TRIG_PTRS(values[CMD_SUB], values[DELIM_SUB], value_len[CMD_SUB], max_output_len);
	GET_CLI_STR_AND_CHECK("DELIM", delim_present, max_output_len, process_delim, values[DELIM_SUB], value_len[DELIM_SUB],
			      values[TRIGNAME_SUB], "Error parsing DELIM string: ");
	GET_CLI_STR_AND_CHECK("NAME", name_present, max_output_len, check_trigger_name, values[TRIGNAME_SUB],
			      value_len[TRIGNAME_SUB], values[OPTIONS_SUB], "Error parsing NAME string: ");
	if (CLI_PRESENT == cli_present("OPTIONS"))
	{
		boolean_t	isolation, noisolation, consistency, noconsistency;

		GET_CLI_STR("OPTIONS", max_output_len, values[OPTIONS_SUB], value_len[OPTIONS_SUB]);
		if (!process_options(values[OPTIONS_SUB], value_len[OPTIONS_SUB], &isolation, &noisolation, &consistency,
				&noconsistency))
		{
			ERROR_MSG_RETURN("Inconsistent values in OPTIONS string: ", value_len[OPTIONS_SUB], values[OPTIONS_SUB]);
		}
		count = 0;
		ptr = values[OPTIONS_SUB];
		if (isolation)
		{
			ADD_STRING(count, ptr, STRLEN(HASHT_OPT_ISOLATION), HASHT_OPT_ISOLATION, max_output_len);
		}
		else if (noisolation)
		{
			ADD_STRING(count, ptr, STRLEN(HASHT_OPT_NOISOLATION), HASHT_OPT_NOISOLATION, max_output_len);
		}
		if (consistency)
		{
			ADD_COMMA_IF_NEEDED(count, ptr, max_output_len);
			ADD_STRING(count, ptr, STRLEN(HASHT_OPT_CONSISTENCY), HASHT_OPT_CONSISTENCY, max_output_len);
		}
		else if (noconsistency)
		{
			ADD_COMMA_IF_NEEDED(count, ptr, max_output_len);
			ADD_STRING(count, ptr, STRLEN(HASHT_OPT_NOCONSISTENCY), HASHT_OPT_NOCONSISTENCY, max_output_len);
		}
		*ptr = '\0';
		value_len[OPTIONS_SUB] = STRLEN(values[OPTIONS_SUB]);
	} else
		value_len[OPTIONS_SUB] = 0;
	UPDATE_TRIG_PTRS(values[OPTIONS_SUB], values[PIECES_SUB], value_len[OPTIONS_SUB], max_output_len);
	GET_CLI_STR_AND_CHECK("PIECES", pieces_present, max_output_len, process_pieces, values[PIECES_SUB], value_len[PIECES_SUB],
			      values[ZDELIM_SUB], "");
	GET_CLI_STR_AND_CHECK("ZDELIM", zdelim_present, max_output_len, process_delim, values[ZDELIM_SUB], value_len[ZDELIM_SUB],
			      values[XECUTE_SUB], "Error parsing ZDELIM string: ");
	/* Since process_xecute() does a test compile of the xecute string, it changes the parsing table from MUPIP TRIGGER
 	 * to GT.M.  To avoid problems with saving and restoring the parse state (which would have to be done with globals
 	 * that are static in cli_parse.c), it is much easier to put process_xecute() last in the list of qualifiers to check -
 	 * solving the problem.  In other words, don't try to neaten things up by making the qualifiers alphabetical!
 	 */
	if (CLI_PRESENT == (xecute_present = cli_present("XECUTE")))
	{
		GET_CLI_STR("XECUTE", max_output_len, values[XECUTE_SUB], value_len[XECUTE_SUB]);
		out_multi_line_xecute |= (value_len[XECUTE_SUB] == XTENDED_START_LEN)
			&& (0 == memcmp(XTENDED_START, values[XECUTE_SUB], XTENDED_START_LEN));
		if (!process_xecute(values[XECUTE_SUB], &value_len[XECUTE_SUB], out_multi_line_xecute))
		{
			ptr = values[XECUTE_SUB];
			len = value_len[XECUTE_SUB];
			ERROR_MSG_RETURN("Error parsing XECUTE string: ", len, ptr);
		}
	} else
		value_len[XECUTE_SUB] = 0;
	if (!in_multi_line_xecute)
	{
		max_output_len -= value_len[XECUTE_SUB];
		UPDATE_TRIG_PTRS(values[XECUTE_SUB], values[CHSET_SUB], value_len[XECUTE_SUB], max_output_len);
		ptr = values[XECUTE_SUB] + value_len[XECUTE_SUB];
		*ptr = '\0';
		if (!xecute_present && !out_multi_line_xecute)
		{
			ERROR_STR_RETURN("XECUTE value missing");
		}
		if (0 == value_len[CMD_SUB])
		{
			ERROR_STR_RETURN("COMMANDS value missing");
		}
		if (delim_present && zdelim_present)
		{
			ERROR_STR_RETURN("Can't have both DELIM and ZDELIM in same entry");
		}
		if ((delim_present || zdelim_present) && !set_present)
		{
			ERROR_STR_RETURN("DELIM and ZDELIM need a commands=SET");
		}
		if (pieces_present && (!delim_present && !zdelim_present))
		{
			ERROR_STR_RETURN("PIECES need either DELIM or ZDELIM");
		}
		if (MAX_HASH_INDEX_LEN < (trigvn_len + 1 + value_len[GVSUBS_SUB] + 1 + value_len[DELIM_SUB] + 1
					  + value_len[ZDELIM_SUB] + 1 + value_len[XECUTE_SUB] + 1))
		{
			ERROR_STR_RETURN("Entry too large to properly index");
		}
	}

	*multi_line_xecute = out_multi_line_xecute;
	*max_len = max_output_len;
	return TRIG_SUCCESS;
}
