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

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "mlkdef.h"
#include "filestruct.h"
#include "gbldirnam.h"
#include "hashtab_mname.h"
#include "hashtab.h"

#ifdef GTM64
#define SAVE_ADDR_REGION			\
{						\
	int4 *tmp = (int *)&(addr->regions);	\
	*int4_ptr++ = *tmp;			\
	int4_ptr++;				\
}
#else /* GTM64 */
#define SAVE_ADDR_REGION			\
	*int4_ptr++ = (int4)addr->regions;
#endif /* GTM64 */

static gdr_name	*gdr_name_head;

gd_addr *create_dummy_gbldir(void)
{
	header_struct	*header;
	gd_addr		*addr;
	gdr_name	*name;
	gd_binding	*map, *map_top;
	gd_region	*region;
	gd_region	*region_top;
	gd_segment	*segment;
	int4		*int4_ptr;
	uint4		t_offset, size;

	size = SIZEOF(header_struct) + SIZEOF(gd_addr) + 3 * SIZEOF(gd_binding) + 1 * SIZEOF(gd_region) + 1 * SIZEOF(gd_segment);
	header = (header_struct *)malloc(ROUND_UP(size, DISK_BLOCK_SIZE));
	memset(header, 0, ROUND_UP(size, DISK_BLOCK_SIZE));
	header->filesize = size;
	size = ROUND_UP(size, DISK_BLOCK_SIZE);
	memcpy(header->label, GDE_LABEL_LITERAL, SIZEOF(GDE_LABEL_LITERAL));
	addr = (gd_addr *)((char *)header + SIZEOF(header_struct));
	addr->max_rec_size = 256;
	addr->maps = (gd_binding*)((UINTPTR_T)addr + SIZEOF(gd_addr));
	addr->n_maps = 3;
	addr->regions = (gd_region*)((INTPTR_T)(addr->maps) + 3 * SIZEOF(gd_binding));
	addr->n_regions = 1;
	addr->segments = (gd_segment*)((INTPTR_T)(addr->regions) + SIZEOF(gd_region));
	addr->n_segments = 1;
	addr->link = 0;
	addr->tab_ptr = 0;
	addr->id = 0;
	addr->local_locks = 0;
	addr->end = (UINTPTR_T)(addr->segments + 1 * SIZEOF(gd_segment));
	int4_ptr = (int4*)(addr->maps);
	*int4_ptr++ = 0x232FFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
        SAVE_ADDR_REGION
	*int4_ptr++ = 0x24FFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
        SAVE_ADDR_REGION
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
        SAVE_ADDR_REGION
	region = (gd_region*)(addr->regions);
	segment = (gd_segment*)(addr->segments);
	region->rname_len = 7;
	memcpy(region->rname,"DEFAULT",7);

	for (map = addr->maps, map_top = map + addr->n_maps; map < map_top ; map++)
	{
		t_offset = map->reg.offset;
		map->reg.addr = (gd_region *)((char *)addr + t_offset);
	}
	for (region = addr->regions, region_top = region + addr->n_regions; region < region_top ; region++)
	{
		t_offset = region->dyn.offset;
		region->dyn.addr = (gd_segment *)((char *)addr + t_offset);
	}
	/* Should be using gd_id_ptr_t below, but ok for now since malloc won't return > 4G
	 * and since addr->id is a 4-byte pointer only until we change the format of the global directory.
	 */
	addr->id = (gd_id *)malloc(SIZEOF(gd_id));
	memset(addr->id, 0, SIZEOF(gd_id));

	addr->tab_ptr = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
	init_hashtab_mname((hash_table_mname *)addr->tab_ptr, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE );

	name = (gdr_name *)malloc(SIZEOF(gdr_name));
	MALLOC_CPY_LIT(name->name.addr, "DUMMY.GLD");
	if (gdr_name_head)
		name->link = (gdr_name *)gdr_name_head;
	else
		name->link = 0;
	gdr_name_head = name;
	gdr_name_head->gd_ptr = addr;
	return addr;
}
