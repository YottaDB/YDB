/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ZYENCODE_ZYDECODE_DEF_DEFINED

#define ARG1_LCL 1
#define ARG1_GBL 2
#define ARG2_LCL 4
#define ARG2_GBL 8
#define IND1 0
#define IND2 1
#define ARG1_IS_LCL(arg) (arg & ARG1_LCL)
#define ARG1_IS_GBL(arg) (arg & ARG1_GBL)
#define ARG2_IS_LCL(arg) (arg & ARG2_LCL)
#define ARG2_IS_GBL(arg) (arg & ARG2_GBL)

/* Construct stacked error objects out of nested errors returned from SAPI functions, ydb_decode_s and ydb_encode_s(),
 * called by the runtime commands, ZYENCODE or ZYDECODE.
 *	STATUS is the status error code for the error being parsed and re-thrown
 *	ERRCODE is which *INCOMPL error begins the stacked error object
 */
#define ENCODE_DECODE_NESTED_RTS_ERROR(STATUS, ERRCODE)										\
{																\
	unsigned long long	len1, len2;											\
	char			*zs1, *zs2, *zs3, *zs4, zstatus[YDB_MAX_ERRORMSG + 160], error_str[YDB_MAX_ERRORMSG];		\
																\
	/* YDB_MAX_ERRORMSG is not quite large enough for YDB_ERR_GVSUBOFLOW */							\
	ydb_zstatus(zstatus, YDB_MAX_ERRORMSG + 160);										\
																\
	/* STATUS codes other than YDB_TP_RESTART are negative, we need positive for rts_error_csa(), hence -STATUS below */	\
	switch(STATUS)														\
		{														\
		case YDB_ERR_GVSUBOFLOW:											\
			/* NOTE: This might need to change if the ERR_GVSUBOFLOW format changes */				\
			zs1 = strstr(zstatus, "YDB-I-GVIS, ");									\
			if (NULL == zs1)											\
				break;												\
			zs1 += 12;												\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERRCODE, 0, -STATUS, 0, ERR_GVIS, 2, LEN_AND_STR(zs1));	\
			break;													\
		case YDB_ERR_LVUNDEF:												\
			/* NOTE: This might need to change if the ERR_LVUNDEF format changes */					\
			zs1 = strstr(zstatus, "variable: ");									\
			if (NULL == zs1)											\
				break;												\
			zs1 += 10;												\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERRCODE, 0, -STATUS, 2, LEN_AND_STR(zs1));			\
			break;													\
		case YDB_ERR_GVUNDEF:												\
			/* NOTE: This might need to change if the ERR_GVUNDEF format changes */					\
			zs1 = strstr(zstatus, "undefined: ");									\
			if (NULL == zs1)											\
				break;												\
			zs1 += 11;												\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERRCODE, 0, -STATUS, 2, LEN_AND_STR(zs1));			\
			break;													\
		case YDB_ERR_INVSTRLEN:												\
		case YDB_ERR_JANSSONINVSTRLEN:											\
			/* NOTE: This might need to change if the ERR_*INVSTRLEN format changes */				\
			/* we need to get at the dynamic data represented by !UL to stack the errors correctly */		\
			zs1 = strstr(zstatus, "length ");									\
			if (NULL == zs1)											\
				break;												\
			zs1 += 7;												\
			errno = 0;												\
			len1 = STRTOU64L(zs1, NULL, MAX_DIGITS_IN_INT);								\
			if (errno)												\
			{													\
				SNPRINTF(error_str, YDB_MAX_ERRORMSG, "STRTOU64L(): %s", STRERROR(errno));			\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERRCODE, 0, ERR_TEXT, 2, LEN_AND_STR(error_str));	\
			}													\
			zs2 = strstr(zstatus, "max ");										\
			if (NULL == zs2)											\
				break;												\
			zs2 += 4;												\
			errno = 0;												\
			len2 = STRTOU64L(zs2, NULL, MAX_DIGITS_IN_INT);								\
			if (errno)												\
			{													\
				SNPRINTF(error_str, YDB_MAX_ERRORMSG, "STRTOU64L(): %s", STRERROR(errno));			\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERRCODE, 0, ERR_TEXT, 2, LEN_AND_STR(error_str));	\
			}													\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERRCODE, 0, -STATUS, 2, len1, len2);				\
			break;													\
		case YDB_ERR_INVVARNAME:											\
			/* NOTE: This might need to change if the ERR_INVVARNAME format changes */				\
			/* we need to get at the dynamic data represented by !AD to stack the errors correctly */		\
			zs1 = strstr(zstatus, "name ");										\
			if (NULL == zs1)											\
				break;												\
			zs1 += 5;												\
			zs2 = strstr(zstatus, " supplied");									\
			if (NULL == zs2)											\
				break;												\
			*zs2 = '\0';	/* turn substring pointers in to C strings */						\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERRCODE, 0, -STATUS, 2, LEN_AND_STR(zs1));			\
			break;													\
		case YDB_ERR_PARAMINVALID:											\
			/* NOTE: This might need to change if the ERR_PARAMINVALID format changes */				\
			/* we need to get at the dynamic data represented by !AD to stack the errors correctly */		\
			zs1 = strstr(zstatus, "PARAMINVALID, ");								\
			if (NULL == zs1)											\
				break;												\
			zs1 += 14;	/* move to end of substring */								\
			zs2 = strstr(zstatus, " parameter specified in ");							\
			if (NULL == zs2)											\
				break;												\
			zs3 = zs2 + 24;	/* move to end of substring */								\
			zs4 = strstr(zstatus, " call");										\
			if (NULL == zs4)											\
				break;												\
			*zs2 = *zs4 = '\0';	/* turn substring pointers in to C strings */					\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERRCODE, 0, -STATUS, 4, LEN_AND_STR(zs1), LEN_AND_STR(zs3));	\
			break;													\
		case YDB_ERR_JANSSONINVALIDJSON:										\
		case YDB_ERR_JANSSONDLERROR:											\
		case YDB_ERR_JANSSONENCODEERROR:										\
			/* NOTE: This might need to change if the ERR_JANSSON* format changes */				\
			/* we need to get at the dynamic data represented by !AD to stack the errors correctly */		\
			zs1 = strstr(zstatus, ": ");										\
			if (NULL == zs1)											\
				break;												\
			zs1 += 2;	/* move to end of substring */								\
			zs2 = strstr(zstatus, " - in ");									\
			if (NULL == zs2)											\
				break;												\
			zs3 = zs2 + 6;	/* move to end of substring */								\
			zs4 = strstr(zstatus, " call");										\
			if (NULL == zs4)											\
				break;												\
			*zs2 = *zs4 = '\0';	/* turn substring pointers in to C strings */					\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERRCODE, 0, -STATUS, 4, LEN_AND_STR(zs1), LEN_AND_STR(zs3));	\
			break;													\
		default:													\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERRCODE, 0, -STATUS);						\
			break;													\
	}															\
	/* Catch strstr library call errors from above - no errno */								\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERRCODE, 0, ERR_TEXT, 2, LEN_AND_LIT("See $ZSTATUS for details"));		\
}

#define ZYENCODE_ZYDECODE_DEF_DEFINED
#endif
