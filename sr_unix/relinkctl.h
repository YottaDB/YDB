/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef RELINKCTL_H_INCLUDED
#define RELINKCTL_H_INCLUDED


/* For autorelink debugging, uncomment the line below */
/* #define AUTORELINK_DEBUG */
#if defined(AUTORELINK_DEBUG)
# define DBGARLNK(x) DBGFPF(x)
# define DBGARLNK_ONLY(x) x
# include "gtm_stdio.h"
# include "gtmio.h"
#else
# define DBGARLNK(x)
# define DBGARLNK_ONLY(x)
#endif

#define	MIN_RELINKCTL_MAPSZ	1024 /* bytes */
#define REC_NOT_FOUND 		(relinkrec_loc_t)(-1)

/* Macros for dealing with relative and absolute addresses and converting between them */
#define RCTLABS2REL(ABSADR, BASEADR) ((relinkrec_loc_t)((UINTPTR_T)(ABSADR) - (UINTPTR_T)(BASEADR)))
#define RCTLREL2ABS(RELADR, BASEADR) ((relinkrec_ptr_abs_t)((UINTPTR_T)(BASEADR) + (UINTPTR_T)(RELADR)))

/* Macro to read the cycle# of a given relinkctl record */
#define RELINKCTL_CYCLE_READ(LINKCTL, RECOFFSET) (((relinkrec_ptr_abs_t)(RCTLREL2ABS((RECOFFSET), (LINKCTL)->rec_base)))->cycle)
/* Macro to bump the cycle# of a given relinkctl record */
#define RELINKCTL_CYCLE_INCR(LINKCTL, RECOFFSET) (((relinkrec_ptr_abs_t)(RCTLREL2ABS((RECOFFSET), (LINKCTL)->rec_base)))->cycle++)

/*
 * Possible TODO as it relates to ensuring consistent code state:
 * 	Along with cycle and rtnname, put MD5 checksum in record. When a process ZLINKs, ensure that checksum of object matches?
 * 	something like:
 * 	1. open() object file, note rhdr->MD5
 * 	2. Note rec->MD5
 * 	3. Issue error if different (unless it's different because of concurrent ZRUPDATE, in which case repeat from step 1.)
 */

/* Shared structure - relink record in a relinkctl (mmap'd) file */
typedef struct relinkrec_struct
{
	mident_fixed	rtnname_fixed;
	uint4		cycle;
	/* TODO: Add hash or tree or sorted list lookup for speed */
} relinkrec_t;

/* Shared structure - relinkctl file header */
typedef struct relinkctl_data_struct
{
	/* TODO: Add zro_entry_name, so that we can "rundown" or delete desired structs */
	uint4		n_records;
	int4		nattached;	/* Number of processes currently attached. this is approximate, because if a process
					 * is kill 9'd, nattached is not decrememented.
					 * If nattached is 0 upon exiting, we can remove the file.
					 * TODO: Provide fancier cleanup scheme for kill 9. two options:
					 * 	1. SYSV semaphore (as with the db). increment in open_relinkctl
					 * 	2. When we want to cleanup (say mupip routine -rundown), execute 'fuser'
					 */
	global_latch_t	attach_latch;
	relinkrec_t	base[1];
} relinkctl_data;

typedef off_t			relinkrec_ptr_rel_t;	/* Relative offset of relinkrec */
typedef relinkrec_t		*relinkrec_ptr_abs_t;	/* Absolute pointer to relinkrec */
/* For now at least, use relative pointers as primary way of referring to shared relink records */
typedef relinkrec_ptr_rel_t	relinkrec_loc_t;

/* Process private structure - describes a relinkctl file. Process private so can be linked into a list in $ZROUTINES order */
typedef struct open_relinkctl_struct
{
	struct open_relinkctl_struct	*next;			/* List of open ctl structures, sorted by zro_entry_name */
	mstr				zro_entry_name;		/* Text resident in stringpool */
	int				fd;
	uint4				n_records;		/* Private copy */
	relinkctl_data			*hdr;			/* Base of mapped file */
	relinkrec_t			*rec_base;
	boolean_t			locked;			/* TRUE if this process owns exclusive lock */
} open_relinkctl_sgm;

/*
 * Prototypes
 */
open_relinkctl_sgm *relinkctl_attach(mstr *obj_container_name);
relinkrec_loc_t relinkctl_find_record(open_relinkctl_sgm *linkctl, mstr *rtnname);
relinkrec_loc_t relinkctl_insert_record(open_relinkctl_sgm *linkctl, mstr *rtnname);
void relinkctl_open(open_relinkctl_sgm *linkctl);
void relinkctl_ensure_fullmap(open_relinkctl_sgm *linkctl);
void relinkctl_lock_exclu(open_relinkctl_sgm *linkctl);
void relinkctl_unlock_exclu(open_relinkctl_sgm *linkctl);
void relinkctl_rundown(boolean_t decr_attached);
#endif /* RELINKCTL_H_INCLUDED */
