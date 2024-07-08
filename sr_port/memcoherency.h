/****************************************************************
 *								*
 * Copyright (c) 2003-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 * Copyright (c) 2018 Stephen L Johnson.			*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MEMCOHERENCY_H_INCLUDED
#define MEMCOHERENCY_H_INCLUDED
#include "gtm_atomic.h"

/* for Uniprocessor systems, no need for "memory barrier" as memory is always coherent.
 * But almost always we expect to be running on a multi-processor system so we want to avoid the cost
 * of the if check and do the memory barrier ALWAYS.
 */

/* A barrier here has to do TWO jobs, and on some architectures they need different things.
 *
<<<<<<< HEAD
 * The first is to stop the PROCESSOR reordering memory accesses around this point. That needs a
 * fence INSTRUCTION, and only on architectures that are weakly ordered.
||||||| parent of 07a11a26bb (GT.M V7.1-004)
 * MB and IMB force all preceding writes to at least reach their respective coherency points. This does not mean that
 * main-memory writes have been done, just that the order of the eventual writes is committed. MB and IMB also force all
 * queued cache invalidates to be delivered to the local caches before starting any subsequent reads (that may otherwise
 * cache hit on stale data) or writes (that may otherwise write the cache, only to have the write effectively overwriiten
 * by a late-delivered invalidate)
 */
#define SHM_WRITE_MEMORY_BARRIER	asm("mb")
#define SHM_READ_MEMORY_BARRIER		SHM_WRITE_MEMORY_BARRIER /* same MB instruction for both read and write barriers */
#define MM_WRITE_MEMORY_BARRIER

#elif defined(POWER) || defined(PWRPC)	/* GT.M defines POWER and PWRPC if _AIX is defined, see sr_rs6000/mdefsp.h */

/* Refer to article "POWER4 and shared memory synchronization by R. William Hay and Gary R. Hook" available at
 * http://www-106.ibm.com/developerworks/eserver/articles/power4_mem.html
 */

/* prototypes */
void do_sync(void);
void do_lwsync(void);
void do_eieio(void);
void do_isync(void);

/* The machine codes were fetched from http://www-106.ibm.com/developerworks/eserver/articles/powerpc.html */

/* sync : Creates a memory barrier. On a given processor, any load or store instructions ahead of the sync instruction
 * in the program sequence must complete their accesses to memory first, and then any load or store instructions after
 * sync can begin
 */
#pragma mc_func	do_sync{"7c0004ac"}
#pragma reg_killed_by do_sync

/* lwsync : Creates a memory barrier that provides the same ordering function as the sync instruction, except that a
 * load caused by an instruction following the lwsync may be performed before a store caused by an instruction that
 * precedes the lwsync, and the ordering does not apply to accesses to I/O memory (memory-mapped I/O).
 * lwsync is a new variant of the sync instruction and is interpreted by older processors as a sync. The instruction,
 * as its name implies, has much less performance impact than sync, and is recommended for syncrhonisation of most
 * memory (but not I/O) references.
 */
#pragma mc_func	do_lwsync{"7c2004ac"}
#pragma reg_killed_by do_lwsync

/* eieio : Creates a memory barrier that provides the same ordering function as the sync instruction except that
 * ordering applies only to accesses to I/O memory
 */
#pragma mc_func	do_eieio{"7c0006ac"}
#pragma reg_killed_by do_eieio

/* isync : Causes the processor to discard any prefetched (and possibly speculatively executed) instructions and
 * refetch the next following instructions. It is used in locking code (e.g. __check_lock()) to ensure that no
 * loads following entry into a critical section can access data (because of aggressive out-of-order and speculative
 * execution in the processor) before the lock is acquired.
 */
#pragma mc_func	do_isync{"4c00012c"}
#pragma reg_killed_by do_isync

#define SHM_WRITE_MEMORY_BARRIER										\
{ /* Ensure that code does not rely on ordering of "loads" following lwsync in programming sequence to occur */ \
  /* after "stores" before lwsync. Use do_sync() if such ordering is required. Replication code (t_end.c,    */ \
  /* tp_end.c) do not rely on store-load order across memory barrier. Note that grab/rel_lock() perform      */ \
  /* "sync" (via call to _clear_lock()), and so, we are guaranteed strict ordering of loads and stores of    */ \
  /* code that reads/writes to journal pool in transaction logic					     */ \
	do_lwsync();												\
}

#define SHM_READ_MEMORY_BARRIER		\
{					\
	do_isync();			\
}

#define  MM_WRITE_MEMORY_BARRIER	\
{					\
	SHM_WRITE_MEMORY_BARRIER;	\
	do_eieio();			\
}

#elif defined(__hppa)
/* For _PA_RISC1_0, _PA_RISC1_1, accesses to the address space (both to memory and I/O) through load, store and
 * semaphore instructions are strongly ordered. This means that accesses appear to software to be done in program order
 * For _PA_RISC2_0, accesses could be "strongly ordered", "ordered", or "weakly ordered" (read PA-RISC 2.0 ARCHITECTURE
 * by Gerry Kane, appendix "Memory Ordering Model").
=======
 * MB and IMB force all preceding writes to at least reach their respective coherency points. This does not mean that
 * main-memory writes have been done, just that the order of the eventual writes is committed. MB and IMB also force all
 * queued cache invalidates to be delivered to the local caches before starting any subsequent reads (that may otherwise
 * cache hit on stale data) or writes (that may otherwise write the cache, only to have the write effectively overwriiten
 * by a late-delivered invalidate)
 */
#define SHM_WRITE_MEMORY_BARRIER	asm("mb")
#define SHM_READ_MEMORY_BARRIER		SHM_WRITE_MEMORY_BARRIER /* same MB instruction for both read and write barriers */
#define MM_WRITE_MEMORY_BARRIER

#elif defined(POWER) || defined(PWRPC)	/* GT.M defines POWER and PWRPC if _AIX is defined, see sr_rs6000/mdefsp.h */

#if defined(__xlc__)
#define do_sync()	__sync()
#define do_lwsync()	__lwsync()
#define do_eieio()	__eieio()
#define do_isync()	__isync()
#elif defined(__open_xl__ )
#define do_sync()	__builtin_ppc_sync()
#define do_lwsync()	__builtin_ppc_lwsync()
#define do_eieio()	__builtin_ppc_eieio()
#define do_isync()	__builtin_ppc_isync()
#else
/* Refer to article "POWER4 and shared memory synchronization by R. William Hay and Gary R. Hook" available at
 * http://www-106.ibm.com/developerworks/eserver/articles/power4_mem.html
 */

/* prototypes */
void do_sync(void);
void do_lwsync(void);
void do_eieio(void);
void do_isync(void);

/* The machine codes were fetched from http://www-106.ibm.com/developerworks/eserver/articles/powerpc.html */

/* sync : Creates a memory barrier. On a given processor, any load or store instructions ahead of the sync instruction
 * in the program sequence must complete their accesses to memory first, and then any load or store instructions after
 * sync can begin
 */
#pragma mc_func	do_sync{"7c0004ac"}
#pragma reg_killed_by do_sync

/* lwsync : Creates a memory barrier that provides the same ordering function as the sync instruction, except that a
 * load caused by an instruction following the lwsync may be performed before a store caused by an instruction that
 * precedes the lwsync, and the ordering does not apply to accesses to I/O memory (memory-mapped I/O).
 * lwsync is a new variant of the sync instruction and is interpreted by older processors as a sync. The instruction,
 * as its name implies, has much less performance impact than sync, and is recommended for syncrhonisation of most
 * memory (but not I/O) references.
 */
#pragma mc_func	do_lwsync{"7c2004ac"}
#pragma reg_killed_by do_lwsync

/* eieio : Creates a memory barrier that provides the same ordering function as the sync instruction except that
 * ordering applies only to accesses to I/O memory
 */
#pragma mc_func	do_eieio{"7c0006ac"}
#pragma reg_killed_by do_eieio

/* isync : Causes the processor to discard any prefetched (and possibly speculatively executed) instructions and
 * refetch the next following instructions. It is used in locking code (e.g. __check_lock()) to ensure that no
 * loads following entry into a critical section can access data (because of aggressive out-of-order and speculative
 * execution in the processor) before the lock is acquired.
 */
#pragma mc_func	do_isync{"4c00012c"}
#pragma reg_killed_by do_isync
#endif

#define SHM_WRITE_MEMORY_BARRIER										\
{ /* Ensure that code does not rely on ordering of "loads" following lwsync in programming sequence to occur */ \
  /* after "stores" before lwsync. Use do_sync() if such ordering is required. Replication code (t_end.c,    */ \
  /* tp_end.c) do not rely on store-load order across memory barrier. Note that grab/rel_lock() perform      */ \
  /* "sync" (via call to _clear_lock()), and so, we are guaranteed strict ordering of loads and stores of    */ \
  /* code that reads/writes to journal pool in transaction logic					     */ \
	COMPILER_FENCE(memory_order_release);									\
	do_lwsync();												\
}

#define SHM_READ_MEMORY_BARRIER			\
{						\
	do_isync();				\
	COMPILER_FENCE(memory_order_acquire);	\
}

#define  MM_WRITE_MEMORY_BARRIER	\
{					\
	SHM_WRITE_MEMORY_BARRIER;	\
	do_eieio();			\
}

#elif defined(__hppa)
/* For _PA_RISC1_0, _PA_RISC1_1, accesses to the address space (both to memory and I/O) through load, store and
 * semaphore instructions are strongly ordered. This means that accesses appear to software to be done in program order
 * For _PA_RISC2_0, accesses could be "strongly ordered", "ordered", or "weakly ordered" (read PA-RISC 2.0 ARCHITECTURE
 * by Gerry Kane, appendix "Memory Ordering Model").
>>>>>>> 07a11a26bb (GT.M V7.1-004)
 *
 * The second is to stop the COMPILER doing the same, and it is needed EVERYWHERE, including on
 * architectures so strongly ordered that the first job takes no instruction at all. Code that reads
 * a shared location, does some work, then reads it again to confirm it has not changed is entitled
 * to have both reads performed: the location is not volatile, so without a barrier the compiler may
 * reuse the first value and fold the second test away as always true. That is exactly what happened
 * to the search index re-check in "gvcst_blk_sidx_locate", and it corrupted databases. See the
 * x86_64 section below.
 *
 * A GCC/clang "__asm__ volatile" statement lists, after the last colon, what the statement may
 * destroy - its clobber list. Naming "memory" there tells the compiler the statement may read or
 * write any memory location, so it may not move a memory access across the statement, nor carry
 * across it a value it loaded before it. That is what does the second job. An "__asm__ volatile"
 * naming "memory" is a compiler barrier whether or not the instruction it emits is a real fence,
 * and one with an EMPTY instruction string emits no instruction at all, so it costs nothing at run
 * time. A barrier defined without "memory" - a bare fence instruction, or a vendor compiler
 * intrinsic - does only the first job, and a barrier defined as nothing at all does neither.
 */

#if defined(__aarch64__)
  /* ############################### ARM 64 bit architecture ############################### */

  /* Weakly ordered, so a real fence instruction is required. "dmb ish" is an inner shareable data
   * memory barrier, which covers the cores sharing this database's shared memory. The "memory"
   * clobber makes it a compiler barrier as well.
   */

#  define SHM_WRITE_MEMORY_BARRIER	__asm__ volatile ("dmb ish" ::: "memory")
#  define SHM_READ_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER
#  define MM_WRITE_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER

#elif defined(__armv7l__)
  /* ############################### ARM 32 bit, ARMv7 ##################################### */

  /* Weakly ordered, so a real fence instruction is required. "dmb" here is the full system domain
   * rather than the inner shareable one used on aarch64. The "memory" clobber makes it a compiler
   * barrier as well. Note that "__armv7l__" is not a compiler predefined macro : it is defined by
   * sr_unix/mdefsp.h from "__arm__" and "__ARM_ARCH_7A__", so that header must be included first.
   */

#  define SHM_WRITE_MEMORY_BARRIER	__asm__ volatile ("dmb" ::: "memory")
#  define SHM_READ_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER
#  define MM_WRITE_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER

#elif defined(__armv6l__)
  /* ############################### ARM 32 bit, ARMv6 ##################################### */

  /* ARMv6 has no "dmb" instruction. The equivalent is the CP15 data memory barrier operation,
   * c7/c10 opcode 5, which is what "dmb" replaced in ARMv7. The "memory" clobber makes it a
   * compiler barrier as well. As with ARMv7 above, "__armv6l__" comes from sr_unix/mdefsp.h.
   */

#  define SHM_WRITE_MEMORY_BARRIER	__asm__ volatile ("mcr p15, 0, r0, c7, c10, 5" : : : "memory")
#  define SHM_READ_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER
#  define MM_WRITE_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER

#elif defined(__x86_64__)
  /* ############################### x86 64 bit architecture ############################### */

  /* Memory accesses are strongly ordered, so no fence INSTRUCTION is needed - and that is the trap.
   * These two macros expanded to NOTHING until YDB#1143 showed what that costs. The hardware
   * guarantee is about ORDER : loads and stores become visible in program order. It says nothing
   * about whether a load happens at all. The compiler is free to reuse a value it loaded from a
   * location earlier instead of loading that location a second time, and a load the compiler never
   * emitted is not a load the hardware can order.
   *
   * The search index in "gvcst_blk_sidx_locate" re-reads a slot's stamp after copying a sample out
   * of it, exactly so that a slot rewritten under the reader is rejected. On a GCC production build
   * - LTO is enabled for GCC only - that function is inlined into "gvcst_search_blk", the two reads
   * land in one function with nothing between them, and the re-check is compiled away. A reader
   * then accepts a sample a builder was part way through writing, resumes its search at a record
   * the sample's key does not belong to, and that becomes keys out of order on disk. It reproduced
   * in seconds on x86_64 and was never seen on aarch64, which is built with clang, gets no LTO, and
   * has always had "memory" in the clobber list of its "dmb ish" barrier above.
   *
   * So both macros are an "__asm__ volatile" with an empty instruction string and "memory" in its
   * clobber list : no instruction is emitted, so there is no run time cost, but the compiler may
   * not move a memory access across the point, nor reuse across it a value it loaded before it.
   *
   * MM_WRITE_MEMORY_BARRIER is deliberately left empty, with MM_WRITE_MEMORY_BARRIER_IS_NO_OP
   * defined so that callers compile the surrounding code out entirely rather than emit a barrier
   * that does nothing.
   */

#  define SHM_WRITE_MEMORY_BARRIER	__asm__ volatile ("" ::: "memory")
#  define SHM_READ_MEMORY_BARRIER	__asm__ volatile ("" ::: "memory")
#  define MM_WRITE_MEMORY_BARRIER
#  define MM_WRITE_MEMORY_BARRIER_IS_NO_OP

#else

<<<<<<< HEAD
#  error "Unsupported architecture : memcoherency.h supports x86_64, aarch64, armv7l and armv6l"
||||||| parent of 07a11a26bb (GT.M V7.1-004)
/* Memory accesses in Intel x86 and IBM S390 archtectures are strongly ordered */

#define SHM_WRITE_MEMORY_BARRIER
#define SHM_READ_MEMORY_BARRIER
#define MM_WRITE_MEMORY_BARRIER
=======
/* Memory accesses in Intel x86 and IBM S390 archtectures are strongly ordered */

#define SHM_WRITE_MEMORY_BARRIER COMPILER_FENCE(memory_order_release);
#define SHM_READ_MEMORY_BARRIER COMPILER_FENCE(memory_order_acquire);
#define MM_WRITE_MEMORY_BARRIER COMPILER_FENCE(memory_order_release);
>>>>>>> 07a11a26bb (GT.M V7.1-004)

#endif

#endif /* MEMCOHERENCY_H_INCLUDED */
