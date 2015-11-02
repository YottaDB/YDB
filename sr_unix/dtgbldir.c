/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "main_pragma.h"

#undef malloc
#undef free
#undef GTM_FD_TRACE	/* this is a standalone executable so do not convert OPEN3 to gtm_open3 for FD tracing */

#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include <errno.h>

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

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

#define asc2i(str,len) atoi((const char*)str)
#ifdef BIGENDIAN
#	define	FIRST_ONE	0x2329FFFF
#	define  SECOND_ONE	0x25FFFFFF
#else
#	define	FIRST_ONE	0xFFFF2923
#	define  SECOND_ONE	0xFFFFFF25
#endif

#ifdef GTM64
#define SAVE_ADDR_REGION                   \
{ \
	int4 *tmp = (int *)&(addr->regions); \
	*int4_ptr++ = *(tmp + lsb_index); \
	int4_ptr++;       \
}
#else /* GTM64 */
#define SAVE_ADDR_REGION                   \
	*int4_ptr++ = (int4)addr->regions;
#endif /* GTM64 */


int main(int argc, char **argv)
{
	int		fd;
	header_struct	*header;
	gd_addr		*addr;
	gd_region	*region;
	gd_region	*region_top;
	gd_segment	*segment;
	int4		*int4_ptr;
	uint4		t_offset, size;
	int		i, alloc, extend, block, global, key, lock, record;
	ZOS_ONLY(int	realfiletag;)

	argv++;
	alloc = 100;
	extend = 100;
	block = 1024;
	global = 128;
	lock = 20;
	record = 256;
	key = 64;
	for (i = argc; i > 1; i--)
	{
		if (argv[0][0] != '-' || argv[0][1] == 'h')
		{
			PRINTF("syntax is dtgbldir [-abegklr] with each qualifier followed by a number\n");
			PRINTF("and separated by spaces. i.e. dtgbldir -a100 -b2048 -l30\n");
			PRINTF("-a is alloc [100]     -b is block size [1024] -e is ext [100]\n");
			PRINTF("-g is glo buff [128]  -k is keysize [64]      -l is lock [20]\n");
			PRINTF("-r is rec size [256]  -h prints this message\n");
			PRINTF("The global directory is created as ./mumps.gld\n");
			return 0;
		}
		switch (argv[0][1])
		{
			case 'a':	if (-1 == (alloc = asc2i((uchar_ptr_t)&argv[0][2],STRLEN(argv[0]) - 2)))
					{
						PRINTF("Invalid value for -a: %s\n", &argv[0][2]);
						return 0;
					}
					break;
			case 'b':	if (-1 == (block = asc2i((uchar_ptr_t)&argv[0][2],STRLEN(argv[0]) - 2)))
					{
						PRINTF("Invalid value for -b: %s\n", &argv[0][2]);
						return 0;
					}
					break;
			case 'e':	if (-1 == (extend = asc2i((uchar_ptr_t)&argv[0][2],STRLEN(argv[0]) - 2)))
					{
						PRINTF("Invalid value for -e: %s\n", &argv[0][2]);
						return 0;
					}
					break;
			case 'g':	if (-1 == (global = asc2i((uchar_ptr_t)&argv[0][2], STRLEN(argv[0]) - 2)))
					{
						PRINTF("Invalid value for -g: %s\n", &argv[0][2]);
						return 0;
					}
					break;
			case 'k':	if (-1 == (key = asc2i((uchar_ptr_t)&argv[0][2], STRLEN(argv[0]) - 2)))
					{
						PRINTF("Invalid value for -k: %s\n", &argv[0][2]);
						return 0;
					}
					break;
			case 'l':	if (-1 == (lock = asc2i((uchar_ptr_t)&argv[0][2], STRLEN(argv[0]) - 2)))
					{
						PRINTF("Invalid value for -l: %s\n", &argv[0][2]);
						return 0;
					}
					break;
			case 'r':	if (-1 == (record = asc2i((uchar_ptr_t)&argv[0][2], STRLEN(argv[0]) - 2)))
					{
						PRINTF("Invalid value for -r: %s\n", &argv[0][2]);
						return 0;
					}
					break;
			default:	PRINTF("unrecognized qualifier %s\n",argv[0]);
					return 0;
		}
		argv++;
	}
	if (argc == 1)
	{
		PRINTF("syntax is dtgbldir [-abegklr] with each qualifier followed by a number\n");
		PRINTF("and separated by spaces. i.e. dtgbldir -a100 -b2048 -l30\n");
		PRINTF("-a is alloc [100]     -b is block size [1024] -e is ext [100]\n");
		PRINTF("-g is glo buff [128]  -k is keysize [64]      -l is lock [20]\n");
		PRINTF("-r is rec size [256]  -h prints this message\n");
		PRINTF("The global directory is created as ./mumps.gld\n");
		PRINTF("\n\nCreating default global directory\n");
	}
	if (record > (block / 2) - 4)
	{
		PRINTF("Record size %d is too large, block size %d will only support record size of %d\n",
			record, block, (block / 2) - 4);
		return 0;
	}
	if (key >= record)
	{
		PRINTF("Key size %d is too large, record size %d will only support key size of %d\n",
			key, record, record - 1);
		return 0;
	}

	if (lock > 100 || lock < 4)
	{
		PRINTF("Lock size %d is invalid, must be between 4 and 100\n", lock);
		return 0;
	}

	if (block % DISK_BLOCK_SIZE)
	{
		PRINTF("Block size %d is invalid, must be a multiple of %d\n", block, DISK_BLOCK_SIZE);
		return 0;
	}

	if (global < 64 || global > 4096)
	{
		PRINTF("Global value %d is invalid, must be between 64 and 4096\n", global);
		return 0;
	}
	if (FD_INVALID == (fd = OPEN3("./mumps.gld", O_CREAT | O_EXCL | O_RDWR, 0600)))
	{
		PERROR("Error opening file");
		return 0;
	}
#if defined(__MVS__)
	if (-1 == gtm_zos_set_tag(fd, TAG_BINARY, TAG_NOTTEXT, TAG_FORCE, &realfiletag))
		PERROR("Error tagging file with TAG_BINARY");
#endif
	size = SIZEOF(header_struct) + SIZEOF(gd_addr) + 3 * SIZEOF(gd_binding) + 1 * SIZEOF(gd_region) + 1 * SIZEOF(gd_segment);
	header = (header_struct *)malloc(ROUND_UP(size, DISK_BLOCK_SIZE));
	memset(header, 0, ROUND_UP(size, DISK_BLOCK_SIZE));
	header->filesize = size;
	size = ROUND_UP(size, DISK_BLOCK_SIZE);
	MEMCPY_LIT(header->label, GDE_LABEL_LITERAL);
	addr = (gd_addr *)((char *)header + SIZEOF(header_struct));
	addr->max_rec_size = 256;
	addr->maps = (gd_binding*)(SIZEOF(gd_addr));
	addr->n_maps = 3;
	addr->regions = (gd_region *)((INTPTR_T)(addr->maps) + 3 * SIZEOF(gd_binding));
	addr->n_regions = 1;
	addr->segments = (gd_segment *)((INTPTR_T)(addr->regions) + SIZEOF(gd_region));
	addr->n_segments = 1;
	addr->link = 0;
	addr->tab_ptr = 0;
	addr->id = 0;
	addr->local_locks = 0;
	addr->end = (INTPTR_T)((char *)addr->segments + 1 * SIZEOF(gd_segment));
	int4_ptr = (int4*)((char *)addr + (INTPTR_T)(addr->maps));
	*int4_ptr++ = FIRST_ONE;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
	*int4_ptr++ = 0xFFFFFFFF;
        SAVE_ADDR_REGION
	*int4_ptr++ = SECOND_ONE;
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
	region = (gd_region*)((char *)addr + (INTPTR_T)(addr->regions));
	segment = (gd_segment*)((char *)addr + (INTPTR_T)(addr->segments));
	region->rname_len = 7;
	memcpy(region->rname,"DEFAULT",7);

	for (region_top = region + addr->n_regions; region < region_top ; region++)
	{	t_offset = region->dyn.offset;
		region->dyn.addr = (gd_segment *)(INTPTR_T)t_offset;
	}

	region = (gd_region*)((char *)addr + (INTPTR_T)(addr->regions));
	region->dyn.offset = (int4)(INTPTR_T)addr->segments;
	region->max_rec_size = record;
	region->max_key_size = key;
	region->open = region->lock_write = region->null_subs = region->jnl_state = 0;
	region->jnl_alq = 0x64;
	region->jnl_deq = 0x64;
	region->jnl_buffer_size = 0x80;
	region->jnl_before_image = 1;
	region->jnl_file_len = region->node = region->cmx_regnum = 0;
	region->sec_size = 0;

	segment->sname_len = 7;
	memcpy(segment->sname,"DEFAULT",7);
	memcpy(segment->fname,"mumps.dat",9);
	segment->fname_len = 9;
	segment->blk_size = block;
	segment->allocation = alloc;
	segment->ext_blk_count = extend;
	segment->cm_blk = 0;
	segment->lock_space = lock;
	memcpy(segment->defext,".DAT",4);
	segment->global_buffers = global;
	segment->buckets = 0;
	segment->windows = 0;
	segment->acc_meth = dba_bg;
	segment->defer_time = 0;
	segment->file_cntl = 0;

	if (lseek(fd, (off_t)0, SEEK_SET))
	{
		PERROR("Error seeking on file");
		return 0;
	}
	if (write(fd, header, size) != size)
	{
		PERROR("Error writing to file");
		return 0;
	}
	return 1;
}

