/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

struct startup_vector
{
	int4		argcnt;		/* number of bytes in vector */
	unsigned char	*rtn_start;
	unsigned char	*rtn_end;
	unsigned char	*zctable_start;
	unsigned char	*zctable_end;
	unsigned char	*zcpackage_begin;
	unsigned char	*zcpackage_end;
	unsigned char	**fp;
	unsigned char	*xf_tab;
	unsigned char	*frm_ptr;
	unsigned char	*base_addr;
	unsigned char	*gtm_main_inaddr;
	int4		user_stack_size;
	int4            user_spawn_flag;
	int4		user_indrcache_size;	/* defunct */
	int4		user_strpl_size;
	int4		user_io_timer;
	int4		user_write_filter;
	int4		special_input;
	int4		undef_inhib;
	int4		ctrlc_enable;
	int4		break_message_mask;
	int4		labels;
	int4		lvnullsubs;
	int4		zdir_form;
	int4		zdate_form;
	mstr		*sysid_ptr;
	unsigned char	*dlr_truth;
};
