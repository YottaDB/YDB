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

#include "mdef.h"

#include <fcntl.h>
#include <unistd.h>

#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "mlkdef.h"
#include "filestruct.h"
#include "gbldirnam.h"
#include "hashdef.h"

extern int errno;

typedef struct gdr_name_struct
{
	mstr		name;
	struct gdr_name	*link;
	gd_addr		*gd_ptr;
} gdr_name;

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
	int4		*long_ptr;
	uint4		t_offset, size;

	size = sizeof(header_struct) + sizeof(gd_addr) + 3 * sizeof(gd_binding) + 1 * sizeof(gd_region) + 1 * sizeof(gd_segment);
	header = (header_struct *)malloc(ROUND_UP(size, DISK_BLOCK_SIZE));
	memset(header, 0, ROUND_UP(size, DISK_BLOCK_SIZE));
	header->filesize = size;
	size = ROUND_UP(size, DISK_BLOCK_SIZE);
	memcpy(header->label, GDE_LABEL_LITERAL, sizeof(GDE_LABEL_LITERAL));
	addr = (gd_addr *)((char *)header + sizeof(header_struct));
	addr->max_rec_size = 256;
	addr->maps = (gd_binding*)((unsigned )addr + sizeof(gd_addr));
	addr->n_maps = 3;
	addr->regions = (gd_region*)((int4)(addr->maps) + 3 * sizeof(gd_binding));
	addr->n_regions = 1;
	addr->segments = (gd_segment*)((int4)(addr->regions) + sizeof(gd_region));
	addr->n_segments = 1;
	addr->link = 0;
	addr->tab_ptr = 0;
	addr->id = 0;
	addr->local_locks = 0;
	addr->end = (int4)(addr->segments + 1 * sizeof(gd_segment));
	long_ptr = (int4*)((int4)(addr->maps));
	*long_ptr++ = 0x232FFFFF;
	*long_ptr++ = 0xFFFFFFFF;
	*long_ptr++ = (int4)addr->regions;
	*long_ptr++ = 0x24FFFFFF;
	*long_ptr++ = 0xFFFFFFFF;
	*long_ptr++ = (int4)addr->regions;
	*long_ptr++ = 0xFFFFFFFF;
	*long_ptr++ = 0xFFFFFFFF;
	*long_ptr++ = (int4)addr->regions;
	region = (gd_region*)((int4)(addr->regions));
	segment = (gd_segment*)((int4)(addr->segments));
	region->rname_len = 7;
	memcpy(region->rname,"DEFAULT",7);

	for (map = addr->maps, map_top = map + addr->n_maps; map < map_top ; map++)
	{	t_offset = map->reg.offset;
		map->reg.addr = (gd_region *)((char *)addr + t_offset);
	}

	for (region = addr->regions, region_top = region + addr->n_regions; region < region_top ; region++)
	{	t_offset = region->dyn.offset;
		region->dyn.addr = (gd_segment *)((char *)addr + t_offset);
	}

	/* Should be using gd_id_ptr_t below, but ok for now since malloc won't return > 4G
	 * and since addr->id is a 4-byte pointer only until we change the format of the global directory.
	 */
	addr->id = (gd_id *)malloc(sizeof(gd_id));
	memset(addr->id, 0, sizeof(gd_id));

	addr->tab_ptr = (htab_desc *)malloc(sizeof(htab_desc));
	ht_init(addr->tab_ptr,0);

	name = (gdr_name *)malloc(sizeof(gdr_name));
	name->name.addr = (char *)malloc(10);
	strcpy(name->name.addr,"DUMMY.GLD");

	if (gdr_name_head)
		name->link = (struct gdr_name *)gdr_name_head;
	else
		name->link = 0;

	gdr_name_head = name;
	gdr_name_head->gd_ptr = addr;

	return addr;
}
