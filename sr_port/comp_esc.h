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

struct ce_sentinel_desc
{
	char			*escape_sentinel;
	int4			escape_length;
	int4			(*user_routine)();
	struct ce_sentinel_desc	*next;
};

int ce_init(void);
void ce_substitute(struct ce_sentinel_desc *shp, int4 source_col, int4 *skip_ct);
void close_ceprep_file(void);
void open_ceprep_file(void);
void put_ceprep_line(void);

