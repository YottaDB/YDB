/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"
#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_string.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif
#include "util.h"

#ifdef UNICODE_SUPPORTED
GBLREF	boolean_t	gtm_utf8_mode;
#endif

#define MAX_LINE	(32767+256)	/* see cli.h */

/* Get one line of input from file stream fp and
 * trim any line terminators.
 *
 * buffersize is the size in bytes of the buffer
 * where the line is returned and includes the
 * terminating null.
 * buffersize must be less than or equal to MAX_LINE
 * for Unicode
 *
 * If the return is NULL, there was an error
 * otherwise it is the start of the line.
 * If remove_leading_spaces, the return value
 * will point at the first non space, according
 * to isspace_asscii.  Note that this may not be
 * the beginning of buffer.
 */

char *util_input(char *buffer, int buffersize, FILE *fp, boolean_t remove_leading_spaces)
{
	size_t		in_len;
	char		*retptr;
#ifdef UNICODE_SUPPORTED
	int		mbc_len, u16_off, non_space_off;
	int32_t		mbc_dest_len;
	boolean_t	found_non_space = FALSE;
	UFILE		*u_fp;
	UChar		*uc_fgets_ret, ufgets_Ubuffer[MAX_LINE];
	UChar32		uc32_cp;
	UErrorCode	errorcode;
#endif

#ifdef UNICODE_SUPPORTED
	if (gtm_utf8_mode)
	{
		assert(MAX_LINE >= buffersize);
		ufgets_Ubuffer[0] = 0;
		u_fp = u_finit(fp, NULL, UTF8_NAME);
		if (NULL != u_fp)
		{
			do
			{	/* no u_ferror */
				uc_fgets_ret = u_fgets(ufgets_Ubuffer, (int32_t)(SIZEOF(ufgets_Ubuffer) / SIZEOF(UChar)) - 1, u_fp);
			} while (NULL == uc_fgets_ret && !u_feof(u_fp) && ferror(fp) && EINTR == errno);
			if (NULL == uc_fgets_ret)
			{
				if (!u_feof(u_fp))
					util_out_print("Error reading from STDIN", TRUE);
				u_fclose(u_fp);
				return NULL;
			}
			in_len = u_strlen(ufgets_Ubuffer);
			in_len = trim_U16_line_term(ufgets_Ubuffer, (int4)in_len);
			for (non_space_off = u16_off = mbc_len = 0; u16_off < in_len && mbc_len < (buffersize - 1); )
			{
				U16_NEXT(ufgets_Ubuffer, u16_off, in_len, uc32_cp);	/* updates u16_off */
				if (remove_leading_spaces && !found_non_space)
					if (U_ISSPACE(uc32_cp))
						continue;
					else
					{
						found_non_space = TRUE;
						non_space_off = u16_off;
						U16_BACK_1(ufgets_Ubuffer, 0, non_space_off); /* get non space offset */
					}
				mbc_len += U8_LENGTH(uc32_cp);
			}
			if (mbc_len >= (buffersize - 1))
			{
				U16_BACK_1(ufgets_Ubuffer, 0, u16_off);
				in_len = u16_off >= 0 ? u16_off + 1 : 0;	/* offset to length */
			}
			errorcode = U_ZERO_ERROR;
			u_strToUTF8(buffer, buffersize, &mbc_dest_len, &ufgets_Ubuffer[non_space_off],
			(int4)in_len - non_space_off + 1, &errorcode);	/* include null */
			if (U_FAILURE(errorcode))
				if (U_BUFFER_OVERFLOW_ERROR == errorcode)
				{       /* truncate so null terminated */
					buffer[buffersize - 1] = 0;
					retptr = buffer;
				} else
					retptr = NULL;
			else
				retptr = buffer;
			u_fclose(u_fp);
		} else
			retptr = NULL;
	} else
	{
#endif
		buffer[0] = '\0';
		do
		{
			FGETS(buffer, buffersize, fp, retptr);
		} while (NULL == retptr && !feof(fp) && ferror(fp) && EINTR == errno);
		if (NULL != retptr)
		{
			if (remove_leading_spaces)
				while (*retptr && ISSPACE_ASCII(*retptr))
					retptr++;
			in_len = strlen(buffer);
			if ('\n' == buffer[in_len - 1])
				buffer[in_len - 1] = '\0';
		} else
		{
			if (!feof(fp))
				util_out_print("Error reading from STDIN", TRUE);
		}
#ifdef UNICODE_SUPPORTED
	}
#endif
	return retptr;
}
