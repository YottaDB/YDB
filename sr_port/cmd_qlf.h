/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef CMD_QLF_H_INCLUDED
#define CMD_QLF_H_INCLUDED

typedef struct
{
	uint4		qlf;
	mval		object_file;
	mval		list_file;
	mval		ceprep_file;
	mval		rtnname;
} command_qualifier;

typedef struct
{	unsigned short	page;		/* page number */
	unsigned short	list_line;	/* listing line number */
	unsigned short	lines_per_page;
	unsigned short	space;		/* spacing */
} list_params;

/* command qualifer bit masks */
#define CQ_LIST			(1 << 0)	/* 0x0001 */
#define CQ_MACHINE_CODE		(1 << 1)	/* 0x0002 */
#define CQ_CROSS_REFERENCE	(1 << 2)	/* 0x0004 */
#define CQ_DEBUG		(1 << 3)	/* 0x0008 */
#define CQ_OBJECT		(1 << 4)	/* 0x0010 */
#define CQ_WARNINGS		(1 << 5)	/* 0x0020 */
#define CQ_IGNORE		(1 << 6)	/* 0x0040 */
#define CQ_LOWER_LABELS		(1 << 7)	/* 0x0080 */
#define CQ_LINE_ENTRY		(1 << 8)	/* 0x0100 */
#define CQ_CE_PREPROCESS        (1 << 9)	/* 0x0200 */
#define CQ_INLINE_LITERALS	(1 << 10)	/* 0x0400 */
#define CQ_ALIGN_STRINGS	(1 << 11)	/* 0x0800 */
#define CQ_UTF8			(1 << 12)	/* 0x1000 */
#define CQ_NAMEOFRTN		(1 << 13)	/* 0x2000 */
#define CQ_DYNAMIC_LITERALS	(1 << 14)	/* 0x4000 -- Set via environmental variable gtm_dynamic_literals (gtm_logicals.h) */

/* TODO: add CQ_ALIGN_STRINGS to the default list below when alignment is supported */
#define CQ_DEFAULT (CQ_WARNINGS | CQ_OBJECT | CQ_IGNORE | CQ_LOWER_LABELS | CQ_LINE_ENTRY | CQ_INLINE_LITERALS)

#define LISTTAB 10
#define PG_WID 132

#define INIT_CMD_QLF_STRINGS(CMD_QLF, OBJ_FILE, LIST_FILE, CEPREP_FILE, SIZE)		\
{											\
	CMD_QLF.object_file.str.addr = OBJ_FILE;					\
	CMD_QLF.object_file.str.len = SIZE;						\
	CMD_QLF.list_file.str.addr = LIST_FILE;						\
	CMD_QLF.list_file.str.len = SIZE;						\
	CMD_QLF.ceprep_file.str.addr = CEPREP_FILE;					\
	CMD_QLF.ceprep_file.str.len = SIZE;						\
}

typedef struct src_line_type
{
	struct
	{
		struct src_line_type *fl,*bl;
	} que;
	char	*addr;
	int4	line;
} src_line_struct;

void zl_cmd_qlf(mstr *quals, command_qualifier *qualif);
void get_cmd_qlf(command_qualifier *qualif);

#endif /* CMD_QLF_H_INCLUDED */
