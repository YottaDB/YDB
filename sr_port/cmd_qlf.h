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

typedef struct
{
	uint4	qlf;
	mval		object_file;
	mval		list_file;
	mval		ceprep_file;
}command_qualifier;


typedef struct
{	unsigned short	page;		/* page number */
	unsigned short	list_line;	/* listing line number */
	unsigned short	lines_per_page;
	unsigned short	space;		/* spacing */
}list_params;

#define CQ_LIST			1
#define CQ_MACHINE_CODE		2
#define CQ_CROSS_REFERENCE	4
#define CQ_DEBUG		8
#define CQ_OBJECT		16
#define CQ_WARNINGS		32
#define CQ_IGNORE		64
#define CQ_LOWER_LABELS		128
#define CQ_LINE_ENTRY		256
#define CQ_CE_PREPROCESS        512

#define CQ_DEFAULT (CQ_WARNINGS | CQ_OBJECT | CQ_IGNORE | CQ_LOWER_LABELS | CQ_LINE_ENTRY)

#define LISTTAB 10
#define PG_WID 132


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
