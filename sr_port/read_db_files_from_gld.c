/****************************************************************
 *								*
 *	Copyright 2003, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmmsg.h"
#include "read_db_files_from_gld.h"
#include "gtm_file_stat.h"
#if defined(UNIX)
#include "parse_file.h"
#include "is_raw_dev.h"
#elif defined(VMS)
#define DEFDBEXT                ".dat"
#endif

gld_dbname_list *read_db_files_from_gld(gd_addr *addr)
{
	gd_segment 		*seg;
	uint4			ustatus;
	gd_region		*map_region;
	gd_region		*map_region_top;
	gld_dbname_list		head, *dblist = &head;
	char 			filename[MAX_FN_LEN];
	mstr 			file, def, ret, *retptr;
	error_def(ERR_FILENOTFND);

	head.next = NULL;
	for (map_region = addr->regions, map_region_top = map_region + addr->n_regions; map_region < map_region_top; map_region++)
	{
		assert (map_region < map_region_top);
		seg = (gd_segment *)map_region->dyn.addr;
		FILE_CNTL_INIT_IF_NULL(seg);
		ret.len = SIZEOF(filename);
		ret.addr = filename;
		retptr = &ret;
		file.addr = (char *)seg->fname;
		file.len = seg->fname_len;
#if defined(UNIX)
		file.addr[file.len] = 0;
		if (is_raw_dev(file.addr))
		{
			def.addr = DEF_NODBEXT;
			def.len = SIZEOF(DEF_NODBEXT) - 1;
		}
		else
		{
			def.addr = DEF_DBEXT;	/* UNIX need to pass "*.dat" but reg->dyn.addr->defext has "DAT" */
			def.len = SIZEOF(DEF_DBEXT) - 1;
		}
#elif defined(VMS)
		def.addr = DEFDBEXT;
		def.len = SIZEOF(DEFDBEXT) - 1;
#endif
		if (FILE_PRESENT != gtm_file_stat(&file, &def, retptr, FALSE, &ustatus))
		{
			gtm_putmsg(VARLSTCNT(5) ERR_FILENOTFND, 2, file.len, file.addr, ustatus);
			return NULL;
		}
		assert(0 == filename[retptr->len]);
		seg->fname_len = retptr->len;
		memcpy(seg->fname, filename, retptr->len + 1);
		dblist = dblist->next
		    = (gld_dbname_list *)malloc(SIZEOF(gld_dbname_list));
	        memset(dblist, 0, SIZEOF(gld_dbname_list));
		map_region->stat.addr = (void *)dblist;
		dblist->gd = map_region;
	}
	return head.next;
}
