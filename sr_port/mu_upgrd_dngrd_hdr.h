/****************************************************************
 *								*
 *	Copyright 2005, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_UPGRD_DNGRD_HEADER_INCLUDED
#define MU_UPGRD_DNGRD_HEADER_INCLUDED

#ifdef VMS
#define BLK_HDR_INCREASE 9
#else
#define BLK_HDR_INCREASE 8
#endif

#define PRINT_JNL_FIELDS(csd)												\
{															\
	error_def(ERR_MUINFOUINT4);											\
	error_def(ERR_MUINFOUINT8);											\
	error_def(ERR_MUINFOSTR);											\
															\
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOSTR, 4, LEN_AND_LIT("Journal file name"), JNL_LEN_STR(csd));			\
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOSTR, 4, LEN_AND_LIT("Journal before imaging"),				\
			LEN_AND_STR((csd->jnl_before_image ? " TRUE" : "FALSE")));					\
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Journal buffer size"),					\
		csd->jnl_buffer_size, csd->jnl_buffer_size);								\
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Journal allocation"), csd->jnl_alq, csd->jnl_alq);	\
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Journal extension"), csd->jnl_deq, csd->jnl_deq);	\
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Journal autoswitchlimit"),				\
								csd->autoswitchlimit, csd->autoswitchlimit);		\
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Journal epoch interval"),				\
								csd->epoch_interval, csd->epoch_interval);		\
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOSTR, 4, LEN_AND_LIT(UNIX_ONLY("Journal sync_io") VMS_ONLY("Journal NOCACHE")), \
			LEN_AND_STR((csd->jnl_sync_io ? " TRUE" : "FALSE")));						\
	UNIX_ONLY(gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Journal yield limit"),			\
							csd->yield_lmt, csd->yield_lmt);)				\
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("Region sequence number"),				\
				&csd->reg_seqno, &csd->reg_seqno);							\
}

void mu_dwngrd_header(sgmnt_data *csd, v15_sgmnt_data *v15_csd);
void mu_upgrd_header(v15_sgmnt_data *v15_csd, sgmnt_data *csd);

#endif
