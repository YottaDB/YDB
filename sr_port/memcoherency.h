/****************************************************************
 *								*
 * Copyright (c) 2003-2016 Fidelity National Information	*
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

/* for Uniprocessor systems, no need for "memory barrier" as memory is always coherent.
 * But almost always we expect to be running on a multi-processor system so we want to avoid the cost
 * of the if check and do the memory barrier ALWAYS.
 */

#ifdef __alpha
  /* ############################### ALPHA architecture ##################################### */

#  include <c_asm.h>

 /* Read Alpha Architecture Reference Manual, edited by Richard L Sites,
  * Chapter "System Architecture and Programming Implications" for memory
  * coherency issues and behavior of "mb" instruction (memory barrier)
  */

 /* NOTES about Alpha (pp. 5-20, section 5.6.5 Implications for Hardware, Chapter 5 System Architecture and Programming
  * Implications, Alpha Architecture Reference Manual, Edited by Richard L Sites
  *
  * MB and IMB force all preceding writes to at least reach their respective coherency points. This does not mean that
  * main-memory writes have been done, just that the order of the eventual writes is committed. MB and IMB also force all
  * queued cache invalidates to be delivered to the local caches before starting any subsequent reads (that may otherwise
  * cache hit on stale data) or writes (that may otherwise write the cache, only to have the write effectively overwriiten
  * by a late-delivered invalidate)
  */
#  define SHM_WRITE_MEMORY_BARRIER	asm("mb")
#  define SHM_READ_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER /* same MB instruction for both read and write barriers */
#  define MM_WRITE_MEMORY_BARRIER
#  define MM_WRITE_MEMORY_BARRIER_IS_NO_OP

#elif defined(POWER) || defined(PWRPC) /* GT.M defines POWER and PWRPC if _AIX is defined, see sr_rs6000/mdefsp.h */
  /* ############################### POWER architecture ################################## */


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
#  pragma mc_func do_sync{"7c0004ac"}
#  pragma reg_killed_by do_sync

  /* lwsync : Creates a memory barrier that provides the same ordering function as the sync instruction, except that a
   * load caused by an instruction following the lwsync may be performed before a store caused by an instruction that
   * precedes the lwsync, and the ordering does not apply to accesses to I/O memory (memory-mapped I/O).
   * lwsync is a new variant of the sync instruction and is interpreted by older processors as a sync. The instruction,
   * as its name implies, has much less performance impact than sync, and is recommended for syncrhonisation of most
   * memory (but not I/O) references.
   */
#  pragma mc_func do_lwsync{"7c2004ac"}
#  pragma reg_killed_by do_lwsync

  /* eieio : Creates a memory barrier that provides the same ordering function as the sync instruction except that
   * ordering applies only to accesses to I/O memory
   */
#  pragma mc_func do_eieio{"7c0006ac"}
#  pragma reg_killed_by do_eieio

  /* isync : Causes the processor to discard any prefetched (and possibly speculatively executed) instructions and
   * refetch the next following instructions. It is used in locking code (e.g. __check_lock()) to ensure that no
   * loads following entry into a critical section can access data (because of aggressive out-of-order and speculative
   * execution in the processor) before the lock is acquired.
   */
#  pragma mc_func do_isync{"4c00012c"}
#  pragma reg_killed_by do_isync

#  define SHM_WRITE_MEMORY_BARRIER                                                                                \
  { /* Ensure that code does not rely on ordering of "loads" following lwsync in programming sequence to occur */ \
    /* after "stores" before lwsync. Use do_sync() if such ordering is required. Replication code (t_end.c,    */ \
    /* tp_end.c) do not rely on store-load order across memory barrier. Note that grab/rel_lock() perform      */ \
    /* "sync" (via call to _clear_lock()), and so, we are guaranteed strict ordering of loads and stores of    */ \
    /* code that reads/writes to journal pool in transaction logic                                             */ \
          do_lwsync();                                                                                            \
  }

#  define SHM_READ_MEMORY_BARRIER         \
  {                                       \
          do_isync();                     \
  }

#  define  MM_WRITE_MEMORY_BARRIER        \
  {                                       \
          SHM_WRITE_MEMORY_BARRIER;       \
          do_eieio();                     \
  }

#elif defined(__hppa)
  /* ############################### PA-RISC architecture ################################## */

  /* For _PA_RISC1_0, _PA_RISC1_1, accesses to the address space (both to memory and I/O) through load, store and
   * semaphore instructions are strongly ordered. This means that accesses appear to software to be done in program order
   * For _PA_RISC2_0, accesses could be "strongly ordered", "ordered", or "weakly ordered" (read PA-RISC 2.0 ARCHITECTURE
   * by Gerry Kane, appendix "Memory Ordering Model").
   *
   * For all PA-RISC architectures, cache flush operations are weakly ordered. Flushes may be delayed or held pending, and
   * a sequence of flush operations may be executed in any order.
   *
   * SYNC : Enforce program order of memory references
   * Any load, store, semaphore, cache flush, or cache purge instructions that follow the SYNC instruction get executed
   * only after all such instructions prior to the SYNC instruction have completed executing. On implementations which
   * execute such instructions out of sequence, this instruction enforces program ordering. In sytems in which all memory
   * references are performed in order, this instruction executes as a null instruction.
   *
   * IMPORTANT: SYNC instruction enforces ordering of only those accesses caused by the instructions executed on the
   * same processor which executes the SYNC instruction.
   *
   * [Vinaya] Research results: Accesses to fields (global) that are defined volatile are ordered (compiler generates
   * LDW (or STW) instruction with the O (ordered) completer, i.e., instructions generated are LDW,O (STW,O). Depending
   * on the requirements, it may be sufficient to define shared fields as volatile to enforce ordering. With replication
   * though, it is important that pending cache flushes are completed so that source server sees the transaction data
   * in its entirety.
   */

#  define SHM_WRITE_MEMORY_BARRIER	(void)_asm("SYNC")

#  define SHM_READ_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER /* same SYNC instruction for both read and write barriers.
  								  * For read, we want all cache purges to be completed before
  								  * we load shared fields */
#elif defined(__ia64)
  /* ############################### IA-64 architecture ################################## */

  /* In the database update logic, we must order memory mapped I/O and shared memory accesses with respect to each other.
   * On Itanium (and on AIX, see eieio above) SHM_WRITE_MEMORY_BARRIER alone is insufficient. We also need an mf.a instruction.
   *
   * Refer to http://www.intel.com/content/dam/www/public/us/en/documents/manuals/
   * 	itanium-architecture-software-developer-rev-2-3-vol-2-manual.pdf - from page 2:616,
   * "If software needs to ensure that all prior memory operations have been accepted by the platform and have been observed
   * by all cache coherent agents, both an mf.a and an mf instruction must be issued. The mf.a must be issued first, and the
   * mf must be issued second."
   */

#  if defined(__hpux)

#	include <machine/sys/kern_inline.h>
#	define SHM_WRITE_MEMORY_BARRIER	_MF()
#	define MM_WRITE_MEMORY_BARRIER			\
  {							\
  	_Asm_mf_a();					\
  	SHM_WRITE_MEMORY_BARRIER;			\
  }

#  elif defined(__linux__) && defined(__INTEL_COMPILER)

#	define SHM_WRITE_MEMORY_BARRIER		__mf()
#	define MM_WRITE_MEMORY_BARRIER			\
	{						\
		__mfa();				\
		SHM_WRITE_MEMORY_BARRIER;		\
	}

#  elif defined(__linux__) /* gcc */

#	define SHM_WRITE_MEMORY_BARRIER		__asm__ __volatile__ ("mf" ::: "memory")
#	define MM_WRITE_MEMORY_BARRIER				\
	{							\
		__asm__ __volatile__ ("mf.a" ::: "memory");	\
		SHM_WRITE_MEMORY_BARRIER;			\
	}

#  endif /* __linux__ */

/* On IA64, cross processor notifications of write barriers are automatic so no read barrier is necessary */
#  define SHM_READ_MEMORY_BARRIER

#elif defined(__armv6l__)

  /* ############################### ARMV6L architecture ################################## */
#  define SHM_WRITE_MEMORY_BARRIER	__asm__ volatile ("mcr p15, 0, r0, c7, c10, 5" : : : "memory")
#  define SHM_READ_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER
#  define MM_WRITE_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER

#elif defined(__armv7l__)

  /* ############################### ARMV7L architecture ################################## */

#  define SHM_WRITE_MEMORY_BARRIER	__asm__ volatile ("dmb" ::: "memory")
#  define SHM_READ_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER
#  define MM_WRITE_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER

#elif defined(__aarch64__)

  /* ############################### ARM architecture ################################## */

#  define SHM_WRITE_MEMORY_BARRIER	__asm__ volatile ("dmb ish" ::: "memory")
#  define SHM_READ_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER
#  define MM_WRITE_MEMORY_BARRIER	SHM_WRITE_MEMORY_BARRIER

#else
  /* ############################### SPARC, I386, S390 architecture ################################## */

  /* Although SPARC architecture allows for out-of-order memory accesses, Solaris forces strong ordering on memory accesses.
   * We do not need memory barrier primitives on Solaris/SPARC.
   */

  /* Memory accesses in Intel x86 and IBM S390 archtectures are strongly ordered, so no fence
   * INSTRUCTION is needed. A COMPILER barrier still is, and these expanded to nothing at all until
   * YDB#1143 showed what that costs.
   *
   * The hardware not reordering two loads does not stop the COMPILER from deleting the second one.
   * Where code reads a shared memory location, does some work, and reads it again to confirm it has
   * not changed, the compiler is entitled to reuse the first value: the location is not volatile and
   * nothing between the two reads tells it otherwise. It then folds the second test away as always
   * true. Every architecture above is accidentally safe from this, because each of their barriers is
   * an "__asm__ volatile" carrying a "memory" clobber, which is a compiler barrier as well as a
   * hardware one. Only this branch, which emits nothing, was not.
   *
   * That is not theoretical. The search index in "gvcst_blk_sidx_locate" re-reads a slot's stamp
   * after copying a sample out of it, exactly so that a slot rewritten under the reader is rejected.
   * On a GCC production build - LTO is enabled for GCC only, so x86_64 - that function is inlined
   * into "gvcst_search_blk", the two reads land in one function with nothing between them, and the
   * re-check is compiled away. A reader then accepts a sample a builder was part way through
   * writing, resumes its search at a record the sample's key does not belong to, and that becomes
   * keys out of order on disk. It reproduces in seconds on x86_64 and has never been seen on
   * aarch64, which is built with clang, gets no LTO, and has a "dmb ish" with a memory clobber.
   *
   * An empty "__asm__ volatile" with a memory clobber emits NO instruction. It costs nothing at run
   * time; it only stops the optimizer moving or removing accesses across this point, which is the
   * whole purpose of the macro.
   *
   * Both macros get the barrier. The read macro is the one a defect was traced to, but the write
   * macro had the identical hole: it too can be shown to permit accesses to move across it, and a
   * macro whose name promises an ordering point should provide one whether or not a failure has
   * been attributed to it yet. The read side is the standing demonstration of what "no failure
   * attributed yet" is worth - it was empty for as long, and cost four database corruptions the
   * first time a build inlined across it.
   */

#  define SHM_WRITE_MEMORY_BARRIER	__asm__ volatile ("" ::: "memory")
#  define SHM_READ_MEMORY_BARRIER	__asm__ volatile ("" ::: "memory")
#  define MM_WRITE_MEMORY_BARRIER
#  define MM_WRITE_MEMORY_BARRIER_IS_NO_OP

#endif

#endif /* MEMCOHERENCY_H_INCLUDED */
