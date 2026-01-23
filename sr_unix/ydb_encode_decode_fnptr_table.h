/****************************************************************
 *								*
 * Copyright (c) 2025-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

YDB_ENCODE_DECODE_FNPTR (yed_decode_json,	"json_loadb")
YDB_ENCODE_DECODE_FNPTR (yed_obj_iter_next,	"json_object_iter_next")
YDB_ENCODE_DECODE_FNPTR (yed_obj_next_key,	"json_object_iter_key")
YDB_ENCODE_DECODE_FNPTR (yed_obj_next_value,	"json_object_iter_value")
YDB_ENCODE_DECODE_FNPTR (yed_get_obj_iter,	"json_object_iter")
YDB_ENCODE_DECODE_FNPTR (yed_get_size,		"json_array_size")
YDB_ENCODE_DECODE_FNPTR (yed_get_value,		"json_array_get")
YDB_ENCODE_DECODE_FNPTR (yed_get_string_value,	"json_string_value")
YDB_ENCODE_DECODE_FNPTR (yed_get_int_value,	"json_integer_value")
YDB_ENCODE_DECODE_FNPTR (yed_get_real_value,	"json_real_value")
YDB_ENCODE_DECODE_FNPTR (yed_new_object,	"json_object")
YDB_ENCODE_DECODE_FNPTR (yed_new_array,		"json_array")
YDB_ENCODE_DECODE_FNPTR (yed_output_json,	"json_dumpb")
YDB_ENCODE_DECODE_FNPTR (yed_dump_json,		"json_dumps")
YDB_ENCODE_DECODE_FNPTR (yed_new_string,	"json_stringn")
YDB_ENCODE_DECODE_FNPTR (yed_new_integer,	"json_integer")
YDB_ENCODE_DECODE_FNPTR (yed_new_real,		"json_real")
YDB_ENCODE_DECODE_FNPTR (yed_new_false,		"json_false")
YDB_ENCODE_DECODE_FNPTR (yed_new_true,		"json_true")
YDB_ENCODE_DECODE_FNPTR (yed_new_null,		"json_null")
YDB_ENCODE_DECODE_FNPTR (yed_set_object,	"json_object_set_new")
YDB_ENCODE_DECODE_FNPTR (yed_set_array,		"json_array_append_new")
YDB_ENCODE_DECODE_FNPTR (yed_object_delete,	"json_delete")
