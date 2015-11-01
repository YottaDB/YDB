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

typedef struct global_dir_path_struct
{	block_id			block;
	int4			offset;
	struct global_dir_path_struct	*next;
}global_dir_path;

typedef struct global_root_list_struct
{	block_id			root;
	global_dir_path 		*dir_path;
	struct global_root_list_struct	*link;
}global_root_list;
