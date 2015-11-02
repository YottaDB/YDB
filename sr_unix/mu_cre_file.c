/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include <errno.h>
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_statvfs.h"

#include "parse_file.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "gtmio.h"
#include "send_msg.h"
#include "is_raw_dev.h"
#include "disk_block_available.h"
#include "mucregini.h"
#include "mu_cre_file.h"
#include "gtmmsg.h"
#include "util.h"

#define BLK_SIZE (((gd_segment*)gv_cur_region->dyn.addr)->blk_size)

#define CLEANUP(XX)	{				\
				if (cc)			\
					free(cc);	\
				if (cs_data)		\
					free(cs_data); 	\
				if (0 < fd)		\
					close(fd);	\
				if (EXIT_ERR == XX)	\
					UNLINK(path);	\
			}

#define SPRINTF_AND_PERROR(MESSAGE)			\
{							\
	save_errno = errno;				\
	SPRINTF(errbuff, MESSAGE, path);		\
	errno = save_errno;				\
	PERROR(errbuff);				\
}


GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;

unsigned char mu_cre_file(void)
{
	char		*cc = NULL, path[MAX_FBUFF + 1], errbuff[512];
	unsigned char	buff[DISK_BLOCK_SIZE];
	int		fd = -1, i, lower, upper, norm_vbn;
        ssize_t         status;
	uint4		raw_dev_size;		/* size of a raw device, in bytes */
	int4		blocks_for_create, blocks_for_extension, save_errno;
	GTM_BAVAIL_TYPE	avail_blocks;
	file_control	fc;
	mstr		file;
	parse_blk	pblk;
	unix_db_info	udi_struct, *udi;
	char		*fgets_res;
	gd_segment	*seg;

	error_def(ERR_NOSPACECRE);
	error_def(ERR_LOWSPACECRE);
	error_def(ERR_MUNOSTRMBKUP);

	assert((-(sizeof(uint4) * 2) & SIZEOF_FILE_HDR_DFLT) == SIZEOF_FILE_HDR_DFLT);
	cs_addrs = &udi_struct.s_addrs;
	cs_data = (sgmnt_data_ptr_t)NULL;	/* for CLEANUP */
	memset(&pblk, 0, sizeof(pblk));
	pblk.fop = (F_SYNTAXO | F_PARNODE);
	pblk.buffer = path;
	pblk.buff_size = MAX_FBUFF;
	file.addr = (char*)gv_cur_region->dyn.addr->fname;
	file.len = gv_cur_region->dyn.addr->fname_len;
	strncpy(path,file.addr,file.len);
	*(path+file.len) = '\0';
	if (is_raw_dev(path))
	{	/* do not use a default extension for raw device files */
		pblk.def1_buf = DEF_NODBEXT;
		pblk.def1_size = sizeof(DEF_NODBEXT) - 1;
	} else
	{
		pblk.def1_buf = DEF_DBEXT;
		pblk.def1_size = sizeof(DEF_DBEXT) - 1;
	}
	if (1 != (parse_file(&file, &pblk) & 1))
	{
		PRINTF("Error translating filename %s.\n", gv_cur_region->dyn.addr->fname);
		return EXIT_ERR;
	}
	path[pblk.b_esl] = 0;
	if (pblk.fnb & F_HAS_NODE)
	{	/* Remote node specification given */
		assert(pblk.b_node);
		PRINTF("Database file for region %s not created; cannot create across network.\n", path);
		return EXIT_WRN;
	}
	udi = &udi_struct;
	udi->raw = is_raw_dev(pblk.l_dir);
	if (udi->raw)
	{
		fd = OPEN(pblk.l_dir,O_EXCL | O_RDWR);
		if (-1 == fd)
		{
			SPRINTF_AND_PERROR("Error opening file %s\n");
			return EXIT_ERR;
		}
		if (-1 != (status = (ssize_t)lseek(fd, 0, SEEK_SET)))
		{
			DOREADRC(fd, buff, sizeof(buff), status);
		} else
			status = errno;
		if (0 != status)
		{
			SPRINTF_AND_PERROR("Error reading header for file %s\n");
			return EXIT_ERR;
		}
		if (!memcmp(buff, GDS_LABEL, STR_LIT_LEN(GDS_LABEL)))
		{
			char rsp[80];
			PRINTF("Database already exists on device %s\n", path);
			PRINTF("Do you wish to re-initialize (all current data will be lost) [y/n] ? ");
			FGETS(rsp, 79, stdin, fgets_res);
			if ('y' != *rsp)
				return EXIT_NRM;
		}
		PRINTF("Determining size of raw device...\n");
		for(i = 1; read(fd, buff, sizeof(buff)) == sizeof(buff);)
		{
			i *= 2;
			lseek(fd, (off_t)i * BUFSIZ, SEEK_SET);
		}

		lower = i / 2;
		upper = i;
		while ((lower + upper) / 2 != lower)
		{
			i = (lower + upper) / 2;
			lseek(fd, (off_t)i * BUFSIZ, SEEK_SET);
 			if (read(fd, buff, sizeof(buff)) == sizeof(buff))
				lower = i;
			else
				upper = i;
		}
		raw_dev_size = i * BUFSIZ;
	} else
	{
		fd = OPEN3(pblk.l_dir, O_CREAT | O_EXCL | O_RDWR, 0600);
		if (-1 == fd)
		{
			SPRINTF_AND_PERROR("Error opening file %s\n");
			return EXIT_ERR;
		}
		if (0 != (save_errno = disk_block_available(fd, &avail_blocks, FALSE)))
		{
			errno = save_errno;
			SPRINTF_AND_PERROR("Error checking available disk space for %s\n");
			CLEANUP(EXIT_ERR);
			return EXIT_ERR;
		}
		seg = gv_cur_region->dyn.addr;

		/* blocks_for_create is in the unit of DISK_BLOCK_SIZE */
		blocks_for_create = (int4)(DIVIDE_ROUND_UP(SIZEOF_FILE_HDR_DFLT, DISK_BLOCK_SIZE) + 1 +
					(seg->blk_size / DISK_BLOCK_SIZE *
					 ((DIVIDE_ROUND_UP(seg->allocation, BLKS_PER_LMAP - 1)) + seg->allocation)));
		if ((uint4)avail_blocks < blocks_for_create)
		{
			gtm_putmsg(VARLSTCNT(6) ERR_NOSPACECRE, 4, LEN_AND_STR(path), blocks_for_create, (uint4)avail_blocks);
			send_msg(VARLSTCNT(6) ERR_NOSPACECRE, 4, LEN_AND_STR(path), blocks_for_create, (uint4)avail_blocks);
			CLEANUP(EXIT_ERR);
			return EXIT_ERR;
		}
		blocks_for_extension = (seg->blk_size / DISK_BLOCK_SIZE *
					((DIVIDE_ROUND_UP(EXTEND_WARNING_FACTOR * seg->ext_blk_count, BLKS_PER_LMAP - 1))
				 	  + EXTEND_WARNING_FACTOR * seg->ext_blk_count));
		if ((uint4)(avail_blocks - blocks_for_create) < blocks_for_extension)
		{
			gtm_putmsg(VARLSTCNT(8) ERR_LOWSPACECRE, 6, LEN_AND_STR(path), EXTEND_WARNING_FACTOR, blocks_for_extension,
					DISK_BLOCK_SIZE, (uint4)(avail_blocks - blocks_for_create));
			send_msg(VARLSTCNT(8) ERR_LOWSPACECRE, 6, LEN_AND_STR(path), EXTEND_WARNING_FACTOR, blocks_for_extension,
					DISK_BLOCK_SIZE, (uint4)(avail_blocks - blocks_for_create));
		}
	}
	gv_cur_region->dyn.addr->file_cntl = &fc;
	fc.file_info = (void*)&udi_struct;
	udi->fd = fd;
	cs_data = (sgmnt_data_ptr_t)malloc(SIZEOF_FILE_HDR_DFLT);
	memset(cs_data, 0, SIZEOF_FILE_HDR_DFLT);
	cs_data->createinprogress = TRUE;
	cs_data->semid = INVALID_SEMID;
	cs_data->shmid = INVALID_SHMID;
	/* We want our datablocks to start on what would be a block boundary within the file which will aid I/O
	   so pad the fileheader if necessary to make this happen.
	*/
	norm_vbn = DIVIDE_ROUND_UP(SIZEOF_FILE_HDR_DFLT, DISK_BLOCK_SIZE) + 1;
	assert(START_VBN_CURRENT >= norm_vbn);
	cs_data->start_vbn = START_VBN_CURRENT;
	cs_data->free_space += (START_VBN_CURRENT - norm_vbn) * DISK_BLOCK_SIZE;
	cs_data->acc_meth = gv_cur_region->dyn.addr->acc_meth;
	if ((dba_mm == cs_data->acc_meth) && (gv_cur_region->jnl_before_image))
	{
		PRINTF("MM access method not compatible with BEFORE image journaling; Database file %s not created.\n", path);
		CLEANUP(EXIT_ERR);
		return EXIT_ERR;
	}
	if (udi->raw)
	{
		/* calculate total blocks, reduce to make room for the
		 * database header (size rounded up to a block), then
		 * make into a multiple of BLKS_PER_LMAP to have a complete bitmap
		 * for each set of blocks.
		 */
		cs_data->trans_hist.total_blks = raw_dev_size - (uint4)ROUND_UP(SIZEOF_FILE_HDR_DFLT, DISK_BLOCK_SIZE);
		cs_data->trans_hist.total_blks /= (uint4)(((gd_segment *)gv_cur_region->dyn.addr)->blk_size);
		if (0 == (cs_data->trans_hist.total_blks - DIVIDE_ROUND_UP(cs_data->trans_hist.total_blks, BLKS_PER_LMAP - 1)
			  % (BLKS_PER_LMAP - 1)))
			cs_data->trans_hist.total_blks -= 1;	/* don't create a bitmap with no data blocks */
		cs_data->extension_size = 0;
		PRINTF("Raw device size is %dK, %d GDS blocks\n",
		raw_dev_size / 1000,
		cs_data->trans_hist.total_blks);
	} else
	{
		cs_data->trans_hist.total_blks = gv_cur_region->dyn.addr->allocation;
		/* There are (bplmap - 1) non-bitmap blocks per bitmap, so add (bplmap - 2) to number of non-bitmap blocks
		 * and divide by (bplmap - 1) to get total number of bitmaps for expanded database. (must round up in this
		 * manner as every non-bitmap block must have an associated bitmap)
		 */
		cs_data->trans_hist.total_blks += DIVIDE_ROUND_UP(cs_data->trans_hist.total_blks, BLKS_PER_LMAP - 1);
		cs_data->extension_size = gv_cur_region->dyn.addr->ext_blk_count;
	}
	mucregini(cs_data->trans_hist.total_blks);
	cs_data->createinprogress = FALSE;
	LSEEKWRITE(udi->fd, 0, cs_data, SIZEOF_FILE_HDR_DFLT, status);
	if (0 != status)
	{
		SPRINTF_AND_PERROR("Error writing out header for file %s\n");
		CLEANUP(EXIT_ERR);
		return EXIT_ERR;
	}
	cc = (char*)malloc(DISK_BLOCK_SIZE);
	memset(cc, 0, DISK_BLOCK_SIZE);
	LSEEKWRITE(udi->fd,
		   (cs_data->start_vbn - 1) * DISK_BLOCK_SIZE + ((off_t)(cs_data->trans_hist.total_blks) * cs_data->blk_size),
		   cc,
		   DISK_BLOCK_SIZE,
		   status);
	if (0 != status)
	{
		SPRINTF_AND_PERROR("Error writing out end of file %s\n");
		CLEANUP(EXIT_ERR);
		return EXIT_ERR;
	}
	if ((!udi->raw) && (-1 == CHMOD(pblk.l_dir, 0666)))
	{
		SPRINTF_AND_PERROR("Error changing file mode on file %s\n");
		CLEANUP(EXIT_WRN);
		return EXIT_WRN;
	}
	if ((32 * 1024 - sizeof(shmpool_blk_hdr)) < cs_data->blk_size)
		gtm_putmsg(VARLSTCNT(5) ERR_MUNOSTRMBKUP, 3, RTS_ERROR_STRING(path), 32 * 1024 - DISK_BLOCK_SIZE);
	util_out_print("Created file !AD", TRUE, RTS_ERROR_STRING(path));
	CLEANUP(EXIT_NRM);
	return EXIT_NRM;
}
