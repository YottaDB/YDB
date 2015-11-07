/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

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

int4 parse_file(mstr *file, parse_blk *pblk);
