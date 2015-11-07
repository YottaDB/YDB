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

#include "mdef.h"

#include <sys/mman.h>

#include "copy.h"
#include "eintr_wrappers.h"
#include "gtm_string.h"
#include "io.h"
#include "gtmio.h"
#include <rtnhdr.h>
#include "rtn_src_chksum.h"

error_def(ERR_FILENOTFND);

/****************************************************************/

/*
 * Initialize MD5 checksum context structure
 */

void rtn_src_chksum_init(gtm_rtn_src_chksum_ctx *ctx)
{
#	ifdef UNIX
	HASH128_STATE_INIT(ctx->hash_state, 0);
	ctx->total_size = 0;
#	else
	cvs_MD5Init(&ctx->md5ctx);
#	endif
}

/*
 * Add a data chunk of length len bytes to our checksum.
 */

void rtn_src_chksum_line(gtm_rtn_src_chksum_ctx *ctx, const void *data, uint4 len)
{
#	ifdef UNIX
	gtmmrhash_128_ingest(&ctx->hash_state, data, len);
	ctx->total_size += len;
#	else
	cvs_MD5Update(&ctx->md5ctx, data, len);
#	endif
}

/*
 * Finished computing checksum. Fill in digest[] array.
 */

void rtn_src_chksum_digest(gtm_rtn_src_chksum_ctx *ctx)
{
#	ifdef UNIX
	gtm_uint16 hash;

	gtmmrhash_128_result(&ctx->hash_state, ctx->total_size, &hash);
	gtmmrhash_128_bytes(&hash, ctx->digest);
#	else
	cvs_MD5Final(ctx->digest, &ctx->md5ctx);
#	endif
#	ifndef GTM_USE_128BIT_SRC_CHKSUM
	GET_ULONG(ctx->checksum, &ctx->digest[0]);
#	endif
}

/*
 * Copy source code checksum from MD5 context into routine header structure.
 */

void set_rtnhdr_checksum(rhdtyp *hdr, gtm_rtn_src_chksum_ctx *ctx)
{
#	ifdef GTM_USE_128BIT_SRC_CHKSUM
	memcpy(&hdr->checksum_128[0], &ctx->digest[0], MD5_DIGEST_LENGTH);
#	else
	hdr->checksum = ctx->checksum;
#	endif
}

/*
 * Compute and digest checksum of a single data chunk, specifically a source file.
 */

void rtn_src_chksum_buffer(gtm_rtn_src_chksum_ctx *ctx, const void *data, uint4 len)
{
	rtn_src_chksum_init(ctx);
	rtn_src_chksum_line(ctx, data, len);
	rtn_src_chksum_digest(ctx);
}

/****************************************************************/

#ifdef GTM_USE_128BIT_SRC_CHKSUM

/*
 * Return start of checksum digest[] array within a routine header structure.
 */

unsigned char *get_rtnhdr_checksum(rhdtyp *hdr)
{
	return &hdr->checksum_128[0];
}

/*
 * Return start of checksum digest[] array within a gtm_rtn_src_chksum_ctx structure.
 */

unsigned char *get_ctx_checksum(gtm_rtn_src_chksum_ctx *ctx)
{
	return &ctx->digest[0];
}

/*
 * Are two checksum digests equal?
 */

boolean_t rtn_src_chksum_match(unsigned char *digest1, unsigned char *digest2)
{
	return (0 == memcmp(digest1, digest2, MD5_DIGEST_LENGTH));
}

#else /* VMS uses 32-bit checksum (... to avoid messing with routine header format). Use first 4 bytes of MD5 checksum */

/*
 * Return 4-byte checksum within a routine header structure.
 */

uint4 get_rtnhdr_checksum(rhdtyp *hdr)
{
	return hdr->checksum;
}

/*
 * Return 4-byte checksum within a gtm_rtn_src_chksum_ctx structure.
 */

uint4 get_ctx_checksum(gtm_rtn_src_chksum_ctx *ctx)
{
	return ctx->checksum;
}

/*
 * Are two 4-byte checksums equal?
 */

boolean_t rtn_src_chksum_match(uint4 checksum1, uint4 checksum2)
{
	return (checksum1 == checksum2);
}

#endif /* GTM_USE_128BIT_SRC_CHKSUM */

/****************************************************************/

/*
 * Extract checksum from a given routine header, and convert to printable hex form.
 * Returns length of valid output copied into 'out' buffer.
 */

int append_checksum(unsigned char *out, rhdtyp *routine)
{
	char		buf[MAX_ROUTINE_CHECKSUM_DIGITS];
	unsigned char	*cptr, *bptr;
	int		i, len, tlen;
	rhdtyp		*hdr;

	hdr = CURRENT_RHEAD_ADR(routine);
#	ifdef GTM_USE_128BIT_SRC_CHKSUM
	cptr = (unsigned char *)get_rtnhdr_checksum(hdr);
	bptr = (unsigned char *)buf;
	len = 0;
	for (i = 0; i < MD5_DIGEST_LENGTH; i++)
	{
		tlen = SPRINTF((char *)bptr, "%02x", cptr[i]);
		bptr += tlen;
		len += tlen;
	}
#	else
	len = SPRINTF(buf, "%04x", (uint4)get_rtnhdr_checksum(hdr));
#	endif
	memcpy(out, (unsigned char *)buf, len);
	return len;
}
