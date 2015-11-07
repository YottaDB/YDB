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

#ifndef RTN_SRC_CHKSUM_H_INCLUDED
#define RTN_SRC_CHKSUM_H_INCLUDED

/*
 * MD5 checksum context structures differ depending on whether openssl or libgcrypt is used.
 * If this is not an encryption-enabled platform, we default to a simple four-byte checksum.
 */

#include "md5hash.h"

#ifdef UNIX
# include "mmrhash.h"
# define GTM_USE_128BIT_SRC_CHKSUM
#endif

typedef struct
{
#	ifdef UNIX
	hash128_state_t	hash_state;
	uint4		total_size;
#	else
	cvs_MD5_CTX	md5ctx;
#	endif
	unsigned char	digest[MD5_DIGEST_LENGTH];
#	ifndef GTM_USE_128BIT_SRC_CHKSUM
	uint4		checksum;	/* 32-bit checksum, equals first 4 bytes of digest */
#	endif
} gtm_rtn_src_chksum_ctx;

#define MAX_ROUTINE_CHECKSUM_DIGITS	33

/*
 * The following functions compute a running checksum of a routine, one line at a time.
 */

void rtn_src_chksum_init(gtm_rtn_src_chksum_ctx *ctx);
void rtn_src_chksum_line(gtm_rtn_src_chksum_ctx *ctx, const void *data, uint4 len);
void rtn_src_chksum_digest(gtm_rtn_src_chksum_ctx *ctx);

void set_rtnhdr_checksum(rhdtyp *hdr, gtm_rtn_src_chksum_ctx *ctx);

#ifdef GTM_USE_128BIT_SRC_CHKSUM
unsigned char *get_rtnhdr_checksum(rhdtyp *hdr);
unsigned char *get_ctx_checksum(gtm_rtn_src_chksum_ctx *ctx);
boolean_t rtn_src_chksum_match(unsigned char *digest1, unsigned char *digest2);
#else
uint4 get_rtnhdr_checksum(rhdtyp *hdr);
uint4 get_ctx_checksum(gtm_rtn_src_chksum_ctx *ctx);
boolean_t rtn_src_chksum_match(uint4 checksum1, uint4 checksum2);
#endif

int append_checksum(unsigned char *out, rhdtyp *routine);

void rtn_src_chksum_buffer(gtm_rtn_src_chksum_ctx *ctx, const void *data, uint4 len);

#endif /* RTN_SRC_CHKSUM_H_INCLUDED */
