/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ipc.h"
#include <sys/mman.h>
#ifndef __MVS__
#include <sys/param.h>
#endif
#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_sem.h"

#include "gt_timer.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "min_max.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "parse_file.h"
#include "jnl.h"
#include "interlock.h"
#include "io.h"
#include "iosp.h"
#include "error.h"
#include "mutex.h"
#include "gtmio.h"
#include "mupipbckup.h"
#include "gtmimagename.h"
#include "mmseg.h"
#include "gtmsecshr.h"
#include "ftok_sems.h"

/* Include prototypes */
#include "mlk_shr_init.h"
#include "eintr_wrappers.h"
#include "is_file_identical.h"

#ifdef MUTEX_MSEM_WAKE
#include "heartbeat_timer.h"
#endif
#include "util.h"
#include "dbfilop.h"
#include "gvcst_init_sysops.h"
#include "is_raw_dev.h"
#include "gv_match.h"
#include "do_semop.h"
#include "gvcmy_open.h"
#include "wcs_sleep.h"
#include "do_shmat.h"
#include "send_msg.h"
#include "gtmmsg.h"

#define ATTACH_TRIES   		10
#define DEFEXT          	"*.dat"
#define MAX_RES_TRIES  	 	620
#define MEGA_BOUND      	(1024 * 1024)
#define EIDRM_SLEEP_INT		500
#define EIDRM_MAX_SLEEPS	20

GBLREF	enum gtmImageTypes	image_type;
GBLREF  uint4                   process_id;
GBLREF  gd_region               *gv_cur_region;
GBLREF  boolean_t               sem_incremented;
GBLREF  boolean_t               mupip_jnl_recover;
GBLREF	ipcs_mesg		db_ipcs;
LITREF  char                    gtm_release_name[];
LITREF  int4                    gtm_release_name_len;

#ifndef MUTEX_MSEM_WAKE
GBLREF	int 	mutex_sock_fd;
#endif

#ifdef DEBUG_DB64
/* if debugging large address stuff, make all memory segments allocate above 1G line */
GBLDEF  sm_uc_ptr_t     next_smseg = (sm_uc_ptr_t)(1L * 1024 * 1024 * 1024);
#endif

static  int             errno_save;
error_def(ERR_DBFILERR);
error_def(ERR_TEXT);

gd_region *dbfilopn (gd_region *reg)
{
        unix_db_info    *udi;
        parse_blk       pblk;
        mstr            file;
        char            *fnptr, fbuff[MAX_FBUFF + 1];
        struct stat     buf;
        gd_region       *prev_reg;
        gd_segment      *seg;
        int             status;
        bool            raw;
	int		stat_res;

        seg = reg->dyn.addr;
        assert(seg->acc_meth == dba_bg  ||  seg->acc_meth == dba_mm);
        if (NULL == seg->file_cntl)
        {
                seg->file_cntl = (file_control *)malloc(sizeof(*seg->file_cntl));
                memset(seg->file_cntl, 0, sizeof(*seg->file_cntl));
        }
        if (NULL == seg->file_cntl->file_info)
        {
                seg->file_cntl->file_info = (void *)malloc(sizeof(unix_db_info));
                memset(seg->file_cntl->file_info, 0, sizeof(unix_db_info));
        }
        file.addr = (char *)seg->fname;
        file.len = seg->fname_len;
        memset(&pblk, 0, sizeof(pblk));
        pblk.buffer = fbuff;
        pblk.buff_size = MAX_FBUFF;
        pblk.fop = (F_SYNTAXO | F_PARNODE);
        memcpy(fbuff,file.addr,file.len);
        *(fbuff + file.len) = '\0';
        if (is_raw_dev(fbuff))
        {
                raw = TRUE;
                pblk.def1_buf = DEF_NODBEXT;
                pblk.def1_size = sizeof(DEF_NODBEXT) - 1;
        } else
        {
                raw = FALSE;
                pblk.def1_buf = DEF_DBEXT;
                pblk.def1_size = sizeof(DEF_DBEXT) - 1;
        }
        status = parse_file(&file, &pblk);
        if (!(status & 1))
        {
                if (GTCM_GNP_SERVER_IMAGE != image_type)
		{
			free(seg->file_cntl->file_info);
                	free(seg->file_cntl);
                	seg->file_cntl = 0;
		}
                rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
        }
        assert(pblk.b_esl < sizeof(seg->fname));
        memcpy(seg->fname, pblk.buffer, pblk.b_esl);
        pblk.buffer[pblk.b_esl] = 0;
        seg->fname[pblk.b_esl] = 0;
        seg->fname_len = pblk.b_esl;
        if (pblk.fnb & F_HAS_NODE)
        {	/* Remote node specification given */
                assert(pblk.b_node && pblk.l_node[pblk.b_node - 1] == ':');
		gvcmy_open(reg, &pblk);
		return (gd_region *)-1;
        }
        fnptr = (char *)seg->fname + pblk.b_node;
        udi = FILE_INFO(reg);
        udi->raw = raw;
        udi->fn = (char *)fnptr;
	OPENFILE(fnptr, O_RDWR, udi->fd);
        udi->ftok_semid = INVALID_SEMID;
        udi->semid = INVALID_SEMID;
	udi->shmid = INVALID_SHMID;
        udi->sem_ctime = 0;
	udi->shm_ctime = 0;
	((sgmnt_addrs *)&FILE_INFO(reg)->s_addrs)->read_write = TRUE;
        if (udi->fd == -1)
        {
                OPENFILE(fnptr, O_RDONLY, udi->fd);
                if (udi->fd == -1)
                {
                        errno_save = errno;
                	if (GTCM_GNP_SERVER_IMAGE != image_type)
			{
				free(seg->file_cntl->file_info);
				free(seg->file_cntl);
				seg->file_cntl = 0;
			}
                        rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno_save);
                }
                reg->read_only = TRUE;
		((sgmnt_addrs *)&FILE_INFO(reg)->s_addrs)->read_write = FALSE;
        }
        STAT_FILE(fnptr, &buf, stat_res);
        set_gdid_from_stat(&udi->fileid, &buf);
        if (prev_reg = gv_match(reg))
        {
                close(udi->fd);
                free(seg->file_cntl->file_info);
                free(seg->file_cntl);
                seg->file_cntl = 0;
                return prev_reg;
        }
        return reg;
}

void dbsecspc(gd_region *reg, sgmnt_data_ptr_t csd)
{
        switch(reg->dyn.addr->acc_meth)
        {
        case dba_mm:
		reg->sec_size = NODE_LOCAL_SPACE + LOCK_SPACE_SIZE(csd) + MMBLK_CONTROL_SIZE(csd)
			+ JNL_SHARE_SIZE(csd) + BACKUP_BUFFER_SIZE;
                break;
        case dba_bg:
		reg->sec_size = NODE_LOCAL_SPACE + (LOCK_BLOCK(csd) * DISK_BLOCK_SIZE) + LOCK_SPACE_SIZE(csd)
			+ CACHE_CONTROL_SIZE(csd) + JNL_SHARE_SIZE(csd) + BACKUP_BUFFER_SIZE;
                break;
        default:
                GTMASSERT;
        }
        return;
}

void db_init(gd_region *reg, sgmnt_data_ptr_t tsd)
{
	static boolean_t	mutex_init_done = FALSE;
        boolean_t       	is_bg, read_only, new_ipc = FALSE;
        char            	machine_name[MAX_MCNAMELEN];
        file_control    	*fc;
	DEBUG_ONLY( gd_region   *r_save;)
	int			gethostname_res, stat_res, mm_prot;
        int4            	status, semval;
        sm_long_t       	status_l;
        sgmnt_addrs     	*csa;
        sgmnt_data_ptr_t        csd;
        struct sembuf   	sop[3];
        struct stat     	stat_buf;
	union semun		semarg;
	struct semid_ds		semstat;
	struct shmid_ds         shmstat;
        uint4           	sopcnt;
        unix_db_info    	*udi;
#ifdef periodic_timer_removed
        void            	periodic_flush_check();
#endif

        error_def(ERR_CLSTCONFLICT);
        error_def(ERR_CRITSEMFAIL);
	error_def(ERR_DBNAMEMISMATCH);
	error_def(ERR_DBIDMISMATCH);
	error_def(ERR_NLMISMATCHCALC);
	error_def(ERR_REQRUNDOWN);
	error_def(ERR_SYSCALL);

        assert(tsd->acc_meth == dba_bg  ||  tsd->acc_meth == dba_mm);
	is_bg = (dba_bg == tsd->acc_meth);
	read_only = reg->read_only;
        udi = FILE_INFO(reg);
        memset(machine_name, 0, sizeof(machine_name));
        if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, gethostname_res))
                rts_error(VARLSTCNT(5) ERR_TEXT, 2, LEN_AND_LIT("Unable to get the hostname"), errno);
	assert(strlen(machine_name) < MAX_MCNAMELEN);
        csa = &udi->s_addrs;
        csa->db_addrs[0] = csa->db_addrs[1] = csa->lock_addrs[0] = NULL;   /* to help in dbinit_ch  and gds_rundown */
        reg->opening = TRUE;
	/*
	 * Create ftok semaphore for this region.
	 * We do not want to make ftok counter semaphore to be 2 for on mupip journal recover process.
	 */
	if (!ftok_sem_get(reg, !mupip_jnl_recover, GTM_ID, FALSE))
		rts_error(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
	/*
	 * At this point we have ftok_semid sempahore based on ftok key.
	 * Any ftok conflicted region will block at this point.
	 * Say, a.dat and b.dat both has same ftok and we have process A to access a.dat and
	 * process B to access b.dat. In this case only one can continue to do db_init()
	 */
        fc = reg->dyn.addr->file_cntl;
        fc->file_type = reg->dyn.addr->acc_meth;
        fc->op = FC_READ;
        fc->op_buff = (sm_uc_ptr_t)tsd;
        fc->op_len = sizeof(*tsd);
        fc->op_pos = 1;
        dbfilop(fc);		/* Read file header */
	udi->shmid = tsd->shmid;
	udi->semid = tsd->semid;
	udi->sem_ctime = tsd->sem_ctime.ctime;
	udi->shm_ctime = tsd->shm_ctime.ctime;
        dbsecspc(reg, tsd); 	/* Find db segment size */
	if (!mupip_jnl_recover)
	{
		if (INVALID_SEMID == udi->semid)
		{
			if (0 != udi->sem_ctime || INVALID_SHMID != udi->shmid || 0 != udi->shm_ctime)
			/* We must have somthing wrong in protocol or, code, if this happens */
				GTMASSERT;
			/*
			 * Create new semaphore using IPC_PRIVATE. System guarantees a unique id.
			 */
			if (-1 == (udi->semid = semget(IPC_PRIVATE, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT)))
			{
				udi->semid = INVALID_SEMID;
				rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, LEN_AND_LIT("Error with database control semget"), errno);
			}
			new_ipc = TRUE;
			tsd->semid = udi->semid;
			semarg.val = GTM_ID;
			/*
			 * Following will set semaphore number 2 (=FTOK_SEM_PER_ID - 1)  value as GTM_ID.
			 * In case we have orphaned semaphore for some reason, mupip rundown will be
			 * able to identify GTM semaphores from the value and can remove.
			 */
			if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, SETVAL, semarg))
				rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl SETVAL"), errno);
			/*
			 * Warning: We must read the sem_ctime using IPC_STAT after SETVAL, which changes it.
			 *	    We must NOT do any more SETVAL after this. Our design is to use
			 *	    sem_ctime as creation time of semaphore.
			 */
			semarg.buf = &semstat;
			if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_STAT, semarg))
				rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl IPC_STAT"), errno);
			tsd->sem_ctime.ctime = udi->sem_ctime = semarg.buf->sem_ctime;
		} else
		{
			if (INVALID_SHMID == udi->shmid)
				/* if mu_rndwn_file gets standalone access of this region and
				 * somehow mupip process crashes, we can have semid != -1 but shmid == -1
				 */
				rts_error(VARLSTCNT(10) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name),
						ERR_TEXT, 2, LEN_AND_LIT("semid is valid but shmid is invalid"));
			semarg.buf = &semstat;
			if (-1 == semctl(udi->semid, 0, IPC_STAT, semarg))
				/* file header has valid semid but semaphore does not exists */
				rts_error(VARLSTCNT(6) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name));
			else if (semarg.buf->sem_ctime != tsd->sem_ctime.ctime)
				rts_error(VARLSTCNT(10) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name),
						ERR_TEXT, 2, LEN_AND_LIT("sem_ctime does not match"));
			if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
				rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, LEN_AND_LIT("Error with database control shmctl"), errno);
			else if (shmstat.shm_ctime != tsd->shm_ctime.ctime)
				rts_error(VARLSTCNT(10) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name),
					ERR_TEXT, 2, LEN_AND_LIT("shm_ctime does not match"));
		}
		/* We already have ftok semaphore of this region, so just plainly do semaphore operation */
		/* This is the database access control semaphore for any region */
		sop[0].sem_num = 0; sop[0].sem_op = 0;	/* Wait for 0 */
		sop[1].sem_num = 0; sop[1].sem_op = 1;	/* Lock */
		sopcnt = 2;
		if (!read_only)
		{
			sop[2].sem_num = 1; sop[2].sem_op  = 1;	 /* increment r/w access counter */
			sopcnt = 3;
		}
		sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = SEM_UNDO | IPC_NOWAIT;
		SEMOP(udi->semid, sop, sopcnt, status);
		if (-1 == status)
		{
			errno_save = errno;
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, errno_save);
		}
	} else /* for mupip_jnl_recover we were already in mu_rndwn_file and got "semid" semaphore  */
	{
		if (INVALID_SEMID == udi->semid || 0 == udi->sem_ctime)
			/* make sure mu_rndwn_file() has reset created semaphore for standalone access */
			GTMASSERT;
		if (INVALID_SHMID != udi->shmid || 0 != udi->shm_ctime)
			/* make sure mu_rndwn_file() has reset shared memory */
			GTMASSERT;
		new_ipc = TRUE;
	}
        sem_incremented = TRUE;
	if (new_ipc)
	{
		/*
		 * Create new shared memory using IPC_PRIVATE. System guarantees a unique id.
		 */
#ifdef __MVS__
		if (-1 == (status_l = udi->shmid = shmget(IPC_PRIVATE, ROUND_UP(reg->sec_size, MEGA_BOUND),
			__IPC_MEGA | IPC_CREAT | IPC_EXCL | RWDALL)))
#else
		if (-1 == (status_l = udi->shmid = shmget(IPC_PRIVATE, reg->sec_size, RWDALL | IPC_CREAT)))
#endif
		{
			udi->shmid = status_l = INVALID_SHMID;
			rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with database shmget"), errno);
		}
		tsd->shmid = udi->shmid;
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
			rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, LEN_AND_LIT("Error with database control shmctl"), errno);
		tsd->shm_ctime.ctime = udi->shm_ctime = shmstat.shm_ctime;
	}
#ifdef DEBUG_DB64
        status_l = (sm_long_t)(csa->db_addrs[0] = (sm_uc_ptr_t)do_shmat(udi->shmid, next_smseg, SHM_RND));
        next_smseg = (sm_uc_ptr_t)ROUND_UP((sm_long_t)(next_smseg + reg->sec_size), SHMAT_ADDR_INCS);
#else
        status_l = (sm_long_t)(csa->db_addrs[0] = (sm_uc_ptr_t)do_shmat(udi->shmid, 0, SHM_RND));
#endif
        if (-1 == status_l)
	{
                rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
                          ERR_TEXT, 2, LEN_AND_LIT("Error attaching to database shared memory"), errno);
	}
	csa->nl = (node_local_ptr_t)csa->db_addrs[0];
	csa->critical = (mutex_struct_ptr_t)(csa->db_addrs[0] + NODE_LOCAL_SIZE);
        assert(((int)csa->critical & 0xf) == 0); 			/* critical should be 16-byte aligned */
#ifdef CACHELINE_SIZE
	assert(0 == ((int)csa->critical & (CACHELINE_SIZE - 1)));
#endif
	/* Note: Here we check jnl_sate from database file and
	 * its value cannot change without standalone access.
	 * In other words it is not necessary to read shared memory for the test (jnl_state != jnl_notallowed) */
	csa->jnl_state = tsd->jnl_state;
	csa->jnl_before_image = tsd->jnl_before_image;
	csa->repl_state = tsd->repl_state;
	/* The jnl_buff buffer should be initialized irrespective of read/write process */
	if (JNL_ALLOWED(csa))
	{
		csa->jnl = (jnl_private_control *)malloc(sizeof(*csa->jnl));
		memset(csa->jnl, 0, sizeof(*csa->jnl));
		csa->jnl->channel = NOJNL;
		csa->jnl->region = reg;
		csa->jnl->jnl_buff = (jnl_buffer_ptr_t)(csa->db_addrs[0] + NODE_LOCAL_SPACE + JNL_NAME_EXP_SIZE);
	}
	else
		csa->jnl = NULL;
	csa->backup_buffer = (backup_buff_ptr_t)(csa->db_addrs[0] + NODE_LOCAL_SPACE + JNL_SHARE_SIZE(tsd));
	csa->lock_addrs[0] = (sm_uc_ptr_t)csa->backup_buffer + BACKUP_BUFFER_SIZE + 1;
	csa->lock_addrs[1] = csa->lock_addrs[0] + LOCK_SPACE_SIZE(tsd) - 1;
	csa->total_blks = tsd->trans_hist.total_blks;   		/* For test to see if file has extended */
	if (new_ipc)
		memset(csa->nl, 0, sizeof(*csa->nl));			/* We allocated shared storage -- we have to init it */
        if (is_bg)
		csd = csa->hdr = (sgmnt_data_ptr_t)(csa->lock_addrs[1] + 1 + CACHE_CONTROL_SIZE(tsd));
	else
        {
		csa->acc_meth.mm.mmblk_state = (mmblk_que_heads_ptr_t)(csa->lock_addrs[1] + 1);
		FSTAT_FILE(udi->fd, &stat_buf, stat_res);
		if (-1 == stat_res)
                        rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
		mm_prot = read_only ? PROT_READ : (PROT_READ | PROT_WRITE);
#ifdef DEBUG_DB64
                if (-1 == (sm_long_t)(csa->db_addrs[0] = (sm_uc_ptr_t)mmap((caddr_t)get_mmseg((size_t)stat_buf.st_size),
                                                                           (size_t)stat_buf.st_size,
                                                                           mm_prot,
									   GTM_MM_FLAGS, udi->fd, (off_t)0)))
                        rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
		put_mmseg((caddr_t)(csa->db_addrs[0]), (size_t)stat_buf.st_size);
#else
                if (-1 == (sm_long_t)(csa->db_addrs[0] = (sm_uc_ptr_t)mmap((caddr_t)NULL,
                                                                           (size_t)stat_buf.st_size,
                                                                           mm_prot,
                                                                           GTM_MM_FLAGS, udi->fd, (off_t)0)))
                        rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
#endif
                csa->db_addrs[1] = csa->db_addrs[0] + stat_buf.st_size - 1;
		csd = csa->hdr = (sgmnt_data_ptr_t)csa->db_addrs[0];
        }
        if (!csa->nl->glob_sec_init)
        {
                if (is_bg)
			*csd = *tsd;
                if (csd->machine_name[0])                  /* crash occured */
                {
                        if (0 != memcmp(csd->machine_name, machine_name, MAX_MCNAMELEN))  /* crashed on some other node */
				rts_error(VARLSTCNT(6) ERR_CLSTCONFLICT, 4, DB_LEN_STR(reg), LEN_AND_STR(csd->machine_name));
			else
				rts_error(VARLSTCNT(6) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csd->machine_name));
                }
		if (is_bg)
		{
                        bt_malloc(csa);
                        csa->nl->cache_off = -CACHE_CONTROL_SIZE(tsd);
                        db_csh_ini(csa);
                }
                db_csh_ref(csa);
		strcpy(csa->nl->machine_name, machine_name);					/* machine name */
		assert(MAX_REL_NAME > gtm_release_name_len);
                memcpy(csa->nl->now_running, gtm_release_name, gtm_release_name_len + 1);	/* GT.M release name */
		memcpy(csa->nl->label, GDS_LABEL, GDS_LABEL_SZ - 1);				/* GDS label */
		memcpy(csa->nl->fname, reg->dyn.addr->fname, reg->dyn.addr->fname_len);		/* database filename */
		csa->nl->creation_date_time = csd->creation.date_time;
                csa->nl->highest_lbm_blk_changed = -1;
                csa->nl->wcs_timers = -1;
                csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
                csa->nl->unique_id.uid = FILE_INFO(reg)->fileid;            /* save what file we initialized this storage for */
		/* save pointers in csa to access shared memory */
		csa->nl->critical = (sm_off_t)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl);
		if (JNL_ALLOWED(csa))
			csa->nl->jnl_buff = (sm_off_t)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl);
		csa->nl->backup_buffer = (sm_off_t)((sm_uc_ptr_t)csa->backup_buffer - (sm_uc_ptr_t)csa->nl);
		csa->nl->hdr = (sm_off_t)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)csa->nl);
		csa->nl->lock_addrs = (sm_off_t)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl);
		if (!read_only || is_bg)
		{
			csd->trans_hist.early_tn = csd->trans_hist.curr_tn;
			csd->max_update_array_size = csd->max_non_bm_update_array_size
				= ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(csd), UPDATE_ARRAY_ALIGN_SIZE);
			csd->max_update_array_size += ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE, UPDATE_ARRAY_ALIGN_SIZE);
			/* add current db_csh counters into the cumulative counters and reset the current counters */
#define TAB_DB_CSH_ACCT_REC(COUNTER, DUMMY1, DUMMY2)					\
				csd->COUNTER.cumul_count += csd->COUNTER.curr_count;	\
				csd->COUNTER.curr_count = 0;
#include "tab_db_csh_acct_rec.h"
#undef TAB_DB_CSH_ACCT_REC
		}
                if (!read_only)
                {
                        if (is_bg)
                        {
                                assert(memcmp(csd, GDS_LABEL, GDS_LABEL_SZ - 1) == 0);
                                LSEEKWRITE(udi->fd, (off_t)0, (sm_uc_ptr_t)csd, sizeof(sgmnt_data), errno_save);
                                if (0 != errno_save)
                                {
                                        rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
                                                  ERR_TEXT, 2, LEN_AND_LIT("Error with database write"), errno_save);
                                }
                        }
                }
                reg->dyn.addr->ext_blk_count = csd->extension_size;
                mlk_shr_init(csa->lock_addrs[0], csd->lock_space_size, csa, (FALSE == read_only));
		DEBUG_ONLY(
			r_save = gv_cur_region; /* set gv_cur_region for LOCK_HIST */
			gv_cur_region = reg;
		)
                gtm_mutex_init(reg, NUM_CRIT_ENTRY, FALSE);
		DEBUG_ONLY(
                	gv_cur_region = r_save; /* restore gv_cur_region */
		)
		if (read_only)
			csa->nl->remove_shm = TRUE;	/* gds_rundown can remove shmem if first process has read-only access */
		db_auto_upgrade(reg);
		csa->nl->glob_sec_init = TRUE;
		STAT_FILE((char *)csa->nl->fname, &stat_buf, stat_res);
		if (-1 == stat_res)
                	rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
		set_gdid_from_stat(&csa->nl->unique_id.uid, &stat_buf);
        } else
        {
                if (strcmp(csa->nl->machine_name, machine_name))       /* machine names do not match */
                {
			if (csa->nl->machine_name[0])
				rts_error(VARLSTCNT(6) ERR_CLSTCONFLICT, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name));
			else
				rts_error(VARLSTCNT(6) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name));
                }
		/* Since nl is memset to 0 initially and then fname is copied over from gv_cur_region and since "fname" is
		 * guaranteed to not exceed MAX_FN_LEN, we should have a terminating '\0' atleast at csa->nl->fname[MAX_FN_LEN]
		 */
		assert(csa->nl->fname[MAX_FN_LEN] == '\0');	/* Note: the first '\0' in csa->nl->fname can be much earlier */
                if (FALSE == is_gdid_gdid_identical(&FILE_INFO(reg)->fileid, &csa->nl->unique_id.uid) ||
						csa->nl->creation_date_time != csd->creation.date_time)
		{
			send_msg(VARLSTCNT(10) ERR_DBIDMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid,
				ERR_TEXT, 2, LEN_AND_LIT("Fileid of database file doesn't match fileid in shared memory"));
			rts_error(VARLSTCNT(10) ERR_DBIDMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid,
				ERR_TEXT, 2, LEN_AND_LIT("Fileid of database file doesn't match fileid in shared memory"));
		}
		/* Check whether cs_addrs->nl->fname exists. If not, then it is a serious condition. Error out. */
		STAT_FILE((char *)csa->nl->fname, &stat_buf, stat_res);
		if (-1 == stat_res)
		{
                        send_msg(VARLSTCNT(7) ERR_DBNAMEMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid, errno);
                        rts_error(VARLSTCNT(7) ERR_DBNAMEMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid, errno);
		}
		/* Check whether csa->nl->fname and csa->nl->unique_id.uid are in sync.
		 * If not its a serious condition. Error out. */
		if (FALSE == is_gdid_stat_identical(&csa->nl->unique_id.uid, &stat_buf))
		{
			send_msg(VARLSTCNT(10) ERR_DBIDMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid,
				ERR_TEXT, 2, LEN_AND_LIT("Database filename and fileid in shared memory are not in sync"));
			rts_error(VARLSTCNT(10) ERR_DBIDMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid,
				ERR_TEXT, 2, LEN_AND_LIT("Database filename and fileid in shared memory are not in sync"));
		}
		/* verify pointers from our calculation vs. the copy in shared memory */
		if (csa->nl->critical != (sm_off_t)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("critical"),
				  	(uint4)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->critical);
		if ((JNL_ALLOWED(csa)) &&
		    (csa->nl->jnl_buff != (sm_off_t)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl)))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("journal buffer"),
				  	(uint4)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->jnl_buff);
		if (csa->nl->backup_buffer != (sm_off_t)((sm_uc_ptr_t)csa->backup_buffer - (sm_uc_ptr_t)csa->nl))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("backup buffer"),
				  (uint4)((sm_uc_ptr_t)csa->backup_buffer - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->backup_buffer);
		if ((is_bg) && (csa->nl->hdr != (sm_off_t)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)csa->nl)))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("file header"),
				  	(uint4)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->hdr);
		if (csa->nl->lock_addrs != (sm_off_t)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("lock address"),
				  (uint4)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->lock_addrs);
        }
        if (-1 == (semval = semctl(udi->semid, 1, GETVAL))) /* semval = number of process attached */
        {
		errno_save = errno;
		gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semctl()"), CALLFROM, errno_save);
	}
	if (!read_only && 1 == semval)
	{	/* For read-write process flush file header to write machine_name,
		 * semaphore, shared memory id and semaphore creation time to disk.
		 */
		csa->nl->remove_shm = FALSE;
		strcpy(csd->machine_name, machine_name);
		if (JNL_ALLOWED(csa))
			memset(&csd->jnl_file, 0, sizeof(csd->jnl_file));
		LSEEKWRITE(udi->fd, (off_t)0, (sm_uc_ptr_t)csd, sizeof(sgmnt_data), errno_save);
		if (0 != errno_save)
		{
			rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with database header flush"), errno_save);
		}
	} else if (read_only && new_ipc)
	{	/* For read-only process if shared memory and semaphore created for first time,
		 * semaphore and shared memory id, and semaphore creation time are written to disk.
		 */
		db_ipcs.semid = tsd->semid;	/* use tsd instead of csd in order for MM to work too */
		db_ipcs.shmid = tsd->shmid;
		db_ipcs.sem_ctime = tsd->sem_ctime.ctime;
		db_ipcs.shm_ctime = tsd->shm_ctime.ctime;
		db_ipcs.fn_len = reg->dyn.addr->fname_len;
		memcpy(db_ipcs.fn, reg->dyn.addr->fname, reg->dyn.addr->fname_len);
		db_ipcs.fn[reg->dyn.addr->fname_len] = 0;
		if (0 != send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0))
			rts_error(VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("gtmsecshr failed to update database file header"));

	}
	++csa->nl->ref_cnt;         /* This value is changed under control of the init/rundown semaphore only */
	if (!mupip_jnl_recover)
	{
 		/* Release control lockout now that it is init'd */
		if (0 != (errno_save = do_semop(udi->semid, 0, -1, SEM_UNDO)))
		{
			errno_save = errno;
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, errno_save);
		}
		sem_incremented = FALSE;
	}
	/* Release ftok semaphore lock so that any other ftok conflicted database can continue now */
	if (!ftok_sem_release(reg, FALSE, FALSE))
		rts_error(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
	/* Do the per process initialization of mutex stuff */
	if (!mutex_init_done) /* If not already done */
	{
		mutex_seed_init();
#ifdef MUTEX_MSEM_WAKE
		start_timer((TID)&heartbeat_timer, HEARTBEAT_INTERVAL, heartbeat_timer, 0, NULL);
#else
		mutex_sock_init();
#endif
		mutex_init_done = TRUE;
	}
        return;
}
