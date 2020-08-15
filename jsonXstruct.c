/**
 * @file jsonXstruct.c
 * @brief A library for converting between json and C-Struct, based on json-c, easy to use.
 *
 * @author lihan(1104616563@qq.com)
 * @date 2019-12-11
 *
 */

#include <string.h>
#include "jsonXstruct_priv.h"

static int  jxs_log_level = JXS_LOG_ERROR;
static void (*jxs_log_callback)(int, const char *, va_list) = jxs_log_default_callback;

static void jxs_log_default_callback(int level, const char *fmt, va_list vl)
{
	if (level > JXS_LOG_ERROR) {
		vfprintf(stdout, fmt, vl);
	} else if (level > JXS_LOG_QUIET) {
		vfprintf(stderr, fmt, vl);
	}
}

static void print_log(int level, const char *fmt, ...)
{
	void (*log_callback)(int, const char *, va_list) = jxs_log_callback;
	if (!log_callback) {
		return;
	}
	va_list vl;
	va_start(vl, fmt);
	log_callback(level, fmt, vl);
	va_end(vl);
}

static const char *get_log_tag(int level){
	static const char *tag_name[] = {
		[JXS_LOG_QUIET] = "quiet",
		[JXS_LOG_FATAL] = "fatal",
		[JXS_LOG_ERROR] = "error",
		[JXS_LOG_WARN]  = "warn",
		[JXS_LOG_INFO]  = "info",
		[JXS_LOG_DEBUG] = "debug",
		[JXS_LOG_TRACE] = "trace",
	};
	if ((level < 0) || (level >= (int)(JXS_NELEM(tag_name)))) {
		return "";
	}
	return tag_name[level];
}

static inline jmap_head_t *get_jmhead(jxs_mapper *mapper)
{
	return &mapper[0].jmhead;
}

static inline jmap_list_t *get_jmlist(jxs_mapper *mapper)
{
	return &mapper[1].jmlist;
}

/**
 * @brief Return a string describing the type of the element type.
 * e.g. "int", or "object", etc...
 * @param type jmap type
 * @return  string or NULL
 */
static const char *type_to_name(jxs_type type)
{
	static const char *jxs_type_name[] = {
		[jxs_type_null]    = "null",
		[jxs_type_boolean] = "boolean",
		[jxs_type_double]  = "double",
		[jxs_type_int]     = "int",
		[jxs_type_string]  = "string",
		[jxs_type_struct]  = "struct",
		[jxs_type_object]  = "object",
		[jxs_type_array]   = "array",
	};
	if ((type < 0) || (type >= (JXS_NELEM(jxs_type_name)))) {
		jxs_log(JXS_LOG_ERROR, "jmap type error[%d].\n", type);
		return NULL;
	}
	return jxs_type_name[type];
}

/**
 * @brief [Multidimensional Arrays] Set the jmap of the next dimension of the
 * array, Until the last dimension of the array is traversed.
 * @param  new_jmitem new jmap item for the next dimension of array.
 * @param  jmitem     parent jmap item.
 * @param  idx        index of the array.
 */
static void jmap_array_move_next_dimen(jmap_item_t *new_jmitem,
                                       jmap_item_t *jmitem, size_t idx)
{
	memset(new_jmitem, 0, sizeof(jmap_item_t));
	if ((jmitem->arr.cur_depth >= jmitem->arr.depth) ||
	    (jmitem->arr.deptab[0] == 0)) {
		jxs_log(JXS_LOG_FATAL, "array depth error.\n");
		return;
	}
	new_jmitem->key      = jmitem->key;
	new_jmitem->subjm    = jmitem->subjm;
	new_jmitem->basetype = jmitem->basetype;
	/* calculate the starting address of the current array */
	new_jmitem->offset = jmitem->offset + (ptrdiff_t)(jmitem->size * idx);
	/* If it is the last dimension, set the current arr_depth type
	 * as the base type, otherwise it is array type */
	new_jmitem->type = ((jmitem->arr.depth - jmitem->arr.cur_depth) == 1) ?
	                   jmitem->basetype : jxs_type_array;
	new_jmitem->size          = jmitem->size / jmitem->arr.deptab[0];
	new_jmitem->arr.length    = jmitem->arr.deptab[0];
	new_jmitem->arr.depth     = jmitem->arr.depth;
	new_jmitem->arr.cur_depth = jmitem->arr.cur_depth + 1;
	/* Shift 'array record table' to the left */
	memcpy(new_jmitem->arr.deptab, &(jmitem->arr.deptab[1]),
	       sizeof(jmitem->arr.deptab) - sizeof(jmitem->arr.deptab[0]));
	new_jmitem->rule = jmitem->rule;
}

/**
 * @brief [struct array] Modify mapper to point to the next struct of the array.
 * @param mapper  mapper object
 * @param stsize  size of a single struct of the array.
 * @param idx     array index
 */
static void jmap_struct_move_forward(jxs_mapper *mapper, size_t stsize, size_t idx)
{
	size_t i = 0;
	if (mapper) {
		jmap_head_t *jmhead = get_jmhead(mapper);
		jmap_list_t *jmlist = get_jmlist(mapper);
		for (i = 0; i < jmhead->idx; i++) {
			jmap_item_t *jmitem = &jmlist[i];
			jmitem->offset = jmitem->offset + (ptrdiff_t)(stsize * idx);
		}
	}
}

/**
 * @brief [struct array] mapper should be reset before the next struct use it.
 * @param mapper  mapper object.
 * @param stsize  size of a single struct of the array.
 * @param idx     array index.
 * @note  Must be used in pairs with 'jmap_struct_move_forward()'
 */
static void jmap_struct_move_backward(jxs_mapper *mapper, size_t stsize, size_t idx)
{
	size_t i = 0;
	if (mapper) {
		jmap_head_t *jmhead = get_jmhead(mapper);
		jmap_list_t *jmlist = get_jmlist(mapper);
		for (i = 0; i < jmhead->idx; i++) {
			jmap_item_t *jmitem = &jmlist[i];
			jmitem->offset = jmitem->offset - (ptrdiff_t)(stsize * idx);
		}
	}
}

/**
 * @brief mapper print warpper
 *
 * @param  jmhead    mapper head.
 * @param  jmitem    jmap item.
 * @param  idx       array index.
 * @param  locator   current locator.
 */
static void jmap_print_warpper(jmap_context_t *ctx, jmap_head_t *jmhead,
                               jmap_item_t *jmitem, size_t idx, const char *locator)
{
	void     *vptr   = (uint8_t *)jmhead->start_addr + jmitem->offset;
	jxs_type  type   = jmitem->type;
	size_t    size   = jmitem->size;
	ptrdiff_t offset = 0;
	if (vptr == NULL) {
		jxs_log(JXS_LOG_ERROR, "%s: jmap struct addr is null.\n", locator);
		return;
	}
	vptr   = (uint8_t *)vptr + size * idx;
	offset = (ptrdiff_t)vptr - (ptrdiff_t)ctx->start_addr;
	switch (type) {
	case jxs_type_null:
		/* nothing to do */
		PRINT_JMITEM(jmitem, "NULL");
		break;

	case jxs_type_boolean:
		if (TYPEOF(size, int)) {
			PRINT_JMITEM(jmitem, "%d", *((int *)vptr));
		} else if (TYPEOF(size, bool)) {
			PRINT_JMITEM(jmitem, "%d", *((bool *)vptr));
		}
		break;

	case jxs_type_double:
		if (TYPEOF(size, double)) {
			PRINT_JMITEM(jmitem, "%lf", *((double *)vptr));
		} else if (TYPEOF(size, float)) {
			PRINT_JMITEM(jmitem, "%f", *((float *)vptr));
		}
		break;

	case jxs_type_int:
		if (TYPEOF(size, int64_t)) {
			PRINT_JMITEM(jmitem, "%" PRId64 "", *((int64_t *)vptr));
		} else if (TYPEOF(size, int32_t)) {
			PRINT_JMITEM(jmitem, "%" PRId32 "", *((int32_t *)vptr));
		} else if (TYPEOF(size, int16_t)) {
			PRINT_JMITEM(jmitem, "%" PRId16 "", *((int16_t *)vptr));
		} else if (TYPEOF(size, int8_t)) {
			PRINT_JMITEM(jmitem, "%" PRId8 "", *((int8_t *)vptr));
		}
		break;

	case jxs_type_string:
		PRINT_JMITEM(jmitem, "%s", *((char(*)[])vptr));
		break;

	case jxs_type_object: {
		json_object *jso = NULL;
		jso = *(json_object **)vptr;
		if (jso) {
			PRINT_JMITEM(jmitem, "%s", json_object_to_json_string(jso));
		} else {
			PRINT_JMITEM(jmitem, "NULL");
		}
		break;
	}

	case jxs_type_struct: {
		jmap_head_t *sub_jmhead = get_jmhead(jmitem->subjm);
		sub_jmhead->start_addr = (uint8_t *)jmhead->start_addr + jmitem->offset;
		jmap_struct_move_forward(jmitem->subjm, size, idx);
		PRINT_JMITEM(jmitem, "[JMAP:%p][OFFSET:%8" FMT_PTRDIFF_T "]", jmitem->subjm,
		             (ptrdiff_t)sub_jmhead->start_addr - (ptrdiff_t)ctx->start_addr);
		jmap_struct_print(ctx, jmitem->subjm, locator);
		jmap_struct_move_backward(jmitem->subjm, size, idx);
		break;
	}

	case jxs_type_array: {
		jmap_item_t new_jmitem;
		jmap_array_move_next_dimen(&new_jmitem, jmitem, idx);
		PRINT_JMITEM(&new_jmitem, "[depth(%" FMT_SIZE_T "), type(%s), "
		             "form(%" FMT_SIZE_T "x%" FMT_SIZE_T ")]",
		             new_jmitem.arr.depth - new_jmitem.arr.cur_depth + 1,
		             type_to_name(new_jmitem.type),
		             new_jmitem.arr.length, new_jmitem.size);
		jmap_array_print(ctx, jmhead, &new_jmitem, locator);
		break;
	}

	default:
		jxs_log(JXS_LOG_ERROR, "%s<error type>\n", jmitem->key);
		break;
	}
}

/**
 * @brief array type print
 *
 * @param  jmhead    mapper head.
 * @param  jmitem    jmap item.
 * @param  locator   current locator.
 */
static void jmap_array_print(jmap_context_t *ctx, jmap_head_t *jmhead,
                             jmap_item_t *jmitem, const char *locator)
{
	size_t i = 0;
	for (i = 0; i < jmitem->arr.length; i++) {
		char *new_locator = NULL;
		SET_NEW_LOCATOR(new_locator, locator, jmitem->key, 1, i);
		jmap_print_warpper(ctx, jmhead, jmitem, i, new_locator);
	}
}

/**
 * @brief struct type print
 *
 * @param  mapper    mapper object.
 * @param  locator   current locator.
 */
static void jmap_struct_print(jmap_context_t *ctx, jxs_mapper *mapper, const char *locator)
{
	size_t       i      = 0;
	jmap_head_t *jmhead = NULL;
	jmap_list_t *jmlist = NULL;
	if (mapper == NULL) {
		jxs_log(JXS_LOG_ERROR, "mapper null.\n");
		return;
	}
	jmhead = get_jmhead(mapper);
	jmlist = get_jmlist(mapper);
	for (i = 0; i < jmhead->idx; i++) {
		jmap_item_t *jmitem      = &jmlist[i];
		char        *new_locator = NULL;
		SET_NEW_LOCATOR(new_locator, locator, jmitem->key, 0, 0);
		jmap_print_warpper(ctx, jmhead, jmitem, 0, new_locator);
	}
}

static item_action jmap_convert_handler(jmap_context_t *ctx, jmap_item_t *jmitem, void *vptr,
                                        json_object **jso, const char *locator)
{
	jxs_type type     = jmitem->type;
	size_t   size     = jmitem->size;
	uint8_t  rule     = jmitem->rule;
	bool     is_force = false;
	if (rule == 0) {
		jxs_log(JXS_LOG_ERROR, "rule flag set error.\n");
		return RULE_ITEM_ERROR;
	}
	ctx->now.vptr = vptr;
	if (ctx->convert.callback) {
		ctx->convert.callback(ctx);
		if (ctx->convert.rule != 0) {
			/* if complex rule is set, overriding basic rules and force to check rules */
			rule     = ctx->convert.rule;
			is_force = true;
			jxs_log(JXS_LOG_TRACE, "%s: set complex rule: 0x%08x.\n", locator, rule);
		}
		/* reset rule flag */
		ctx->convert.rule = 0;
	}
	if (rule == JXS_RULE_KEEP_RAW) {
		// jxs_log(JXS_LOG_TRACE, "%s: keep raw.\n", locator);
		return RULE_ITEM_KEEP;
	}
	/* if complex rule not set, the rule action can only take effect when the data is empty */
	if (!is_force) {
		bool is_empty = false;
		switch (type) {
		case jxs_type_null:
			/* nothing to do */
			break;

		case jxs_type_boolean: {
			int tmpbool = 0;
			if (TYPEOF(size, int)) {
				tmpbool = *((int *)vptr);
			} else if (TYPEOF(size, bool)) {
				tmpbool = *((bool *)vptr);
			}
			if (tmpbool == false) {
				is_empty = true;
			}
			break;
		}

		case jxs_type_double: {
			double tmpdouble = 0;
			if (TYPEOF(size, double)) {
				tmpdouble = *((double *)vptr);
			} else if (TYPEOF(size, float)) {
				tmpdouble = *((float *)vptr);
			}
			if (tmpdouble == 0) {
				is_empty = true;
			}
			break;
		}

		case jxs_type_int: {
			int64_t tmpint = 0;
			if (TYPEOF(size, int64_t)) {
				tmpint = *((int64_t *)vptr);
			} else if (TYPEOF(size, int32_t)) {
				tmpint = *((int32_t *)vptr);
			} else if (TYPEOF(size, int16_t)) {
				tmpint = *((int16_t *)vptr);
			} else if (TYPEOF(size, int8_t)) {
				tmpint = *((int8_t *)vptr);
			}
			if (tmpint == 0) {
				is_empty = true;
			}
			break;
		}

		case jxs_type_string: {
			char *tmpstr = *((char(*)[])vptr);
			if ((tmpstr == NULL) || (tmpstr[0] == '\0')) {
				is_empty = true;
			}
			break;
		}

		case jxs_type_object: {
			json_object *tmpjso = NULL;
			tmpjso = *((json_object **)vptr);
			if (tmpjso == NULL) {
				is_empty = true;
			}
			break;
		}

		case jxs_type_struct:
		case jxs_type_array:
		default:
			break;
		}
		/* If the data is not empty, don't modify anything */
		if (!is_empty) {
			// jxs_log(JXS_LOG_TRACE, "%s: keep raw.\n", locator);
			return RULE_ITEM_KEEP;
		}
	}
	if (rule == JXS_RULE_DROP_SELF) {
		// jxs_log(JXS_LOG_TRACE, "%s: delete it.\n", locator);
		return RULE_ITEM_DELETE;
	} else if (rule == JXS_RULE_SET_NULL) {
		*jso = NULL;
		// jxs_log(JXS_LOG_TRACE, "%s: set null.\n", locator);
		return RULE_ITEM_SET;
	}
	// jxs_log(JXS_LOG_TRACE, "%s: keep raw.\n", locator);
	return RULE_ITEM_KEEP;
}

static int jmap_to_json_warpper(jmap_context_t *ctx, jmap_head_t *jmhead,
                                jmap_item_t *jmitem, size_t idx,
                                json_object **jso, const char *locator)
{
	int ret = 0;
	json_object *item_jso = NULL;
	void        *vptr     = (uint8_t *)jmhead->start_addr + jmitem->offset;
	jxs_type     type     = jmitem->type;
	size_t       size     = jmitem->size;
	item_action  action   = 0;
	if (vptr == NULL) {
		jxs_log(JXS_LOG_ERROR, "%s: jmap struct addr is null.\n", locator);
		return -1;
	}
	vptr   = (uint8_t *)vptr + size * idx;
	action = jmap_convert_handler(ctx, jmitem, vptr, &item_jso, locator);
	if (action == RULE_ITEM_ERROR) {
		jxs_log(JXS_LOG_WARN, "Rules handling error, skip rules.\n");
	} else if (action == RULE_ITEM_DELETE) {
		/* delete current item */
		ret = 1;
		goto ok;
	} else if (action == RULE_ITEM_SET) {
		/* set item_jso inside, so skip the following operations and return. */
		ret = 0;
		goto ok;
	}
	switch (type) {
	case jxs_type_null:
		item_jso = NULL;
		break;

	case jxs_type_boolean:
		if (TYPEOF(size, int)) {
			item_jso = json_object_new_boolean(*((int *)vptr));
		} else if (TYPEOF(size, bool)) {
			item_jso = json_object_new_boolean(*((bool *)vptr));
		} else {
			jxs_log(JXS_LOG_ERROR, "%s: required type '%s' <sizeof> does not match.\n",
			        locator, type_to_name(type));
			goto end;
		}
		break;

	case jxs_type_double:
		if (TYPEOF(size, double)) {
			item_jso = json_object_new_double(*((double *)vptr));
		} else if (TYPEOF(size, float)) {
			item_jso = json_object_new_double(*((float *)vptr));
		} else {
			jxs_log(JXS_LOG_ERROR, "%s: required type '%s' <sizeof> does not match.\n",
			        locator, type_to_name(type));
			goto end;
		}
		break;

	case jxs_type_int:
		if (TYPEOF(size, int64_t)) {
			item_jso = json_object_new_int64(*((int64_t *)vptr));
		} else if (TYPEOF(size, int32_t)) {
			item_jso = json_object_new_int(*((int32_t *)vptr));
		} else if (TYPEOF(size, int16_t)) {
			item_jso = json_object_new_int(*((int16_t *)vptr));
		} else if (TYPEOF(size, int8_t)) {
			item_jso = json_object_new_int(*((int8_t *)vptr));
		} else {
			jxs_log(JXS_LOG_ERROR, "%s: required type '%s' <sizeof> does not match.\n",
			        locator, type_to_name(type));
			goto end;
		}
		break;

	case jxs_type_string: {
		char *tmpstr = *((char(*)[])vptr);
		item_jso = json_object_new_string(tmpstr);
		break;
	}

	case jxs_type_object:
		item_jso = json_object_get(*((json_object **)vptr));
		break;

	case jxs_type_struct:
		if (jmitem->subjm == NULL) {
			jxs_log(JXS_LOG_ERROR, "%s: Please create a mapper for your nested struct..\n", locator);
			goto end;
		}
		item_jso = json_object_new_object();
		if (item_jso == NULL) {
			jxs_log(JXS_LOG_ERROR, "%s: new json object error.\n", locator);
			goto end;
		}
		jmap_head_t *sub_jmhead = get_jmhead(jmitem->subjm);
		sub_jmhead->start_addr = (uint8_t *)jmhead->start_addr + jmitem->offset;
		jmap_struct_move_forward(jmitem->subjm, size, idx);
		if ((ret = jmap_to_json_object(ctx, jmitem->subjm, item_jso)) == -1) {
			jxs_log(JXS_LOG_ERROR, "%s: struct to json error.\n", locator);
			goto end;
		}
		jmap_struct_move_backward(jmitem->subjm, size, idx);
		break;

	case jxs_type_array: {
		jmap_item_t new_jmitem;
		jmap_array_move_next_dimen(&new_jmitem, jmitem, idx);
		item_jso = json_object_new_array();
		if (item_jso == NULL) {
			jxs_log(JXS_LOG_ERROR, "%s: new json array object error.\n", locator);
			goto end;
		}
		if ((ret = jmap_to_json_array(ctx, jmhead, &new_jmitem, item_jso)) == -1) {
			jxs_log(JXS_LOG_ERROR, "%s: array to json error.\n", locator);
			goto end;
		}
		break;
	}

	default:
		jxs_log(JXS_LOG_ERROR, "%s: error mapper type.\n", locator);
		goto end;
	}
ok:
	*jso = item_jso;
	return ret;
end:
	if (item_jso) {
		jxs_log(JXS_LOG_ERROR, "%s: call json_object_put to free it.\n", locator);
		json_object_put(item_jso);
	}
	return ret;
}

static int jmap_from_json_warpper(jmap_context_t *ctx, jmap_head_t *jmhead,
                                  jmap_item_t *jmitem, size_t idx,
                                  json_object *jso, const char *locator)
{
	json_object *item_jso = jso;
	void        *vptr     = (uint8_t *)jmhead->start_addr + jmitem->offset;
	jxs_type     type     = jmitem->type;
	size_t       size     = jmitem->size;
	if (vptr == NULL) {
		jxs_log(JXS_LOG_ERROR, "%s: jmap struct addr is null.\n", locator);
		return -1;
	}
	vptr = (uint8_t *)vptr + size * idx;
	switch (type) {
	case jxs_type_null:
		memset(vptr, 0, size);
		break;

	case jxs_type_boolean:
		if (TYPEOF(size, int)) {
			*((int *)vptr) = (int)json_object_get_boolean(item_jso);
		} else if (TYPEOF(size, bool)) {
			*((bool *)vptr) = (bool)json_object_get_boolean(item_jso);
		} else {
			jxs_log(JXS_LOG_ERROR, "%s: required type '%s' <sizeof> does not match.\n",
			        locator, type_to_name(type));
			return -1;
		}
		break;

	case jxs_type_double:
		if (TYPEOF(size, double)) {
			*((double *)vptr) = json_object_get_double(item_jso);
		} else if (TYPEOF(size, float)) {
			*((float *)vptr) = (float)json_object_get_double(item_jso);
		} else {
			jxs_log(JXS_LOG_ERROR, "%s: required type '%s' <sizeof> does not match.\n",
			        locator, type_to_name(type));
			return -1;
		}
		break;

	case jxs_type_int:
		if (TYPEOF(size, int64_t)) {
			*((int64_t *)vptr) = (int64_t)json_object_get_int64(item_jso);
		} else if (TYPEOF(size, int32_t)) {
			*((int32_t *)vptr) = (int32_t)json_object_get_int(item_jso);
		} else if (TYPEOF(size, int16_t)) {
			*((int16_t *)vptr) = (int16_t)json_object_get_int(item_jso);
		} else if (TYPEOF(size, int8_t)) {
			*((int8_t *)vptr) = (int8_t)json_object_get_int(item_jso);
		} else {
			jxs_log(JXS_LOG_ERROR, "%s: required type '%s' <sizeof> does not match.\n",
			        locator, type_to_name(type));
			return -1;
		}
		break;

	case jxs_type_string: {
		const char *tmpstr = NULL;
		tmpstr = json_object_get_string(item_jso);
		if (tmpstr == NULL) {
			memset(vptr, 0, size);
		} else {
			snprintf(*((char(*)[])vptr), size, "%s", tmpstr);
		}
		break;
	}

	case jxs_type_object:
		*((json_object **)vptr) = json_object_get(item_jso);
		break;

	case jxs_type_struct:
		if (jmitem->subjm == NULL) {
			jxs_log(JXS_LOG_ERROR, "%s: Please create a mapper for your nested struct..\n", locator);
			return -1;
		}
		if (item_jso == NULL) {
			memset(vptr, 0, size);
		} else {
			jmap_head_t *sub_jmhead = get_jmhead(jmitem->subjm);
			sub_jmhead->start_addr = (uint8_t *)jmhead->start_addr + jmitem->offset;
			jmap_struct_move_forward(jmitem->subjm, size, idx);
			if (jmap_from_json_object(ctx, jmitem->subjm, item_jso) != 0) {
				jxs_log(JXS_LOG_ERROR, "%s: struct from json error.\n", locator);
				return -1;
			}
			jmap_struct_move_backward(jmitem->subjm, size, idx);
		}
		break;

	case jxs_type_array:
		if (item_jso == NULL) {
			memset(vptr, 0, size);
		} else {
			jmap_item_t new_jmitem;
			jmap_array_move_next_dimen(&new_jmitem, jmitem, idx);
			if (jmap_from_json_array(ctx, jmhead, &new_jmitem, item_jso) != 0) {
				jxs_log(JXS_LOG_ERROR, "%s: array from json error.\n", locator);
				return -1;
			}
		}
		break;

	default:
		jxs_log(JXS_LOG_ERROR, "%s: error mapper type.\n", locator);
		return -1;
	}
	return 0;
}

/**
 * @brief Fill json_object based on array's jmap
 *
 * @param[in]  jmitem  jampitem of the array.
 * @param[out] arrjso  json_object of array.
 * @return 0 for success, -1 for error.
 */
static int jmap_to_json_array(jmap_context_t *ctx, jmap_head_t *jmhead,
                              jmap_item_t *jmitem, json_object *arrjso)
{
	int         ret       = 0;
	size_t      i         = 0;
	const char *locator   = ctx->now.locator;
	const char *fzlocator = ctx->now.fzlocator;
	size_t      arr_len   = jmitem->arr.length;
	if ((jmitem->size == 0) || (arr_len == 0)) {
		jxs_log(JXS_LOG_ERROR, "%s: array 'size', 'len' cannot not be zero.\n", locator);
		return -1;
	}
	/* If not json array type, return error */
	if (json_object_get_type(arrjso) != json_type_array) {
		jxs_log(JXS_LOG_ERROR, "%s: this json_object is not a 'json_type_array' object.\n", locator);
		return -1;
	}
	for (i = 0; i < arr_len; i++) {
		json_object *item_jso = NULL;
		ctx->now.jmitem = jmitem;
		ctx->now.jmhead = jmhead;
		ctx->now.idx    = i;
		SET_NEW_LOCATOR(ctx->now.locator, locator, jmitem->key, 1, i);
		SET_NEW_FUZZY_LOCATOR(ctx->now.fzlocator, fzlocator, jmitem->key, 1);
		ret = jmap_to_json_warpper(ctx, jmhead, jmitem, i, &item_jso, ctx->now.locator);
		if (ret == -1) {
			jxs_log(JXS_LOG_ERROR, "%s: jmap to json error.\n", locator);
			return -1;
		} else if (ret == 0) {
			json_object_array_add(arrjso, item_jso);
		} else {
			jxs_log(JXS_LOG_TRACE, "delete '%s[%" FMT_SIZE_T "]' item.\n", locator, i);
			ret = 0;
		}
	}
	return ret;
}

/**
 * @brief Write the struct to the json_object according to the struct's mapper
 *
 * @param  mapper  [input]struct's mapper
 * @param  jso   [output]json_object, Must be initialized
 * @return 0 for success, -1 for error.
 */
static int jmap_to_json_object(jmap_context_t *ctx, jxs_mapper *mapper, json_object *jso)
{
	int          ret       = 0;
	size_t       i         = 0;
	const char  *locator   = ctx->now.locator;
	const char  *fzlocator = ctx->now.fzlocator;
	jmap_head_t *jmhead    = get_jmhead(mapper);
	jmap_list_t *jmlist    = get_jmlist(mapper);
	if ((mapper == NULL) || (jso == NULL)) {
		jxs_log(JXS_LOG_ERROR, "%s: mapper or json_object is null.\n", locator);
		return -1;
	}
	if (json_object_get_type(jso) != json_type_object) {
		jxs_log(JXS_LOG_ERROR, "%s: this json_object is not a 'json_type_object' object.\n", locator);
		return -1;
	}
	for (i = 0; i < jmhead->idx; i++) {
		json_object *item_jso = NULL;
		jmap_item_t *jmitem   = &jmlist[i];
		ctx->now.jmitem = jmitem;
		ctx->now.jmhead = jmhead;
		ctx->now.idx    = 0;
		SET_NEW_LOCATOR(ctx->now.locator, locator, jmitem->key, 0, 0);
		SET_NEW_FUZZY_LOCATOR(ctx->now.fzlocator, fzlocator, jmitem->key, 0);
		ret = jmap_to_json_warpper(ctx, jmhead, jmitem, 0, &item_jso, ctx->now.locator);
		if (ret == -1) {
			jxs_log(JXS_LOG_ERROR, "%s: jmap to json error.\n", locator);
			return -1;
		} else if (ret == 0) {
			json_object_object_add(jso, jmitem->key, item_jso);
		} else {
			jxs_log(JXS_LOG_TRACE, "%s: delete current item.\n", locator);
			ret = 0;
		}
	}
	return ret;
}

/**
 * @brief Fill array's jmap based on json_object
 *
 * @param  jmitem     [output]Jmap of the base address of the array.
 * @param  arr_jso  [input]json_object of array type.
 * @return 0 for success, -1 for error.
 */
static int jmap_from_json_array(jmap_context_t *ctx, jmap_head_t *jmhead,
                                jmap_item_t *jmitem, json_object *arrjso)
{
	size_t      i         = 0;
	const char *locator   = ctx->now.locator;
	const char *fzlocator = ctx->now.fzlocator;
	size_t      arr_len   = jmitem->arr.length;
	size_t      jarr_len  = 0;
	if (arrjso == NULL) {
		jxs_log(JXS_LOG_ERROR, "%s: array json_object is null.\n", locator);
		return -1;
	}
	if ((jmitem->size == 0) || (arr_len == 0)) {
		jxs_log(JXS_LOG_ERROR, "%s: array 'size', 'len' cannot not be zero.\n", locator);
		return -1;
	}
	if (json_object_get_type(arrjso) != json_type_array) {
		jxs_log(JXS_LOG_ERROR, "%s: this json_object is not a 'json_type_array' object.\n", locator);
		return -1;
	}
	/* If the json array length exceeds the buf value, the excess is discarded */
	jarr_len = json_object_array_length(arrjso);
	if (jarr_len > arr_len) {
		jxs_log(JXS_LOG_WARN, "%s: array length exceeds the buffer, throw it.\n", locator);
	}
	arr_len = jarr_len;
	for (i = 0; i < arr_len; i++) {
		ctx->now.jmitem = jmitem;
		ctx->now.jmhead = jmhead;
		ctx->now.idx    = i;
		SET_NEW_LOCATOR(ctx->now.locator, locator, jmitem->key, 1, i);
		SET_NEW_FUZZY_LOCATOR(ctx->now.fzlocator, fzlocator, jmitem->key, 1);
		if (jmap_from_json_warpper(ctx, jmhead, jmitem, i,
		                           json_object_array_get_idx(arrjso, i),
		                           ctx->now.locator) != 0) {
			jxs_log(JXS_LOG_ERROR, "%s: jmap from json error.\n", locator);
			return -1;
		}
	}
	return 0;
}

/**
 * @brief Write the data in the json_object to the struct through jmap
 *
 * @param  jmap  struct's mapper
 * @param  jso   json_object, Must be initialized
 * @return 0 for success, -1 for error.
 */
static int jmap_from_json_object(jmap_context_t *ctx, jxs_mapper *mapper, json_object *jso)
{
	size_t       i         = 0;
	const char  *locator   = ctx->now.locator;
	const char  *fzlocator = ctx->now.fzlocator;
	jmap_head_t *jmhead    = get_jmhead(mapper);
	jmap_list_t *jmlist    = get_jmlist(mapper);
	if (mapper == NULL) {
		jxs_log(JXS_LOG_ERROR, "%s: mapper cannot be null.\n", locator);
		return -1;
	}
	if (jso == NULL) {
		jxs_log(JXS_LOG_ERROR, "%s: json_object is null.\n", locator);
		return -1;
	}
	for (i = 0; i < jmhead->idx; i++) {
		jmap_item_t *jmitem = &jmlist[i];
		ctx->now.jmitem = jmitem;
		ctx->now.jmhead = jmhead;
		ctx->now.idx    = 0;
		SET_NEW_LOCATOR(ctx->now.locator, locator, jmitem->key, 0, 0);
		SET_NEW_FUZZY_LOCATOR(ctx->now.fzlocator, fzlocator, jmitem->key, 0);
		if (jmap_from_json_warpper(ctx, jmhead, jmitem, 0,
		                           json_object_object_get(jso, jmitem->key),
		                           ctx->now.locator) != 0) {
			jxs_log(JXS_LOG_ERROR, "%s: jmap from json error.\n", locator);
			return -1;
		}
	}
	return 0;
}

/**
 * @brief delete the mapper, You should call it only for the top-arr_depth mapper.
 * delete both top mapper and child mapper will cause a double free. Please
 * ensure you are calling it for the top-arr_depth mapper.
 *     1. call @ref json_object_put() to decrement the reference count of the
 *        json_object items.
 *     2. free all the mapper new by @ref jxs_map_basic_new().
 *     3. decrement the reference count of the mapper's json_object.
 * @param  mapper  [input]struct's mapper
 * @see jxs_map_basic_new()
 */
static void jxs_map_basic_delete(jxs_mapper *mapper)
{
	size_t i = 0;
	if (mapper) {
		jmap_head_t *jmhead = get_jmhead(mapper);
		jmap_list_t *jmlist = get_jmlist(mapper);
		/* Last reference */
		if (jmhead->ref == 1) {
			for (i = 0; i < jmhead->idx; i++) {
				jmap_item_t *jmitem = &jmlist[i];
				if ((jmitem->type == jxs_type_struct) || (jmitem->basetype == jxs_type_struct)) {
					jxs_map_basic_delete(jmitem->subjm);
				}
			}
			jxs_log(JXS_LOG_INFO, "JMAP DELETE[%p]%s\n", mapper, jmhead->isbuf ? "(BUFFER)" : "");
			if (jmhead->isbuf == false) {
				free(mapper);
			}
		} else {
			/* Decrease the reference count value */
			jmhead->ref--;
		}
	}
}

jxs_mapper *jxs_map_basic_new(void *context, size_t num)
{
	jxs_mapper     *mapper = NULL;
	jmap_head_t    *jmhead = NULL;
	jmap_context_t *ctx    = (jmap_context_t *)context;
	if (num == 0) {
		jxs_log(JXS_LOG_ERROR, "jmap item cannot be 0.\n");
		return NULL;
	}
	if (context == NULL) {
		jxs_log(JXS_LOG_ERROR, "context data cannot be 0.\n");
		return NULL;
	}
	if ((MAPPER_BUFFER_LENGTH - ctx->buf.idx) < (num + 1)) {
		mapper = (jxs_mapper *)calloc(num + 1, sizeof(jxs_mapper));
		if (mapper == NULL) {
			jxs_log(JXS_LOG_ERROR, "jmap new failed.\n");
			return NULL;
		}
		jmhead        = get_jmhead(mapper);
		jmhead->isbuf = false;
	} else {
		/* Prefer to use local variables to store mapper to improve performance,
		 * Avoid malloc and free memory frequently. buffer length defined by macro
		 * MAPPER_BUFFER_LENGTH.
		 */
		mapper        = &ctx->buf.arr[ctx->buf.idx];
		ctx->buf.idx += (num + 1);
		jmhead        = get_jmhead(mapper);
		jmhead->isbuf = true;
	}
	jmhead->limit      = num;
	jmhead->start_addr = ctx->start_addr;
	jxs_log(JXS_LOG_INFO, "JMAP NEW[%p]%s\n", mapper, jmhead->isbuf ? "(BUFFER)" : "");
	return mapper;
}

jxs_item *jxs_item_basic_add(jxs_mapper *mapper, jxs_type type, const char *key,
                             ptrdiff_t offset, size_t mbsize, jxs_mapper *subjm, ...)
{
	size_t       i = 0;
	va_list      ap;
	jmap_head_t *jmhead = NULL;
	jmap_list_t *jmlist = NULL;
	jmap_item_t *jmitem = NULL;
	if (mapper == NULL) {
		jxs_log(JXS_LOG_ERROR, "mapper cannot be null.\n");
		return NULL;
	}
	if (type == jxs_type_array) {
		jxs_log(JXS_LOG_ERROR, "'Multi-Dimen Array' Usage error.\n");
		return NULL;
	} else if ((type == jxs_type_struct) && (subjm == NULL)) {
		jxs_log(JXS_LOG_ERROR, "you must specify a mapper for the sub-struct.\n");
		return NULL;
	}
	jmhead = get_jmhead(mapper);
	jmlist = get_jmlist(mapper);
	/* avoid overflow */
	if (jmhead->idx >= jmhead->limit) {
		jxs_log(JXS_LOG_ERROR, "add too many, drop it.\n");
		return NULL;
	}
	jmitem = &jmlist[jmhead->idx];
	memset(jmitem, 0, sizeof(jmap_item_t));
	jmitem->key      = key;
	jmitem->offset   = offset;
	jmitem->size     = mbsize;
	jmitem->subjm    = subjm;
	jmitem->basetype = type;
	va_start(ap, subjm);
	for (i = 0; i < JXS_NELEM(jmitem->arr.deptab); i++) {
		// TODO: does 'int' is engouth?
		jmitem->arr.deptab[i] = (size_t)va_arg(ap, int);
		/* Use 0 to mark the end of the variable argument list */
		if (jmitem->arr.deptab[i] == 0) {
			jmitem->arr.depth = i;
			break;
		}
	}
	va_end(ap);
	/* If no 0 is received, the array exceeds a predefined maximum depth */
	if (jmitem->arr.deptab[JXS_ARRAY_DEPTH] != 0) {
		jxs_log(JXS_LOG_ERROR, "Exceeds the predefined maximum array depth[%d], "
		        "you can modify the MACRO manually.\n", JXS_ARRAY_DEPTH);
		return NULL;
	}
	/* If it is not an array, the type is equal to the basic type,
	 * otherwise it is an array type */
	jmitem->type = (jmitem->arr.depth == 0) ? jmitem->basetype : jxs_type_array;
	jmitem->rule = JXS_RULE_KEEP_RAW;
	/* Increase the jmapper reference count */
	if (subjm) {
		jmap_head_t *head = get_jmhead(subjm);
		head->ref++;
	}
	/* increment the index */
	jmhead->idx++;
	return jmitem;
}

static int check_ref_count(jxs_mapper *mapper)
{
	jmap_head_t *head = get_jmhead(mapper);
	if (head->ref != 0) {
		jxs_log(JXS_LOG_ERROR, "The return value of the descriptor callback "
		        "function is incorrect, it needs to be the top-level mapper, "
		        "otherwise, a memory leak will occur.\n");
		return -1;
	}
	/*set top-level mapper reference count is 1 */
	head->ref = 1;
	return 0;
}

void jxs_print_struct(jxs_descriptor func, void *stptr, void *opaque)
{
	jxs_mapper    *mapper = NULL;
	jxs_mapper     buffer[MAPPER_BUFFER_LENGTH] = { { .jmhead = { 0 } } };
	jmap_context_t ctx;
	memset(&ctx, 0, sizeof(jmap_context_t));
	ctx.buf.arr = buffer;
	if ((func == NULL) || (stptr == NULL)) {
		jxs_log(JXS_LOG_ERROR, "constructor or struct cannot be null.\n");
		return;
	}
	ctx.start_addr = stptr;
	ctx.opaque     = opaque;
	if ((mapper = func(&ctx)) == NULL) {
		jxs_log(JXS_LOG_ERROR, "mapper cannot be null.\n");
		return;
	}
	if (check_ref_count(mapper) != 0) {
		jxs_log(JXS_LOG_ERROR, "Incorrect mapper returned.\n");
		goto end;
	}
	jmap_struct_print(&ctx, mapper, NULL);
end:
	jxs_map_basic_delete(mapper);
}

json_object *jxs_struct_to_json_object(jxs_descriptor func,
                                       void *stptr, void *opaque)
{
	int ret = 0;
	json_object   *jso    = NULL;
	jxs_mapper    *mapper = NULL;
	jxs_mapper     buffer[MAPPER_BUFFER_LENGTH] = { { .jmhead = { 0 } } };
	jmap_context_t ctx;
	memset(&ctx, 0, sizeof(jmap_context_t));
	ctx.buf.arr = buffer;
	if ((func == NULL) || (stptr == NULL)) {
		jxs_log(JXS_LOG_ERROR, "constructor or struct cannot be null.\n");
		ret = -1;
		goto end;
	}
	jso = json_object_new_object();
	if (jso == NULL) {
		jxs_log(JXS_LOG_ERROR, "json_object new failed.\n");
		ret = -1;
		goto end;
	}
	ctx.start_addr = stptr;
	ctx.opaque     = opaque;
	if ((mapper = func(&ctx)) == NULL) {
		jxs_log(JXS_LOG_ERROR, "mapper cannot be null.\n");
		ret = -1;
		goto end;
	}
	if (check_ref_count(mapper) != 0) {
		jxs_log(JXS_LOG_ERROR, "Incorrect mapper returned.\n");
		ret = -1;
		goto end;
	}
	if (jmap_to_json_object(&ctx, mapper, jso) != 0) {
		jxs_log(JXS_LOG_ERROR, "jmap to json [%p] error.\n", jso);
		ret = -1;
		goto end;
	}
end:
	jxs_map_basic_delete(mapper);
	if ((ret != 0) && jso) {
		json_object_put(jso);
		jso = NULL;
	}
	return jso;
}

int jxs_struct_from_json_object(jxs_descriptor func,
                                void *stptr, void *opaque, json_object *jso)
{
	int            ret    = 0;
	jxs_mapper    *mapper = NULL;
	jxs_mapper     buffer[MAPPER_BUFFER_LENGTH] = { { .jmhead = { 0 } } };
	jmap_context_t ctx;
	memset(&ctx, 0, sizeof(jmap_context_t));
	ctx.buf.arr = buffer;
	if ((func == NULL) || (stptr == NULL) || (jso == NULL)) {
		jxs_log(JXS_LOG_ERROR, "constructor, struct or jso cannot be null.\n");
		ret = -1;
		goto end;
	}
	ctx.start_addr = stptr;
	ctx.opaque     = opaque;
	if ((mapper = func(&ctx)) == NULL) {
		jxs_log(JXS_LOG_ERROR, "mapper cannot be null.\n");
		ret = -1;
		goto end;
	}
	if (check_ref_count(mapper) != 0) {
		jxs_log(JXS_LOG_ERROR, "Incorrect mapper returned.\n");
		ret = -1;
		goto end;
	}
	if (jmap_from_json_object(&ctx, mapper, jso) != 0) {
		jxs_log(JXS_LOG_ERROR, "jmap from json [%p] error.\n", jso);
		ret = -1;
		goto end;
	}
end:
	jxs_map_basic_delete(mapper);
	return ret;
}

const char *jxs_struct_to_json_string_ext(jxs_descriptor func, void *stptr,
                                          void *opaque, int flags)
{
	const char  *jstring = NULL;
	const char  *copy    = NULL;
	json_object *jso     = NULL;
	jso = jxs_struct_to_json_object(func, stptr, opaque);
	if (jso == NULL) {
		jxs_log(JXS_LOG_ERROR, "struct to json_object failed.\n");
		jstring = NULL;
		goto end;
	}
#if (JSON_C_VERSION_NUM >= 0xb00)
	jstring = json_object_to_json_string_ext(jso, flags);
#else
	(void)flags;
	jstring = json_object_to_json_string(jso);
#endif
	if (jstring == NULL) {
		jxs_log(JXS_LOG_ERROR, "json object to string error.\n");
		goto end;
	} else {
		copy = strdup(jstring);
	}
end:
	if (jso) {
		json_object_put(jso);
	}
	return copy;
}

const char *jxs_struct_to_json_string(jxs_descriptor func, void *stptr, void *opaque)
{
#ifdef JSON_C_TO_STRING_PLAIN
	return jxs_struct_to_json_string_ext(func, stptr, opaque, JSON_C_TO_STRING_PLAIN);
#else
	return jxs_struct_to_json_string_ext(func, stptr, opaque, 0);
#endif
}

void jxs_free_json_string(char *jstring)
{
	if (jstring) {
		free(jstring);
	}
}

int jxs_struct_from_json_string(jxs_descriptor func,
                                void *stptr, void *opaque,
                                const char *jstring)
{
	int ret = 0;
	json_object *jso = NULL;
	if (jstring == NULL) {
		jxs_log(JXS_LOG_ERROR, "json string cannot be null.\n");
		ret = -1;
		goto end;
	}
	jso = json_tokener_parse(jstring);
	if (jso == NULL) {
		jxs_log(JXS_LOG_ERROR, "json string parse error.\n");
		ret = -1;
		goto end;
	}
	if (jxs_struct_from_json_object(func, stptr, opaque, jso) != 0) {
		jxs_log(JXS_LOG_ERROR, "jmap from json [%p] error.\n", jso);
		ret = -1;
		goto end;
	}
end:
	if (jso) {
		json_object_put(jso);
	}
	return ret;
}

int jxs_struct_to_file_ext(jxs_descriptor func,
                           void *stptr, void *opaque,
                           const char *filename, int flags)
{
	int ret = 0;
	json_object *jso = NULL;
	if (filename == NULL) {
		jxs_log(JXS_LOG_ERROR, "filename cannot be null.\n");
		ret = -1;
		goto end;
	}
	jso = jxs_struct_to_json_object(func, stptr, opaque);
	if (jso == NULL) {
		jxs_log(JXS_LOG_ERROR, "struct to json_object failed.\n");
		ret = -1;
		goto end;
	}
#if (JSON_C_VERSION_NUM >= 0xc00)
	ret = json_object_to_file_ext(filename, jso, flags);
#elif (JSON_C_VERSION_NUM >= 0xb00)
	ret = json_object_to_file_ext((char *)filename, jso, flags);
#else
	(void)flags;
	ret = json_object_to_file((char *)filename, jso);
#endif
	if (ret != 0) {
		jxs_log(JXS_LOG_ERROR, "json to file [%s] error.\n", filename);
		ret = -1;
		goto end;
	}
end:
	if (jso) {
		json_object_put(jso);
	}
	return ret;
}

int jxs_struct_to_file(jxs_descriptor func,
                       void *stptr, void *opaque, const char *filename)
{
#ifdef JSON_C_TO_STRING_PLAIN
	return jxs_struct_to_file_ext(func, stptr, opaque,
	                              filename, JSON_C_TO_STRING_PLAIN);
#else
	return jxs_struct_to_file_ext(func, stptr, opaque, filename, 0);
#endif
}

int jxs_struct_from_file(jxs_descriptor func,
                         void *stptr, void *opaque, const char *filename)
{
	int ret = 0;
	json_object *jso = NULL;
	if (filename == NULL) {
		jxs_log(JXS_LOG_ERROR, "filename cannot be null.\n");
		ret = -1;
		goto end;
	}
	jso = json_object_from_file(filename);
	if (jso == NULL) {
		jxs_log(JXS_LOG_ERROR, "json from file [%s] error.\n", filename);
		ret = -1;
		goto end;
	}
	if (jxs_struct_from_json_object(func, stptr, opaque, jso) != 0) {
		jxs_log(JXS_LOG_ERROR, "jmap from json [%p] error.\n", jso);
		ret = -1;
		goto end;
	}
end:
	if (jso) {
		json_object_put(jso);
	}
	return ret;
}

void jxs_item_set_rule(jxs_item *item, jxs_rule rule)
{
	jmap_item_t *jmitem = item;
	if (jmitem == NULL) {
		jxs_log(JXS_LOG_ERROR, "jmitem can not be null.\n");
		return;
	}
	jmitem->rule = (uint8_t)rule;
}

void jxs_item_set_constkey(jxs_item *item, const char *key)
{
	jmap_item_t *jmitem = item;
	if (jmitem == NULL) {
		jxs_log(JXS_LOG_ERROR, "jmitem can not be null.\n");
		return;
	}
	jmitem->key = key;
}

void jxs_set_convert_callback(void *context, void (*cb)(void *))
{
	jmap_context_t *ctx = (jmap_context_t *)context;
	if (ctx == NULL) {
		jxs_log(JXS_LOG_ERROR, "jamp context cannot be null.\n");
		return;
	}
	if (cb == NULL) {
		jxs_log(JXS_LOG_ERROR, "complex rule handler function cannot be null.\n");
		return;
	}
	ctx->convert.callback = cb;
}

const char *jxs_cvt_get_locator(void *context)
{
	jmap_context_t *ctx = (jmap_context_t *)context;
	if (ctx == NULL) {
		jxs_log(JXS_LOG_ERROR, "jmap context cannot be null.\n");
		return NULL;
	}
	return ctx->now.locator;
}

const char *jxs_cvt_get_fuzzy_locator(void *context)
{
	jmap_context_t *ctx = (jmap_context_t *)context;
	if (ctx == NULL) {
		jxs_log(JXS_LOG_ERROR, "jmap context cannot be null.\n");
		return NULL;
	}
	return ctx->now.fzlocator;
}

void *jxs_cvt_get_item_each(void *context)
{
	jmap_context_t *ctx = (jmap_context_t *)context;
	if (ctx == NULL) {
		jxs_log(JXS_LOG_ERROR, "jmap context cannot be null.\n");
		return NULL;
	}
	return ctx->now.vptr;
}

void *jxs_cvt_get_item_fuzzy(void *context, const char *fuzzy_locator)
{
	jmap_context_t *ctx = (jmap_context_t *)context;
	if ((ctx == NULL) || (fuzzy_locator == NULL)) {
		jxs_log(JXS_LOG_ERROR, "jmap context or fuzzy locator cannot be null.\n");
		return NULL;
	}
	if (strcmp(ctx->now.fzlocator, fuzzy_locator) == 0) {
		return ctx->now.vptr;
	}
	return NULL;
}

void *jxs_cvt_get_item(void *context, const char *locator)
{
	jmap_context_t *ctx = (jmap_context_t *)context;
	if ((ctx == NULL) || (locator == NULL)) {
		jxs_log(JXS_LOG_ERROR, "jmap context or locator cannot be null.\n");
		return NULL;
	}
	if (strcmp(ctx->now.locator, locator) == 0) {
		return ctx->now.vptr;
	}
	return NULL;
}

void jxs_cvt_set_item_rule(void *context, jxs_rule rule)
{
	jmap_context_t *ctx = (jmap_context_t *)context;
	if (ctx == NULL) {
		jxs_log(JXS_LOG_ERROR, "jamp context cannot be null.\n");
		return;
	}
	ctx->convert.rule = (uint8_t)rule;
}

void *jxs_get_userdata(void *context)
{
	jmap_context_t *ctx = (jmap_context_t *)context;
	if (ctx == NULL) {
		jxs_log(JXS_LOG_ERROR, "jmap context cannot be null.\n");
		return NULL;
	}
	return ctx->opaque;
}

void jxs_set_loglevel(int level)
{
	jxs_log_level = level;
}

int jxs_get_loglevel(void)
{
	return jxs_log_level;
}

void jxs_set_log_callback(void (*callback)(int, const char *, va_list))
{
	jxs_log_callback = callback;
}

