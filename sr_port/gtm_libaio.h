/****************************************************************
 *								*
 * Copyright (c) 2016-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_LIBAIO_H_INCLUDED
#define GTM_LIBAIO_H_INCLUDED

/* Enable LIBAIO only on 64-bit linux. We disable LIBAIO on 32-bit linux
 * for non-technical reasons.
 */
#if defined(__linux__) && defined(__x86_64__)
#define USE_LIBAIO
#elif defined(__CYGWIN__)
#define USE_NOAIO
#endif

#ifdef USE_NOAIO
      /* AIO NOT SUPPORTED */
/* minimal just to satisfy mur_init.c and mur_read_file.h.
 * More would be needed if MUR_USE_AIO were defined */
struct aiocb {
	int		aio_fildes;
	volatile void	*aio_buf;
	size_t		aio_nbytes;
	off_t		aio_offset;
	size_t		aio_bytesread;
	int		aio_errno;
};
#define IF_LIBAIO(x) /* NONE */
#define IF_LIBAIO_ELSE(x, y) y
#elif !defined(USE_LIBAIO)
#include <aio.h>
#define IF_LIBAIO(x) /* NONE */
#define IF_LIBAIO_ELSE(x, y) y
#else	/* USE_LIBAIO */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/syscall.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/aio_abi.h>

#define	GTM_AIO_NR_EVENTS_DEFAULT 	128	/* Represents the default queue size for in-flight IO's
						 * used by the kernel.
						 */

#define IO_SETUP_ERRSTR_ARRAYSIZE (MAX_TRANS_NAME_LEN + 11)
	/* We add 12 to the MAX_TRANS_NAME_LEN to make space for the
	 * message, "io_setup(%d)\x00", where "%d" represents a
	 * number that was parsed by trans_numeric(), hence the
	 * MAX_TRANS_NAME_LEN usage.
	 */
#define IO_SETUP_FMT "io_setup(%d)"

/* This struct mimics the structure of struct iocb, but adds a few fields
 * to the end for our own use. See <linux/aio_abi.h>::struct iocb.
 * Note: that Linux v4.14 typedef'ed aio_rw_flags like so.
 *	typedef int __bitwise __kernel_rwf_t;
 */
struct aiocb {
	/* kernel-internel structure, mirrors struct iocb */
	__u64	aio_data;
#if defined(__BYTE_ORDER) ? __BYTE_ORDER == __LITTLE_ENDIAN : defined(__LITTLE_ENDIAN)
	__u32	aio_key;	/* the kernel sets aio_key to the req # */
	int __bitwise aio_rw_flags;	/* RWF_* flags */
#elif defined(__BYTE_ORDER) ? __BYTE_ORDER == __BIG_ENDIAN : defined(__BIG_ENDIAN)
	int __bitwise aio_rw_flags;	/* RWF_* flags */
	__u32	aio_key;	/* the kernel sets aio_key to the req # */
#else
#error edit for your odd byteorder.
#endif
	__u16	aio_lio_opcode;
	__s16	aio_reqprio;
	__u32	aio_fildes;
	__u64	aio_buf;
	__u64	aio_nbytes;
	__s64	aio_offset;
	__u64	aio_reserved2;
	__u32	aio_flags;
	__u32	aio_resfd;

	/* personal implementation-specific definitions */
	volatile int res;	/* If status is not EINPROGRESS, then denotes the
				 * return value of the IO that just finished. The
				 * return value is analagous to that of the return
				 * value for a synchronous read()/write() syscall.
				 */
	volatile int status;	/* status of the IO in flight */
};

#define IF_LIBAIO(x) x
#define IF_LIBAIO_ELSE(x,y) x

#ifdef PADDED
/* linux/aio_abi.h provides PADDED until Linux v4.14 to define the above struct, but
 * this collides with our personal #define which means something completely different.
 */
#undef PADDED
#endif
#endif	/* USE_LIBAIO */

#endif	/* GTM_LIBAIO_H_INCLUDED  */
