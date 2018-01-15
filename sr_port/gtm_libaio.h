/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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
#	define USE_LIBAIO
#elif defined(__CYGWIN__)
#	define USE_NOAIO
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
#	define IF_LIBAIO(x) /* NONE */
#	define IF_LIBAIO_ELSE(x, y) y

#elif !defined(USE_LIBAIO)
#	include <aio.h>
#	define IF_LIBAIO(x) /* NONE */
#	define IF_LIBAIO_ELSE(x, y) y

#else	/* USE_LIBAIO */
#	ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#	endif
#	include <sys/syscall.h>
#	include <errno.h>
#	include <unistd.h>
#	include <string.h>
#	include <fcntl.h>
#	include <linux/aio_abi.h>
#	define	GTM_AIO_NR_EVENTS_DEFAULT 	128	/* Represents the default queue size for in-flight IO's
							 * used by the kernel.
							 */
#	define IO_SETUP_ERRSTR_ARRAYSIZE (MAX_TRANS_NAME_LEN + 11)
	/* We add 12 to the MAX_TRANS_NAME_LEN to make space for the
	 * message, "io_setup(%d)\x00", where "%d" represents a
	 * number that was parsed by trans_numeric(), hence the
	 * MAX_TRANS_NAME_LEN usage.
	 */
#	define IO_SETUP_FMT "io_setup(%d)"
	/* This struct mimics the structure of struct iocb, but adds a few fields
	 * to the end for our own use. See <linux/aio_abi.h>::struct iocb.
	 */
	struct aiocb {
		struct iocb sys_iocb;	/* kernel internal structure */
		/* YottaDB-specific extensions */
		volatile int res;	/* If status is not EINPROGRESS, then denotes the
					 * return value of the IO that just finished. The
					 * return value is analagous to that of the return
					 * value for a synchronous read()/write() syscall.
					 */
		volatile int status;	/* status of the IO in flight */
	};
#	define IF_LIBAIO(x) x
#	define IF_LIBAIO_ELSE(x,y) x
	/* linux/aio_abi.h provides PADDED to define the above struct, but this collides with
	 * our personal #define which means something completely different.
	 */
#	undef PADDED
#endif	/* USE_LIBAIO */

#endif	/* GTM_LIBAIO_H_INCLUDED  */
