/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef REPL_INSTANCE_DUMP_INCLUDED
#define REPL_INSTANCE_DUMP_INCLUDED

#define	TIME_DISPLAY_FAO	" !19AD"

#define	DASHES_NO_OFFSET	"----------------------------------------------------------"
#define	DASHES_YES_OFFSET	"----------------------------------------------------------------------------"
#define	PRINT_DASHES		util_out_print(detail_specified ? DASHES_YES_OFFSET : DASHES_NO_OFFSET, TRUE)

GBLREF	boolean_t	detail_specified;

void	repl_inst_dump_filehdr(repl_inst_hdr_ptr_t repl_instance);
void	repl_inst_dump_gtmsrclcl(gtmsrc_lcl_ptr_t gtmsrclcl_ptr);
void	repl_inst_dump_history_records(char *inst_fn, int4 num_histinfo);
void	repl_dump_histinfo(FILE *log_fp, boolean_t stamptime, boolean_t flush, char *start_text, repl_histinfo *cur_histinfo);
void	repl_inst_dump_jnlpoolctl(jnlpool_ctl_ptr_t jnlpool_ctl);
void	repl_inst_dump_gtmsourcelocal(gtmsource_local_ptr_t gtmsourcelocal_ptr);

void	mupcli_get_offset_size_value(uint4 *offset, uint4 *size, gtm_uint64_t *value, boolean_t *value_present);
void	mupcli_edit_offset_size_value(sm_uc_ptr_t buff, uint4 offset, uint4 size, gtm_uint64_t value, boolean_t value_present);

#endif /* REPL_INSTANCE_DUMP_INCLUDED */
