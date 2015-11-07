/* md5hash.h is adapted to work with FIS GT.M (http://fis-gtm.com) from
 * source code at http://cvsweb.xfree86.org/cvsweb/cvs/lib/md5.h?rev=1.6
 * to which no claim of Copyright was made by the authors. No claim of
 * copyright is made by Fidelity Information Services, Inc. with respect
 * to the changes made to adapt it to GT.M.
 *
 * Summary of changes:
 * 	+ add MD5_BLOCK_LENGTH and MD5_DIGEST_LENGTH macros
 * 	+ add typedef cvs_MD5_CTX
 * 	+ remove PROTO preprocessor code
 */

#ifndef MD5HASH_H_INCLUDED
#define MD5HASH_H_INCLUDED

#define MD5_BLOCK_LENGTH 		64
#define MD5_DIGEST_LENGTH		16

/* Unlike previous versions of this code, uint32 need not be exactly
   32 bits, merely 32 bits or more.  Choosing a data type which is 32
   bits instead of 64 is not important; speed is considerably more
   important.  ANSI guarantees that "unsigned long" will be big enough,
   and always using it seems to have few disadvantages.  */
typedef unsigned long cvs_uint32;

typedef struct cvs_MD5Context {
	cvs_uint32	buf[4];
	cvs_uint32	bits[2];
	unsigned char	in[MD5_BLOCK_LENGTH];
} cvs_MD5_CTX;

void cvs_MD5Init(struct cvs_MD5Context *context);
void cvs_MD5Update(struct cvs_MD5Context *context, unsigned char const *buf, unsigned len);
void cvs_MD5Final(unsigned char digest[MD5_DIGEST_LENGTH], struct cvs_MD5Context *context);
void cvs_MD5Transform(cvs_uint32 buf[4], const unsigned char in[MD5_BLOCK_LENGTH]);

#endif /* MD5HASH_H_INCLUDED */
