/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupipbckup.h -- requires gdsfhead.h in front of this.
 */

typedef enum
{
	backup_to_file = 0,
	backup_to_exec,
	backup_to_tcp
} backup_type;

typedef enum
{
	need_to_free_space = 0,
	need_to_del_tempfile,
	need_to_rel_crit,
	num_of_clnup_stage
} clnup_stage;

typedef enum
{
	keep_going = 0,
	give_up_before_create_tempfile,
	give_up_after_create_tempfile,
	num_backup_proc_status
} backup_proc_status;

/* ATTN: the first four items in this structure need to be identical to those
 *	 in structure tp_region in tp.h
 */
typedef struct backup_reg_list_struct
{
	struct 	backup_reg_list_struct	*fPtr;
	struct 	gd_region_struct 	*reg;
	gd_id           		unique_file_id;		/* both for VMS and UNIX */
	mstr 				backup_file;
	short 				crashcnt, filler;
	backup_proc_status		not_this_time;
	backup_type			backup_to;
	sgmnt_data_ptr_t		backup_hdr;
	trans_num			tn;
	block_id			last_blk_at_last_bkup;
	int 				backup_fd;
	char				backup_tempfile[256];
} backup_reg_list;

#define BACKUP_TEMPFILE_PREFIX  "gtm_online_backup"
#ifdef UNIX
#define BACKUP_READ_SIZE        (64 * 1024)
#else
#define BACKUP_READ_SIZE	(32 * 1024 - DISK_BLOCK_SIZE)
#endif
#define BACKUP_TEMPFILE_BUFF_SIZE (BACKUP_READ_SIZE + SIZEOF(muinc_blk_hdr))
#define BLOCKING_FACTOR         32
#define STARTING_BLOCKS         16
#define EXTEND_SIZE             256
#define INTERNAL_ERROR 		"Internal MUPIP incremental backup error"
#define	MAX_TEMP_OPEN_TRY	15
#define MAX_MUBCLNUP_STAGE	2
#define DEFAULT_BKRS_PORT	6100
#define DEFAULT_BKRS_TIMEOUT	30

#define END_MSG "END OF SAVED BLOCKS"
#define HDR_MSG "END OF FILE HEADER"
#define MAP_MSG "END OF MASTER MAP"

LITREF  mval            	mu_bin_datefmt;

boolean_t backup_block(sgmnt_addrs *csa, block_id blk, cache_rec_ptr_t backup_cr, sm_uc_ptr_t backup_blk_p);
bool mubfilcpy(backup_reg_list *list);
bool mubgetfil(backup_reg_list *list, char *name, unsigned short len);
bool mubinccpy(backup_reg_list *list);
bool mubgetfil(backup_reg_list *list, char *name, unsigned short len);
boolean_t backup_buffer_flush(gd_region *reg);
void mubclnup(backup_reg_list *curr_ptr, clnup_stage stage);
#ifdef VMS
void mubexpfilnam(backup_reg_list *list);
#elif defined(UNIX)
void mubexpfilnam(char *dirname, unsigned int dirlen, backup_reg_list *list);
#else
#error Unsupported Platform
#endif
gd_region *mubfndreg(unsigned char *regbuf, unsigned short len);
void mup_bak_mag(void);
void mup_bak_pause(void);


