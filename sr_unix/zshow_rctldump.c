/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef AUTORELINK_SUPPORTED

#include "gtm_string.h"

#include "relinkctl.h"
#include "util.h"
#include "zshow.h"

#define	DUMP_ONE_LINE(OUTPUT, BUFF, NBYTES)						\
{											\
	mstr	line;									\
											\
	if (NBYTES >= SIZEOF(BUFF))							\
		NBYTES = SIZEOF(BUFF); /* Output from SNPRINTF was truncated. */	\
	if (NULL != OUTPUT)								\
	{	/* Caller is ZSHOW "R". Use zshow_output. */				\
		line.len = NBYTES;							\
		line.addr = &BUFF[0];							\
		OUTPUT->flush = TRUE;							\
		zshow_output(OUTPUT, &line);						\
	} else										\
	{	/* Caller is MUPIP RCTLDUMP. Use util_out_print. */			\
		util_out_print("!AD", FLUSH, NBYTES, BUFF);				\
	}										\
}

/* Implements ZSHOW "A". But also called by MUPIP RCTLDUMP ("output" parameter is NULL in this case) to do the same thing. */
void	zshow_rctldump(zshow_out *output)
{
	open_relinkctl_sgm	*linkctl;
	relinkshm_hdr_t		*shm_hdr;
	relinkrec_t		*linkrec;
	relinkctl_data		*hdr;
	rtnobjshm_hdr_t		*rtnobj_shm_hdr;
	int			i, j, recnum, n_records, nbytes;
	char			buff[OUT_BUFF_SIZE];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (linkctl = TREF(open_relinkctl_list); NULL != linkctl; linkctl = linkctl->next)
	{
		hdr = linkctl->hdr;
		nbytes = SNPRINTF(buff, SIZEOF(buff), "Object Directory         : %.*s",
			linkctl->zro_entry_name.len, linkctl->zro_entry_name.addr);
		DUMP_ONE_LINE(output, buff, nbytes);
		nbytes = SNPRINTF(buff, SIZEOF(buff), "Relinkctl filename       : %s", linkctl->relinkctl_path);
		DUMP_ONE_LINE(output, buff, nbytes);
		n_records = hdr->n_records;
		nbytes = SNPRINTF(buff, SIZEOF(buff), "# of routines / max      : %d / %d", n_records,
				  hdr->relinkctl_max_rtn_entries);
		DUMP_ONE_LINE(output, buff, nbytes);
		nbytes = SNPRINTF(buff, SIZEOF(buff), "# of attached processes  : %d", hdr->nattached);
		DUMP_ONE_LINE(output, buff, nbytes);
		nbytes = SNPRINTF(buff, SIZEOF(buff), "Relinkctl shared memory  : shmid: %d  shmlen: 0x%llx",
			hdr->relinkctl_shmid, hdr->relinkctl_shmlen);
		DUMP_ONE_LINE(output, buff, nbytes);
		shm_hdr = GET_RELINK_SHM_HDR(linkctl);
		for (i = 0, j = 1; i < NUM_RTNOBJ_SHM_INDEX; i++)
		{
			rtnobj_shm_hdr = &shm_hdr->rtnobj_shmhdr[i];
			if (INVALID_SHMID != rtnobj_shm_hdr->rtnobj_shmid)
			{
				nbytes = SNPRINTF(buff, SIZEOF(buff), "Rtnobj shared memory #%2.d : shmid: %u  shmlen: 0x%llx"
					"  shmused: 0x%llx  shmfree: 0x%llx  objlen: 0x%llx",
					j, rtnobj_shm_hdr->rtnobj_shmid, rtnobj_shm_hdr->shm_len,
					rtnobj_shm_hdr->used_len,
					rtnobj_shm_hdr->shm_len - rtnobj_shm_hdr->used_len,
					rtnobj_shm_hdr->real_len);
				DUMP_ONE_LINE(output, buff, nbytes);
				j++;
			}
		}
		for (linkrec = linkctl->rec_base, recnum = 1; recnum <= n_records; linkrec++, recnum++)
		{
			nbytes = SNPRINTF(buff, SIZEOF(buff), "    rec#%d: rtnname: %.*s  cycle: %d  objhash: 0x%llx"
				"  numvers: %d  objlen: 0x%llx  shmlen: 0x%llx",
				recnum, mid_len(&linkrec->rtnname_fixed), &linkrec->rtnname_fixed.c,
				linkrec->cycle, linkrec->objhash, linkrec->numvers, linkrec->objLen,
				linkrec->usedLen);
			DUMP_ONE_LINE(output, buff, nbytes);
		}
	}
}

#endif
