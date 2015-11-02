/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define MAX_FBUFF	255		/* max file size (why different from MAX_PATH[_LEN]? */
#define DEF_DBEXT	"*.dat"
#define DEF_NODBEXT	"*"

typedef struct parse_blk_struct
{
	unsigned char	b_esl;		/* resultant name length */
	unsigned char	b_node;		/* length of node name at front - db opening only */
	unsigned char	b_dir;		/* length of directory path */
	unsigned char	b_name;		/* length of file name */
	unsigned char	b_ext;		/* length of extension */
	unsigned char	def1_size;	/* default 1 string size */
	char		*def1_buf;	/* default 1 buffer */
	unsigned char	def2_size;	/* default 1 string size */
	char		*def2_buf;	/* default 1 buffer */
	unsigned char	buff_size;	/* result buffer size */
	char		*buffer;	/* result buffer */
	int4		fnb;		/* parse result characteristics */
	int4		fop;		/* parse options SYNTAX_ONLY only */
	char		*l_node,	/* pointer to node specification - db opening only */
			*l_dir,		/* pointer to directory path string */
			*l_name,	/* pointer to file name string */
			*l_ext;		/* pointer to extension string */
} parse_blk;

typedef struct plength_struct
{
	union
	{
		int4	pint;
		struct
		{
			unsigned char	b_esl;	/* resultant name length */
			unsigned char	b_dir;	/* length of directory path */
			unsigned char	b_name;	/* length of file name */
			unsigned char	b_ext;	/* length of extension */
		} pblk;
	} p;
} plength;

#define F_HAS_EXT	1	/* if file has explicit extension */
#define F_HAS_NAME	2	/* if file has explicit name */
#define F_HAS_DIR	4	/* if file has explicit directory path */
#define F_WILD_NAME	8	/* if there is a wild card character in the name */
#define F_WILD_DIR	16	/* if there is a wild card character in the directory */
#define F_WILD		24	/* if there is a wild card character in the result */
#define F_HAS_NODE	32	/* if there is a node specification on the front - db opening only */

#define V_HAS_EXT	0	/* bit offsets for F_ constants */
#define V_HAS_NAME	1
#define V_HAS_DIR	2
#define V_WILD_NAME	3
#define V_WILD_DIR	4
#define V_HAS_NODE	5	/* db opening only */

#define F_SYNTAXO	1	/* SYNTAX ONLY */
#define F_PARNODE	2	/* look for a node specification - db opening only */

int4 parse_file(mstr *file, parse_blk *pblk);
