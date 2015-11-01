/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GDSROOT_H__
#define __GDSROOT_H__

#include <sys/types.h>

#define DIR_ROOT 1
#define CDB_STAGNATE 3
#define CDB_MAX_TRIES (CDB_STAGNATE + 2) /* used in defining arrays, usage requires it must be at least 1 more than CSB_STAGNATE*/
#define MAX_BT_DEPTH 7
#define GLO_NAME_MAXLEN 33	/* 1 for length, 4 for prefix, 15 for dvi, 1 for $, 12 for fid */
#define MAX_NUM_SUBSC_LEN 10	/* one for exponent, nine for the 18 significant digits */
#define EXTEND_WARNING_FACTOR 3

typedef gtm_uint64_t	trans_num;
typedef uint4		trans_num_4byte;
typedef int4    block_id;

enum db_acc_method
{	dba_rms,
	dba_bg,
	dba_mm,
	dba_cm,
	dba_usr,
	n_dba,
	dba_dummy = 0x0ffffff	/* to ensure an int on S390 */
};

#define CREATE_IN_PROGRESS n_dba

typedef struct
{
	char dvi[16];
	unsigned short did[3];
	unsigned short fid[3];
} gds_file_id;

#if defined(VMS)
	typedef struct           /* really just a place holder for gdsfhead.h union */
	{
		unsigned int	inode;		/* ino_t really but VMS defines are not useful here */
		int		device;		/* dev_t really */
		unsigned int 	st_gen;
	} unix_file_id;
	typedef	gds_file_id	gd_id;
#elif defined(UNIX)
	typedef struct gd_id_struct  /* note this is not the same size on all platforms
				       but must be less than or equal to gds_file_id */
	{	ino_t	inode;
		dev_t	device;
#if defined(__hpux) || defined(__linux__) || defined(_UWIN)
		unsigned int st_gen;
#elif defined(_AIX)
		ulong_t st_gen;
#else
		uint_t	st_gen;
#endif
	} unix_file_id;
	typedef unix_file_id	gd_id;
#else
# error Unsupported platform
#endif

#define UNIQUE_ID_SIZE sizeof(gd_id)

typedef union
{
	gd_id 		uid;
	char		file_id[UNIQUE_ID_SIZE];
} unique_file_id;


/* Since it is possible that a block_id/unix_file_id/gd_id may live in shared memory, define a
   shared memory pointer type to it so the pointer will be 64 bits if necessary. */

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef block_id	*block_id_ptr_t;
typedef unix_file_id	*unix_file_id_ptr_t;
typedef	gd_id		*gd_id_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

typedef struct vms_lock_sb_struct
{
	short	cond;
	short	reserved;
	int4	lockid;
	int4	valblk[4];
} vms_lock_sb;

typedef struct
{
	uint4	low, high;
} date_time;
/*
 * Note: For AIX we do not need to check FS_REMOTE == st_flag. Because st_gen is already set to 0
 * or, appropriate non-zero value.
 */
#ifdef UNIX
#if defined(__osf__) || defined(_AIX)
#define gdid_cmp(A, B) 							\
	(((A)->inode != (B)->inode)					\
		? ((A)->inode > (B)->inode ? 1 : -1)			\
		: ((A)->device != (B)->device) 				\
			? ((A)->device > (B)->device ? 1 : -1)		\
			: ((A)->st_gen != (B)->st_gen)			\
				? ((A)->st_gen > (B)->st_gen ? 1 : -1)	\
				: 0)
#else
#define gdid_cmp(A, B) 							\
	(((A)->inode != (B)->inode)					\
		? ((A)->inode > (B)->inode ? 1 : -1)			\
		: ((A)->device != (B)->device) 				\
			? ((A)->device > (B)->device ? 1 : -1)		\
			: 0)
#endif
#define is_gdid_gdid_identical(A, B) (0 == gdid_cmp(A, B) ? TRUE: FALSE)
#endif

/* Prototypes below */
block_id get_dir_root(void);
boolean_t get_full_path(char *orig_fn, unsigned int orig_len, char *full_fn, unsigned int *full_len,
										int max_len, uint4 *status);
void gvinit(void);
#ifdef VMS
void global_name(unsigned char prefix[], gds_file_id *fil,
	unsigned char *buff);
#endif
#endif
