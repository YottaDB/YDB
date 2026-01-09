/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <glob.h>
#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"

#include "io.h"
#include "iosp.h"
#include "zroutines.h"
#include "parse_file.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "zro_shlibs.h"
#include "gtm_limits.h"

error_def(ERR_DIRONLY);
error_def(ERR_FILEPARSE);
error_def(ERR_FSEXP);
error_def(ERR_INVZROENT);
error_def(ERR_MAXARGCNT);
error_def(ERR_NOLBRSRC);
error_def(ERR_QUALEXP);
error_def(ERR_ZROSYNTAX);
error_def(ERR_FILEPATHTOOLONG);
error_def(ERR_TEXT);

STATICFNDEF boolean_t so_file_ending(char *str, int len);
STATICFNDEF boolean_t has_glob_char(char *str, int len);

/* Used to free malloced memory on error. */
GBLDEF	char	**zro_char_buff = NULL;

#define	COPY_TO_EXP_BUFF(STR, LEN, EXP_BUFF, EXP_LEN_USED, EXP_ALLOC_LEN)	\
MBSTART {									\
	int	new_len_used;							\
	char	*tmpExpBuff;							\
										\
	assert(LEN);								\
	new_len_used = LEN + EXP_LEN_USED;					\
	if (EXP_ALLOC_LEN < new_len_used)					\
	{	/* Current allocation not enough. Allocate more. */		\
		EXP_ALLOC_LEN = (new_len_used * 2);				\
		tmpExpBuff = (char *)malloc(EXP_ALLOC_LEN);			\
		memcpy(tmpExpBuff, EXP_BUFF, EXP_LEN_USED);			\
		free(EXP_BUFF);							\
		EXP_BUFF = tmpExpBuff;						\
	}									\
	memcpy((char *)EXP_BUFF + EXP_LEN_USED, STR, LEN);			\
	EXP_LEN_USED += LEN;							\
	assert(EXP_LEN_USED <= EXP_ALLOC_LEN);					\
} MBEND

#define	COPY_ZROENT_AS_APPROPRIATE(OP, PBLK, EXP_BUFF, EXP_LEN_USED, EXP_ALLOC_LEN)				\
{														\
	char	*has_envvar;											\
														\
	has_envvar = memchr(OP->str.addr, '$', OP->str.len);							\
	if (has_envvar)												\
	{	/* Copy object/source directory (with env vars expanded) into what becomes final $ZROUTINES */	\
		COPY_TO_EXP_BUFF(PBLK.buffer, PBLK.b_esl, EXP_BUFF, EXP_LEN_USED, EXP_ALLOC_LEN);		\
	} else													\
	{	/* Copy object/source directory as is (e.g. ".") into what becomes final $ZROUTINES */		\
		COPY_TO_EXP_BUFF(OP->str.addr, OP->str.len, EXP_BUFF, EXP_LEN_USED, EXP_ALLOC_LEN);		\
	}													\
}

#define SO_EXT ".so"
#define CWD_PATH_START "./"

/* Checks whether character is the start of a special glob pattern.
 * Other characters have special meaning in glob patterns,
 * but only inside of square brackets.
 * The glob characters checked are '*', '\', '?', and '['.
 * Documentation https://man7.org/linux/man-pages/man7/glob.7.html
 * @param char character the character you want to evaluate.
 * @return
 * 	TRUE if character is glob character
 * 	FALSE otherwise
 */
#define IS_GLOB_CHAR(check_char) (('*' == check_char) || ('[' == check_char) || ('?' == check_char) || ('\\' == check_char))

/* Used to handle a fileparse error. does nothing if fileparse error due to glob pattern
 * @param char *strAddr a pointer to the string to check for glob pattern
 * @param int strLen length to search.
 */
#define FILEPARSE_ERR(strAddr, strLen, status, tok, num_glob_matches)						\
{														\
	if (!has_glob_char(strAddr, strLen))									\
	{													\
		if ((ERR_FILEPATHTOOLONG == status) && (0 != num_glob_matches))					\
		{												\
			RTS_ERROR_CSA_ABT(NULL,  VARLSTCNT(10) ERR_ZROSYNTAX, 2, tok.len, tok.addr,		\
					ERR_FILEPATHTOOLONG, 0, ERR_TEXT, 2, LEN_AND_LIT(			\
			"A glob pattern matched a filename longer than 255 characters."));			\
		} else												\
		{												\
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_ZROSYNTAX, 2, str_input->len, str_input->addr,	\
					ERR_FILEPARSE, 2, tok.len, tok.addr, status);				\
		}												\
	}													\
}

/* Used to expand the space in str if needed in zro_load. Will update all variables that are pointing at the old buffer.
 * If EXPAND_BUFF finds that the buffer is already big enough, it does nothing.
 * If the buffer needs to be expanded, it will at minimum double in size.
 * 	str *mstr whose address needs more space. Note that str->len does not include the null byte.
 * 	int expand_amount the number of new bytes of space needed.
 * 	int str_max_size the current allocated size of str->addr in bytes. Will be updated in this macro.
 * 	char *lp should be pointing to some place in the buffer, will be updated to point to the same place in the new buffer.
 * 	char *top will point to the null byte at the end of the buffer.
 * 	mstr tok tok.addr points to somewhere in the buffer, will be updated to point to the same place in the new buffer.
 */
#define EXPAND_BUFF(str, expand_amount, str_max_size, lp, top, tok)							\
{															\
	int tok_index, lp_index, min_size_needed;									\
	char *new_buff;													\
															\
	/* str->len does not include the null byte, so +1 needed. */							\
	min_size_needed = str->len + expand_amount + 1;									\
	if (min_size_needed > str_max_size)										\
	{														\
		str_max_size = MAX(min_size_needed, (str_max_size*2));							\
		tok_index = tok.addr - str->addr;									\
		lp_index = lp - str->addr;										\
		new_buff = malloc(str_max_size * SIZEOF(char));								\
		memcpy(new_buff, str->addr, str->len + 1);								\
		free(str->addr);											\
		str->addr = new_buff;											\
		lp = str->addr + lp_index;										\
		top = str->addr + str->len;										\
		tok.addr = str->addr + tok_index;									\
	}														\
}

/* Routine to parse the value of $ZROUTINES and create the list of structures that define the (new) routine
 * search list order and define which (if any) directories can use auto-relink.
 *
 * Parameter:
 *   str   - string to parse (usually dollar_zroutines)
 *
 * Return code:
 *   none
 */
void zro_load(mstr *str_input)
{
	unsigned		toktyp, status;
	boolean_t		arlink_thisdir_enable, arlink_enabled, found_pattern, added_entry, added_slash;
	mstr			tok, transtr, str_obj;
	char			*lp, *top;
	zro_ent			array[ZRO_MAX_ENTS], *op, *zro_root_ptr;
	int			oi, si, total_ents, max_str_len;
	struct  stat		outbuf;
	int			stat_res, start_len, size_added;
	char			tranbuf[MAX_FN_LEN + 3];
	int			exp_len_used, exp_alloc_len, glob_stat, path_index, filepath_len, num_glob_matches;
	int			str_offsets[ZRO_MAX_ENTS], array_index, entry_type, glob_str_len;
	char			*exp_buff, *filepath;
	parse_blk		pblk;
	size_t			root_alloc_size;	/* For SCI */
	mstr			*str;
	glob_t			gstruct;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	str = &str_obj;
	str->addr = malloc(str_input->len + 1);
	/* Assumes not in recursive call and memory was properly reset from all previous calls. */
	assert(NULL == zro_char_buff);
	/* & is used as str->addr might change (due to EXPAND_BUFF calls)
	 * &(str->addr) will not change till the end of the function.
	 */
	zro_char_buff = &(str->addr);
	memcpy(str->addr, str_input->addr, str_input->len);
	str->addr[str_input->len] = '\0';
	/* str->len does not include the null byte. */
	str->len = str_input->len;
	/* Size of malloced space for str->addr. */
	max_str_len = str->len + 1;
	num_glob_matches = 0;
	arlink_enabled = FALSE;
	memset(array, 0, SIZEOF(array));
	lp = str->addr;
	top = lp + str->len;
	while ((lp < top) && (ZRO_DEL == *lp))	/* Bypass leading blanks */
		lp++;
	array[0].type = ZRO_TYPE_COUNT;
	array[0].count = 0;
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = tranbuf;
	toktyp = zro_gettok(&lp, top, &tok);
	/* Will be treated as an empty string if all entries are no match glob patterns. */
	added_entry = FALSE;
	/* Parse supplied string. */
	for (oi = 1;;)
	{
		if (ZRO_EOL == toktyp)
			break;
		if (ZRO_IDN != toktyp)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_ZROSYNTAX, 2, str_input->len, str_input->addr, ERR_FSEXP);
		if (ZRO_MAX_ENTS <= (oi + 1))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_ZROSYNTAX, 2, str_input->len, str_input->addr,
				ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
		/* We have type ZRO_IDN (an identifier/name of some sort). See if token has a "*" (ZRO_ALF) at the end
		 * of it indicating that it is supposed to (1) be a directory and not a shared library and (2) that the
		 * user desires this directory to have auto-relink capability.
		 */
		arlink_thisdir_enable = FALSE;
		/* All platforms allow the auto-relink indicator on object directories but only autorelink able platforms
		 * (#ifdef AUTORELINK_SUPPORTED is set) do anything with it. Other platforms just ignore it. Specifying
		 * "*" at end of non-object directories causes an error further downstream (FILEPARSE) when the "*" is
		 * not stripped off the file name - unless someone has managed to create a directory with a "*" suffix.
		 */
		if (ZRO_ALF == *(tok.addr + tok.len - 1))
		{	/* Auto-relink is indicated */
			arlink_thisdir_enable = TRUE;
			--tok.len;		/* Remove indicator from name so we can use it */
			assert(0 <= tok.len);
		}
		if (MAX_FN_LEN < tok.len)
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_ZROSYNTAX, 2, tok.len + arlink_thisdir_enable, tok.addr,
					ERR_FILEPATHTOOLONG, 0);
		}
		/* Run specified directory through parse_file to fill in any missing pieces and get some info on it */
		pblk.buff_size = MAX_FN_LEN;	/* Don't count null terminator here */
		pblk.fnb = 0;
		status = parse_file(&tok, &pblk);
		if (!(status & 1))
		{	/* If env variable expanded, there might be glob patterns in the env var. */
			if (memchr(tok.addr, '$', tok.len))
			{
				FILEPARSE_ERR(tranbuf, pblk.b_esl, status, tok, num_glob_matches);
			} else
			{
				FILEPARSE_ERR(tok.addr, tok.len, status, tok, num_glob_matches);
			}
		}
		tranbuf[pblk.b_esl] = 0;		/* Needed for some subsequent STAT_FILE */
		STAT_FILE(tranbuf, &outbuf, stat_res);
		if (-1 == stat_res)
		{
			/* As we did not find a directory, put the trailing * back. */
			tok.len = tok.len + arlink_thisdir_enable;
			/* If there is an environment variable, we give the relative path from tranbuf.
			 * It is not possible to tell what the relative part of the path is in that case,
			 * so we will have to make it an absolute path. If there is no environment variable
			 * we can pass tok directly into glob.
			 */
			added_slash = FALSE;
			if (!memchr(tok.addr, '$', tok.len))
			{
				assert(SIZEOF(tranbuf) > tok.len + 2);
				if (!memchr(tok.addr, '/', tok.len))
				{	/* dlopen, used in zro_shlibs_find, requires a '/'
					 * in the filename to treat it as a relative path.
					 * https://man7.org/linux/man-pages/man3/dlopen.3.html
					 */
					added_slash = TRUE;
					memcpy(tranbuf, CWD_PATH_START, 2);
					memcpy(tranbuf + 2, tok.addr, tok.len);
					tranbuf[2 + tok.len] = '\0';
					glob_str_len = tok.len + 2;
				} else
				{
					memcpy(tranbuf, tok.addr, tok.len);
					tranbuf[tok.len] = '\0';
					glob_str_len = tok.len;
				}
			} else
				glob_str_len = pblk.b_esl;
			found_pattern = has_glob_char(tranbuf, glob_str_len);
			if (found_pattern && (so_file_ending(tok.addr, tok.len)))
			{
				/* If we found a glob character in the filepath and the filename ends in .so
				 * then the filename is treated as a glob pattern.
				 * So we use glob to check for pattern matches.
				 * Even if no matches are found, that is not considered an error as it is with a normal
				 * filename.
				 * The structure of this code is that the currently found glob pattern will have all
				 * the matching files added to str to be analyzed in a later iteration.
				 */
				if (ZRO_LBR == *lp)
				{	/* We want to show the user the libsource that was invalid,
					 * so add it to tok before printing tok.
					 */
					while ((tok.addr + tok.len < top) && (ZRO_RBR != tok.addr[tok.len - 1]))
						tok.len++;
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_ZROSYNTAX, 2, tok.len, tok.addr,
							ERR_NOLBRSRC);
				}
				/* In case of an error, we need to zero gstruct so that globfree is safe to call. */
				memset(&gstruct, 0, SIZEOF(gstruct));
				glob_stat = glob(tranbuf, GLOB_ERR, NULL, &gstruct);
				if ((0 == glob_stat) || (GLOB_NOMATCH == glob_stat))
				{	/* glob_stat==GLOB_NOMATCH if no files were found that match the pattern. */
					if (GLOB_NOMATCH == glob_stat)
						gstruct.gl_pathc = 0; /* Typically already true but not guaranteed. */
					start_len = str->len;
					for (path_index = 0; path_index < gstruct.gl_pathc; path_index++)
					{
						filepath = gstruct.gl_pathv[path_index];
						filepath_len = strlen(filepath);
						/* As pattern must end in .so, filepath also must end in .so. */
						assert(so_file_ending(filepath, filepath_len));
						/* If not a shared library, ignore file. */
						if (!(zro_shlibs_find(filepath, SUCCESS)))
							continue;
						if (added_slash)
						{
							filepath = filepath + 2;
							filepath_len = filepath_len - 2;
							assert(0 < filepath_len);
						}
						/* Length of new entry INCLUDING space. */
						EXPAND_BUFF(str, filepath_len + 1,
							max_str_len, lp, top, tok);
						/* Copy the new filename. */
						memcpy(str->addr + str->len + 1, filepath, filepath_len + 1);
						/* Add space separator. */
						str->addr[str->len] = ' ';
						str->len += filepath_len + 1;
						top = str->addr + str->len;
						num_glob_matches++;
					}
					size_added = str->len - start_len;
					if (0 != size_added)
					{
						EXPAND_BUFF(str, size_added, max_str_len, lp, top, tok);
						memmove(tok.addr + tok.len + size_added, tok.addr + tok.len,
							(str->len - (tok.addr - str->addr)) - tok.len);
						memcpy(tok.addr + tok.len, str->addr + str->len, size_added);
						str->addr[str->len] = '\0';
					}
					assert(top == str->addr + str->len);
					globfree(&gstruct);
					toktyp = zro_gettok(&lp, top, &tok);
					if (ZRO_EOL == toktyp)
						break;
					else if (ZRO_DEL == toktyp)
						toktyp = zro_gettok(&lp, top, &tok);
					else
					{
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_ZROSYNTAX, 2,
							tok.len + 1, tok.addr);
					}
					continue;
				} else
				{	/* glob error (other than no match found). */
					globfree(&gstruct);
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_ZROSYNTAX, 2, str_input->len,
						str_input->addr, ERR_FILEPARSE, 2, tok.len, tok.addr, errno);
				}
			}
			if ((found_pattern && (!so_file_ending(tok.addr, tok.len))) && !arlink_thisdir_enable)
			{	/* If arlink_thisdir_enable, assume user is looking for directory, not using glob. */
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZROSYNTAX, 2, tok.len, tok.addr,
					ERR_TEXT, 2, LEN_AND_LIT("glob filename pattern must end in .so"));
			} else
			{	/* Subtract arlink_thisdir_enable back off of tok.len to be consistent with other errors. */
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_ZROSYNTAX, 2, str_input->len, str_input->addr,
					ERR_FILEPARSE, 2, tok.len - arlink_thisdir_enable, tok.addr, errno);
			}
		}
		added_entry = TRUE;
		if (S_ISREG(outbuf.st_mode))
		{	/* Regular file - a shared library file */
			if (arlink_thisdir_enable)
				/* Auto-relink indicator on shared library not permitted */
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZROSYNTAX, 2, str_input->len, str_input->addr,
					ERR_FILEPARSE, 2, tok.len, tok.addr);
			array[oi].shrlib = zro_shlibs_find(tranbuf, ERROR);
			array[oi].type = ZRO_TYPE_OBJLIB;
			si = oi + 1;
		} else
		{
			if (!S_ISDIR(outbuf.st_mode))
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZROSYNTAX, 2, str_input->len, str_input->addr,
					ERR_INVZROENT, 2, tok.len, tok.addr);
			array[oi].type = ZRO_TYPE_OBJECT;
			array[oi + 1].type = ZRO_TYPE_COUNT;
			si = oi + 2;
#					ifdef AUTORELINK_SUPPORTED
#						ifdef DEBUG
				/* If env var ydb_test_autorelink_always is set in dbg version, treat every
					* object directory specified in $zroutines as if * has been additionally specified.
					*/
				if (TREF(ydb_test_autorelink_always))
					arlink_thisdir_enable = TRUE;
#						endif
			if (arlink_thisdir_enable)
			{	/* Only setup autorelink struct if it is enabled */
				if (!TREF(is_mu_rndwn_rlnkctl))
				{
					transtr.addr = tranbuf;
					transtr.len = pblk.b_esl;
					array[oi].relinkctl_sgmaddr = (void_ptr_t)relinkctl_attach(&transtr, NULL, 0);
				} else
				{	/* If zro_load() is called as a part of MUPIP RUNDOWN -RELINKCTL, then we do not
					 * want to do relinkctl_attach() on all relinkctl files at once because we leave
					 * the function holding the linkctl lock, which might potentially cause a deadlock
					 * if multiple processes are run concurrently with different $ydb_routines. However,
					 * we need a way to tell mu_rndwn_rlnkctl() which object directories are autorelink-
					 * enabled. For that we set a negative number to the presently unused count field of
					 * object directory entries in the zro_ent linked list. If we ever decide to make
					 * that value meaningful, then, perhaps, ensuring that this count remains negative
					 * in case of MUPIP RUNDOWN -RELINKCTL but has the correct absolute value would do
					 * the trick.
					 */
					array[oi].count = ZRO_DIR_ENABLE_AR;
				}
			}
#					endif
		}
		arlink_enabled |= arlink_thisdir_enable;	/* Cumulative value of enabled dirs */
		array[0].count++;
		array[oi].str.len = tok.len;
		str_offsets[oi] = tok.addr - str->addr;
		toktyp = zro_gettok(&lp, top, &tok);
		if (ZRO_LBR == toktyp)
		{
			if (ZRO_TYPE_OBJLIB == array[oi].type)
			{	/* We want to show the user the libsource that was invalid,
				 * so add it to tok before printing tok.
				 */
				while ((tok.addr + tok.len < top) && (ZRO_RBR != tok.addr[tok.len - 1]))
					tok.len++;
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_ZROSYNTAX, 2, tok.len, tok.addr,
						ERR_NOLBRSRC);
			}
			toktyp = zro_gettok(&lp, top, &tok);
			if (ZRO_DEL == toktyp)
				toktyp = zro_gettok(&lp, top, &tok);
			if ((ZRO_IDN != toktyp) && (ZRO_RBR != toktyp))
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_ZROSYNTAX, 2, str_input->len, str_input->addr,
					ERR_QUALEXP);
			array[oi + 1].count = 0;
			for (;;)
			{
				if (ZRO_RBR == toktyp)
					break;
				if (ZRO_IDN != toktyp)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_ZROSYNTAX, 2,
						str_input->len, str_input->addr, ERR_FSEXP);
				if (ZRO_MAX_ENTS <= si)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_ZROSYNTAX, 2,
						str_input->len, str_input->addr, ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
				if (MAX_FN_LEN <= tok.len)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZROSYNTAX, 2,
						str_input->len, str_input->addr, ERR_FILEPARSE, 2, tok.len, tok.addr);
				pblk.buff_size = MAX_FN_LEN;
				pblk.fnb = 0;
				status = parse_file(&tok, &pblk);
				if (!(status & 1))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_ZROSYNTAX, 2, str_input->len,
						str_input->addr, ERR_FILEPARSE, 2, tok.len, tok.addr, status);
				tranbuf[pblk.b_esl] = 0;
				STAT_FILE(tranbuf, &outbuf, stat_res);
				if (-1 == stat_res)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_ZROSYNTAX, 2, str_input->len,
						str_input->addr, ERR_FILEPARSE, 2, tok.len, tok.addr, errno);
				if (!S_ISDIR(outbuf.st_mode))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZROSYNTAX, 2,
						str_input->len, str_input->addr, ERR_DIRONLY, 2, tok.len, tok.addr);
				array[oi + 1].count++;
				array[si].type = ZRO_TYPE_SOURCE;
				array[si].str.len = tok.len;
				str_offsets[si] = tok.addr - str->addr;
				si++;
				toktyp = zro_gettok(&lp, top, &tok);
				if (ZRO_DEL == toktyp)
					toktyp = zro_gettok(&lp, top, &tok);
			}
			toktyp = zro_gettok(&lp, top, &tok);
		} else
		{
			if ((ZRO_TYPE_OBJLIB != array[oi].type) && ((ZRO_DEL == toktyp) || (ZRO_EOL == toktyp)))
			{
				if (ZRO_MAX_ENTS <= si)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_ZROSYNTAX, 2,
						str_input->len, str_input->addr, ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
				array[oi + 1].count = 1;
				array[si] = array[oi];
				str_offsets[si] = str_offsets[oi];
				array[si].type = ZRO_TYPE_SOURCE;
				si++;
			}
		}
		if (ZRO_EOL == toktyp)
			break;
		else if (ZRO_DEL == toktyp)
			toktyp = zro_gettok(&lp, top, &tok);
		else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_ZROSYNTAX, 2, str_input->len, str_input->addr);
		oi = si;
		if (0 < num_glob_matches)
			num_glob_matches--;
	}
	/* str->addr might change in EXPAND_BUFF so we do not save pointers directly in the above loop.
	 * Instead, we save the offsets in str_offsets and convert back here once all possible EXPAND_BUFF
	 * calls are done. So the values of array[array_index].str.len are filled in but the
	 * array[array_index].str.addr values will be added in this loop.
	 */
	if (added_entry)
	{
		for (array_index=0; array_index < si; array_index++)
		{
			entry_type = array[array_index].type;
			if ((ZRO_TYPE_SOURCE == entry_type) || (ZRO_TYPE_OBJLIB == entry_type) || (ZRO_TYPE_OBJECT == entry_type))
				array[array_index].str.addr = str->addr + str_offsets[array_index];
		}
	}
	else
	{ /* Null string - set default - implies current working directory only */
		array[0].count = 1;
		array[1].type = ZRO_TYPE_OBJECT;
		array[1].str.len = 0;
		array[2].type = ZRO_TYPE_COUNT;
		array[2].count = 1;
		array[3].type = ZRO_TYPE_SOURCE;
		array[3].str.len = 0;
		si = 4;
	}
	total_ents = si;
	if (TREF(zro_root))
	{
		assert((TREF(zro_root))->type == ZRO_TYPE_COUNT);
		oi = (TREF(zro_root))->count;
		assert(oi);
		for (op = TREF(zro_root) + 1; 0 < oi--;)
		{	/* Release space held by translated entries */
			assert((ZRO_TYPE_OBJECT == op->type) || (ZRO_TYPE_OBJLIB == op->type));
			if (op->str.len)
				free(op->str.addr);
			if (ZRO_TYPE_OBJLIB == (op++)->type)
				continue;	/* i.e. no sources for shared library */
			assert(ZRO_TYPE_COUNT == op->type);
			si = (op++)->count;
			for (; si-- > 0; op++)
			{
				assert(ZRO_TYPE_SOURCE == op->type);
				if (op->str.len)
					free(op->str.addr);
			}
		}
		free(TREF(zro_root));
	}
	root_alloc_size = total_ents * SIZEOF(zro_ent);	/* For SCI */
	zro_root_ptr = (zro_ent *)malloc(root_alloc_size);
	assert(NULL != zro_root_ptr);
	memcpy((uchar_ptr_t)zro_root_ptr, (uchar_ptr_t)array, root_alloc_size);
	TREF(zro_root) = zro_root_ptr;
	assert(ZRO_TYPE_COUNT == (TREF(zro_root))->type);
	oi = (TREF(zro_root))->count;
	assert(oi);
	exp_alloc_len = 512;		/* Initial allocation */
	exp_buff = (char *)malloc(exp_alloc_len);
	exp_len_used = 0;
	for (op = TREF(zro_root) + 1; 0 < oi--; )
	{
		assert((ZRO_TYPE_OBJECT == op->type) || (ZRO_TYPE_OBJLIB == op->type));
		if (op->str.len)
		{
			pblk.buff_size = MAX_FN_LEN;
			pblk.fnb = 0;
			status = parse_file(&op->str, &pblk);
			if (!(status & 1))
			{
				free(exp_buff);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_ZROSYNTAX, 2, str_input->len, str_input->addr,
					      ERR_FILEPARSE, 2, op->str.len, op->str.addr, status);
			}
			/* Copy object directory into what will become final $ZROUTINES */
			COPY_ZROENT_AS_APPROPRIATE(op, pblk, exp_buff, exp_len_used, exp_alloc_len);
			if (NULL != op->relinkctl_sgmaddr)
				COPY_TO_EXP_BUFF("*", 1, exp_buff, exp_len_used, exp_alloc_len);	/* Add "*" to objdir */
			op->str.addr = (char *)malloc(pblk.b_esl + 1);
			op->str.len = pblk.b_esl;
			memcpy(op->str.addr, pblk.buffer, pblk.b_esl);
			op->str.addr[pblk.b_esl] = 0;
		} else
			COPY_TO_EXP_BUFF(".", 1, exp_buff, exp_len_used, exp_alloc_len); /* Empty objdir == "." */
		if (ZRO_TYPE_OBJLIB != (op++)->type)
		{
			assert(ZRO_TYPE_COUNT == op->type);
			si = (op++)->count;
			COPY_TO_EXP_BUFF("(", 1, exp_buff, exp_len_used, exp_alloc_len);
			for (; 0 < si--; op++)
			{
				assert(ZRO_TYPE_SOURCE == op->type);
				if (op->str.len)
				{
					pblk.buff_size = MAX_FN_LEN;
					pblk.fnb = 0;
					status = parse_file(&op->str, &pblk);
					if (!(status & 1))
					{
						free(exp_buff);
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_ZROSYNTAX, 2,
								str_input->len, str_input->addr,
								ERR_FILEPARSE, 2, op->str.len, op->str.addr, status);
					}
					/* Copy object directory into what will become final $ZROUTINES */
					COPY_ZROENT_AS_APPROPRIATE(op, pblk, exp_buff, exp_len_used, exp_alloc_len);
					op->str.addr = (char *)malloc(pblk.b_esl + 1);
					op->str.len = pblk.b_esl;
					memcpy(op->str.addr, pblk.buffer, pblk.b_esl);
					op->str.addr[pblk.b_esl] = 0;
				} else
				{
					assert(0 == si);
					COPY_TO_EXP_BUFF(".", 1, exp_buff, exp_len_used, exp_alloc_len);
				}
				if (si)
				{	/* Add space before next srcdir */
					COPY_TO_EXP_BUFF(" ", 1, exp_buff, exp_len_used, exp_alloc_len);
				}
			}
			COPY_TO_EXP_BUFF(")", 1, exp_buff, exp_len_used, exp_alloc_len);
		}
		if (oi)
			COPY_TO_EXP_BUFF(" ", 1, exp_buff, exp_len_used, exp_alloc_len); /* Add space for next objdir */
	}
	if ((TREF(dollar_zroutines)).addr)
		free ((TREF(dollar_zroutines)).addr);
	(TREF(dollar_zroutines)).addr = (char *)malloc(exp_len_used);
	memcpy((TREF(dollar_zroutines)).addr, exp_buff, exp_len_used);
	(TREF(dollar_zroutines)).len = exp_len_used;
	free(exp_buff);
	free(str->addr);
	zro_char_buff = NULL;
	ARLINK_ONLY(TREF(arlink_enabled) = arlink_enabled);	/* Set if any zro entry is enabled for autorelink */
	(TREF(set_zroutines_cycle))++;			/* Signal need to recompute zroutines histories for each linked routine */
}

/* Checks whether str ends with ".so".
 * @return
 *	TRUE If str ends in .so
 *	FALSE otherwise
 */
STATICFNDEF boolean_t so_file_ending(char *str, int len)
{
	if (len >= (SIZEOF(SO_EXT) - 1))
		return !MEMCMP_LIT(str + (len - (SIZEOF(SO_EXT) - 1)), SO_EXT);
	else
		return FALSE;
}

/* Returns true if glob char is in str, '\', '*', '?', or '['. */
STATICFNDEF boolean_t has_glob_char(char *str, int len)
{
	int index;

	for (index = 0; index < len; index++)
	{
		if (IS_GLOB_CHAR(str[index]))
			return TRUE;
	}
	return FALSE;
}


