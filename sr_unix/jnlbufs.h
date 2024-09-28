/****************************************************************
 *                                                              *
 * Copyright (c) 2023-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved. *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef JNLBUFS_H
#define JNLBUFS_H
#include "mdef.h"
#include "jnl.h"
#include "gtm_atomic.h"
DEFINE_ATOMIC_OP(gtm_atomic_uint, ATOMIC_FETCH_OR, memory_order_relaxed)
DEFINE_ATOMIC_OP(gtm_atomic_uint, ATOMIC_FETCH_AND, memory_order_relaxed)
DEFINE_ATOMIC_OP(gtm_atomic_uint, ATOMIC_STORE, memory_order_release)
DEFINE_ATOMIC_OP(gtm_atomic_uint, ATOMIC_LOAD, memory_order_relaxed)


#define JPLWRITES_NEEDED 0x1U
#define SRC_JPLWRITES_NEEDED_MASK(IDX) (JPLWRITES_NEEDED << IDX)
#define INST_JPLWRITES_NEEDED_MASK 0xFFFF

/* The following macros are currently implemented without any memory barriers. It is worth noting the assumptions that make
 * this possible. First, there are two kinds of operation that we wish to do on the bitmask. A read-modify-write operation,
 * covered by the SET_ and UNSET_ macros, and pure loads covered by the SRC_ and INST_NEEDS macros. The C standard (in the
 * linux case when we are using C11 atomic) or the compiler (in the AIX/xlc case) guarantees that all operations on the same
 * atomic object will occur in some total order. Insofar as there are only atomic operations on a single object, there is only a
 * need for memory barriers to guarantee some particular ordering; a consistent ordering is already guaranteed. The guarantees we
 * need are that:
 *
 * 1. If a process acquires a sequence number n and another process acquires sequence number n + 1, the second process must not
 *    read a state of write_jnldata that is prior to the one read by the first process. Specifically, the first process's load of
 *    write_jnldata must have a 'happens before' relationship to the second process's load.
 * 2. A source server both does a read-modify-write of the write_jnldata field and reads the jnlpool sequence number to
 *    determine how to begin reading from the pool, if it is reading pool records. Suppose a source server modifies write_jnldata
 *    and that this modification obtains a 'happens before' relationship to any read C0 described in 1).
 *    Read C0 is the head of a chain of reads that observe the changed state of the field, or the result of a later chains.
 *    For all C0, there must be some read Cn, where n is greater than or equal to 0 and Cn does not inter-thread happen before C0,
 *    for which the source server's read of the sequence number happens before Cn, and no C-1 or earlier occurs between the source
 *    server seqno read and Cn. That is: we can't have a process thinking that no journalpool writes are necessary if the source
 *    server has already read in a sequence number (since it will read up to this sequence number from files, but expect the rest
 *    to be in the pool).
 *
 * The first is guaranteed by only reading from the field, on the mumps process side, while holding instance crit. When the
 * mutex is acquired, the process is guaranteed visibility of everything that was visible to the last process to release the mutex
 * (or a later state). Therefore any process getting a later sequence number will see a later state of the variable - this is
 * already guaranteed by the critical section and any additional memory ordering will add overhead. The second is guaranteed by
 * doing the read-modify-write at any time prior to releasing the mutex in which we read the jnlpool sequence number. Once the
 * source server releases crit, it guarantees that any process to grab crit afterwards will see the updated or a later state of the
 * variable. Doing the read-modify-write in crit itself isn't necessary under the current design.
 */

#define SRC_NEEDS_JPLWRITES(JPL, GTMSOURCE_LOCAL) (SRC_JPLWRITES_NEEDED_MASK(GTMSOURCE_LOCAL->gtmsrc_lcl_array_index)           \
                & ATOMIC_LOAD(&JPL->jnlpool_ctl->write_jnldata, memory_order_relaxed))

#define INST_NEEDS_JPLWRITES(JPL) (INST_JPLWRITES_NEEDED_MASK                                                                   \
                & ATOMIC_LOAD(&JPL->jnlpool_ctl->write_jnldata, memory_order_relaxed))

#define SET_SRC_NEEDS_JPLWRITES(JPL, GTMSOURCE_LOCAL) ATOMIC_FETCH_OR(&JPL->jnlpool_ctl->write_jnldata,				\
                SRC_JPLWRITES_NEEDED_MASK(GTMSOURCE_LOCAL->gtmsrc_lcl_array_index), memory_order_relaxed)

#define FIRST_SRC_NEEDING_JPLWRITES(JPL, GTMSOURCE_LOCAL)                                                                       \
        !(INST_JPLWRITES_NEEDED_MASK & SET_SRC_NEEDS_JPLWRITES(JPL, GTMSOURCE_LOCAL))

#define UNSET_SRC_NEEDS_JPLWRITES(JPL, GTMSOURCE_LOCAL) ATOMIC_FETCH_AND(&JPL->jnlpool_ctl->write_jnldata,                    \
                ~SRC_JPLWRITES_NEEDED_MASK(GTMSOURCE_LOCAL->gtmsrc_lcl_array_index), memory_order_relaxed)

#define LAST_SRC_NEEDING_JPLWRITES(JPL, GTMSOURCE_LOCAL)                                                                        \
        (SRC_JPLWRITES_NEEDED_MASK(GTMSOURCE_LOCAL->gtmsrc_lcl_array_index) == UNSET_SRC_NEEDS_JPLWRITES(JPL, GTMSOURCE_LOCAL))

#define INIT_JPLWRITES_NEEDED(JPL) ATOMIC_STORE(&JPL->jnlpool_ctl->write_jnldata, 0U, memory_order_release)

#endif
