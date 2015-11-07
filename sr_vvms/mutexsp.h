/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUTEXSP_H
#define MUTEXSP_H

/*
 * Initialize a mutex with n que entries. If crash is TRUE, then this is
 * a "crash" reinitialization;  otherwise it's a "clean" initialization.
 */
void		mutex_init(mutex_struct_ptr_t addr, int4 n, bool crash);

/* mutex_lockw - write access to mutex at addr */
#if defined(__vax)
enum 	cdb_sc 	mutex_lockw(mutex_struct_ptr_t addr, int4 crash_count, uint4 *write_lock);
#define MUTEX_LOCKW(addr, seq, flag, spin_parms)	mutex_lockw(addr, seq, flag) /* ignore spin_parms on VAX */

#elif defined(__alpha)
enum 	cdb_sc 	mutex_lockw(mutex_struct_ptr_t addr, int4 crash_count, uint4 *write_lock, mutex_spin_parms_ptr_t spin_parms);
#define MUTEX_LOCKW(addr, seq, flag, spin_parms)	mutex_lockw(addr, seq, flag, spin_parms)

#else
#error UNSUPPORTED_PLATFORM /* neither ALPHA VMS, nor VAX VMS */
#endif

/*
 * mutex_lockwim - write access to mutex at addr; if cannot lock,
 *                 immediately return cdb_sc_nolock
 */
enum	cdb_sc	mutex_lockwim(mutex_struct_ptr_t addr, int4 crash_count, uint4 *write_lock);

/*
 * mutex_lockw_ccp - write access to mutex at addr; if cannot lock,
 *                   queue CCP for "wakeup" and return
 *                   cdb_sc_nolock (do NOT hiber)
 */
enum	cdb_sc	mutex_lockw_ccp(mutex_struct_ptr_t addr, int4 crash_count, uint4 *write_lock, void *super_crit);

/* mutex_unlockw - unlock write access to mutex at addr */
enum	cdb_sc	mutex_unlockw(mutex_struct_ptr_t addr, int4 crash_count, uint4 *write_lock);


#endif /* MUTEXSP_H */
