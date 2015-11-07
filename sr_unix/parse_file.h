/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef PARSE_FILE_H_INCLUDED
#define PARSE_FILE_H_INCLUDED

#define MAX_FBUFF	255		/* Max file size (why different from MAX_PATH[_LEN]/GTM_PATH_MAX ? */
#define DEF_DBEXT	"*.dat"
#define DEF_NODBEXT	"*"

typedef struct parse_blk_struct
{
	unsigned char	b_esl;		/* Resultant name length */
	unsigned char	b_node;		/* Length of node name at front - db opening only */
	unsigned char	b_dir;		/* Length of directory path */
	unsigned char	b_name;		/* Length of file name */
	unsigned char	b_ext;		/* Length of extension */
	unsigned char	def1_size;	/* Default 1 string size */
	char		*def1_buf;	/* Default 1 buffer */
	unsigned char	def2_size;	/* Default 2 string size */
	char		*def2_buf;	/* Default 2 buffer */
	unsigned char	buff_size;	/* Result buffer size */
	char		*buffer;	/* Result buffer */
	int4		fnb;		/* Parse result characteristics */
	int4		fop;		/* Parse options SYNTAX_ONLY only */
	char		*l_node,	/* Pointer to node specification - db opening only */
			*l_dir,		/* Pointer to directory path string */
			*l_name,	/* Pointer to file name string */
			*l_ext;		/* Pointer to extension string */
} parse_blk;

typedef struct plength_struct
{
	union
	{
		int4	pint;
		struct
		{
			unsigned char	b_esl;	/* Resultant name length */
			unsigned char	b_dir;	/* Length of directory path */
			unsigned char	b_name;	/* Length of file name */
			unsigned char	b_ext;	/* Length of extension */
		} pblk;
	} p;
} plength;

#define F_HAS_EXT	1	/* 0x01 If file has explicit extension */
#define F_HAS_NAME	2	/* 0x02 If file has explicit name */
#define F_HAS_DIR	4	/* 0x04 If file has explicit directory path */
#define F_WILD_NAME	8	/* 0x08 If there is a wild card character in the name */
#define F_WILD_DIR	16	/* 0x10 If there is a wild card character in the directory */
#define F_WILD		24	/* 0x18 If there is a wild card character in the result (dir or name) */
#define F_HAS_NODE	32	/* 0x20 If there is a node specification on the front - db opening only */

#define V_HAS_EXT	0	/* Bit offsets for F_ constants */
#define V_HAS_NAME	1
#define V_HAS_DIR	2
#define V_WILD_NAME	3
#define V_WILD_DIR	4
#define V_HAS_NODE	5	/* DB opening only */

#define F_SYNTAXO	1	/* SYNTAX ONLY - Otherwise checks on directory existence returning ERR_FILENOTFOUND if
				 * directory does not exist or is not a directory.
				 */
#define F_PARNODE	2	/* Look for a node specification - db opening only */

/* Sets the relevant length fields in a plength-typed structure:
 *  b_esl  - full length of PATH;
 *  b_dir  - length of the PATH preceding the file name (includes the slash);
 *  b_name - length of the file name in PATH (excludes the extension); and
 *  b_ext  - length of the file name extension.
 * Note that PLEN is a pointer-type argument. ABSOLUTE expects the indication of whether the PATH is absolute or relative.
 */
#define SET_LENGTHS(PLEN, PATH, LENGTH, ABSOLUTE)								\
{														\
	int		i;											\
	boolean_t	seen_ext;										\
														\
	(PLEN)->p.pblk.b_esl = LENGTH;										\
	(PLEN)->p.pblk.b_ext = 0;										\
	for (i = LENGTH - 1, seen_ext = FALSE; i >= 0; i--)							\
	{													\
		if ('.' == *((PATH) + i))									\
		{												\
			if (!seen_ext)										\
			{											\
				(PLEN)->p.pblk.b_ext = LENGTH - i;						\
				seen_ext = TRUE;								\
			}											\
		} else if ('/' == *((PATH) + i))								\
			break;											\
	}													\
	assert((i >= 0) || !(ABSOLUTE)); /* On UNIX absolute paths must have '/'. */				\
	(PLEN)->p.pblk.b_dir = i + 1;										\
	(PLEN)->p.pblk.b_name = LENGTH - (PLEN)->p.pblk.b_dir - (PLEN)->p.pblk.b_ext;				\
}

/* Canonicalize the path by appropriately removing '.' and '..' path modifiers. Note that all* modifications are
 * performed on the passed string directly.
 */
#define CANONICALIZE_PATH(PATH)											\
{														\
	char		*src, *dst;										\
	char		cur_char;										\
	boolean_t	need_slash;										\
														\
	src = dst = (PATH);											\
	assert('/' == *src);											\
	need_slash = FALSE;											\
	while ('\0' != (cur_char = *src++))									\
	{													\
		if ('/' == cur_char)										\
		{	/* Current character is '/'. If it is the last one, do not append a trailing slash. */	\
			if ('\0' == (cur_char = *src++))							\
				break;										\
			if ('/' == cur_char)									\
			{	/* Current sequence is '//'. Restart the loop from the second slash. */		\
				src--;										\
				need_slash = TRUE;								\
			} else if ('.' == cur_char)								\
			{	/* Current sequence is '/.'. We need to examine a few potential scenarios. In	\
				 * particular, if we are at the last character, no need to append anything.	\
				 */										\
				if ('\0' == (cur_char = *src++))						\
					break;									\
				if ('/' == cur_char)								\
				{	/* Current sequence is '/./'. Restart the loop from the second '/'. */	\
					src--;									\
					need_slash = FALSE;							\
				} else if ('.' == cur_char)							\
				{	/* Current sequence is '/..'. If the next character is '/' or we are at	\
					 * the end of the line, snip off one directory from the tail, if found.	\
					 */									\
					cur_char = *src++;							\
					if (('\0' == cur_char) || ('/' == cur_char))				\
					{	/* Find an earlier '/'. Reset to '/' if not found. */		\
						while (--dst >= (PATH))						\
							if ('/' == *dst)					\
								break;						\
						if ((PATH) > dst)						\
						{								\
							need_slash = TRUE;					\
							dst = (PATH);						\
						} else								\
							need_slash = FALSE;					\
						src--;								\
					} else									\
					{	/* Current sequence is '/..<x>', where x is not '/' or '\0'. */	\
						need_slash = FALSE;						\
						*dst++ = '/';							\
						*dst++ = '.';							\
						*dst++ = '.';							\
						*dst++ = cur_char;						\
					}									\
				} else										\
				{	/* Current sequence is '/.<x>', where x is not '/' or '.' or '\0'. */	\
					need_slash = FALSE;							\
					*dst++ = '/';								\
					*dst++ = '.';								\
					*dst++ = cur_char;							\
				}										\
			} else											\
			{	/* Current sequence is '/<x>', where x is not '/' or '.' or '\0'. */		\
				need_slash = FALSE;								\
				*dst++ = '/';									\
				*dst++ = cur_char;								\
			}											\
		} else												\
		{	/* Current character is not '/', so write it. But prepend it with a '/' if we have	\
			 * previously indicated a need for one.							\
			 */											\
			if (need_slash)										\
				*dst++ = '/';									\
			need_slash = FALSE;									\
			*dst++ = cur_char;									\
		}												\
	}													\
	assert(dst >= (PATH));											\
	/* If we did not have anything to put in the canonicalized path, default to '/'. */			\
	if (dst == (PATH))											\
		*dst++ = '/';											\
	*dst = '\0';												\
}

/* Escape all '[' and ']' characters to prevent glob() from trying to match sets enclosed in them. */
#define ESCAPE_BRACKETS(ORIG_PATH, RES_PATH)									\
{														\
	char		*src, *dst;										\
	char		cur_char;										\
	boolean_t	has_slash;										\
														\
	src = ORIG_PATH;											\
	dst = RES_PATH;												\
	has_slash = FALSE;											\
	while ('\0' != (cur_char = *src++))									\
	{													\
		if ('\\' == cur_char)										\
			has_slash = !has_slash;									\
		else if (('[' == cur_char) || (']' == cur_char))						\
		{												\
			if (!has_slash)										\
				*dst++ = '\\';									\
			else											\
				has_slash = FALSE;								\
		} else if (has_slash)										\
			has_slash = FALSE;									\
		*dst++ = cur_char;										\
	}													\
	*dst = '\0';												\
}

int4 parse_file(mstr *file, parse_blk *pblk);

#endif /* PARSE_FILE_H_INCLUDED */
