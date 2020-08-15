/**
 * @file jsonXstruct_priv.h
 * @brief A library for converting between json and C-Struct, based on json-c, easy to use.
 *
 * @author lihan(1104616563@qq.com)
 * @date 2019-12-11
 *
 */
#ifndef JSONXSTRUCT_PRIV_H
#define JSONXSTRUCT_PRIV_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>
#include "jsonXstruct.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Cross-platform compatibility handling */
#if defined(_MSC_VER)
/*
 * MSVC doesn't have %zu, since it was introduced in C99,
 * but has its own %Iu for printing size_t values.
 */
#define FMT_SIZE_T       "Iu"
#define FMT_SSIZE_T      "Id"
#define FMT_PTRDIFF_T    "Id"
/* MSC has the version as _strdup */
#define strdup           _strdup
#else
#define FMT_SIZE_T       "zu"
#define FMT_SSIZE_T      "zd"
#define FMT_PTRDIFF_T    "zd"
#endif

#define JXS_TAG          "jsonXstruct"

#define jxs_log(level, format, ...)                                                \
	do {                                                                           \
		if (level <= jxs_log_level) {                                              \
			if (jxs_log_level >= JXS_LOG_DEBUG) {                                  \
				print_log(level, "[" JXS_TAG ".%s][%s %d]: " format,               \
				          get_log_tag(level), __func__, __LINE__, ## __VA_ARGS__); \
			}                                                                      \
			else {                                                                 \
				print_log(level, "[" JXS_TAG ".%s]: " format,                      \
				          get_log_tag(level), ## __VA_ARGS__);                     \
			}                                                                      \
		}                                                                          \
	} while (0)

#define PRINT_JMITEM(item, format, ...)                        \
	do {                                                       \
		if (jxs_log_level >= JXS_LOG_DEBUG) {                  \
			print_log(JXS_LOG_INFO,                            \
			          "[" JXS_TAG ".struct][%s %d]: "          \
			          " [JMAP:%p][OFFSET:%8" FMT_PTRDIFF_T "]" \
			          "(%7s)%s=<" format ">\n",                \
			          __func__, __LINE__,                      \
			          item, offset,                            \
			          type_to_name((item)->type),              \
			          locator, ## __VA_ARGS__);                \
		}                                                      \
		else {                                                 \
			print_log(JXS_LOG_INFO,                            \
			          "[" JXS_TAG ".struct]: "                 \
			          " (%7s)%s=<" format ">\n",               \
			          type_to_name((item)->type),              \
			          locator, ## __VA_ARGS__);                \
		}                                                      \
	}                                                          \
	while (0)

typedef struct _jmap_head   jmap_head_t;
typedef struct _jmap_item   jmap_item_t;
typedef struct _jmap_item   jmap_list_t;
/**
 * The maximum dimension of the array, which can be manually modified,
 * But remember to recompile after modification.
 */
#define JXS_ARRAY_DEPTH         8

/* 'key' string max length */
#define JXS_KEY_MAXLEN          1024

/* mapper buffer length.
 * Limit stack size and avoid defining too large local variable */
#define MAPPER_BUFFER_LENGTH    (10000 / sizeof(jmap_item_t))

typedef struct jmap_context_t {
	void *start_addr;   /**< struct's start addr */
	void *opaque;       /**< struct's start addr */
	struct {
		jxs_mapper *arr;
		size_t      idx;
	}     buf;       /**< stack buffer */
	struct {
		jmap_head_t *jmhead;
		jmap_item_t *jmitem;
		size_t       idx;
		const char  *locator;
		const char  *fzlocator;
		void        *vptr;
	}     now;        /**< current context */
	struct {
		uint8_t rule; /**< Rule condition */
		void (*callback)(void *);
	}     convert;
} jmap_context_t;

/**
 * 'Json x Struct' Mapping Table
 */
struct _jmap_head {
	bool   isbuf;          /**< mapper is in the local buffer */
	size_t limit;          /**< jmap item limit */
	size_t idx;            /**< jmap item counter */
	size_t ref;            /**< jmapper reference count */
	void  *start_addr;     /**< current struct's address */
};

struct _jmap_item {
	jxs_type    type;                       /**< jmap type */
	const char *key;                        /**< jmap key(usually the same as a struct member name) */
	ptrdiff_t   offset;                     /**< jmap value(struct member address) */
	size_t      size;                       /**< struct member total size */
	jxs_mapper *subjm;                      /**< sub-struct's jmaplist */
	jxs_type    basetype;                   /**< basic type */
	struct {
		size_t length;                      /**< Length(number) of array elements */
		size_t deptab[JXS_ARRAY_DEPTH + 1]; /**< Array's Dimension record table */
		size_t depth;                       /**< Array's Dimension */
		size_t cur_depth;
	}           arr;                        /**< Array's attribute */
	uint8_t     rule;
};

union jxs_mapper {
	jmap_head_t jmhead;
	jmap_list_t jmlist;
};

typedef enum item_action {
	RULE_ITEM_ERROR = -1, /**< rule handling error */
	RULE_ITEM_KEEP,       /**< keep raw data */
	RULE_ITEM_DELETE,     /**< delete current item */
	RULE_ITEM_SET         /**< set own data */
} item_action;

#define SET_NEW_LOCATOR(new_locator, locator, key, isarray, idx)         \
	char _new_locator[JXS_KEY_MAXLEN] = { 0 };                           \
	do {                                                                 \
		if (isarray) {                                                   \
			snprintf(_new_locator, sizeof(_new_locator),                 \
			         "%s[%" FMT_SIZE_T "]",                              \
			         (locator ? locator : key), (size_t)idx);            \
		}                                                                \
		else {                                                           \
			snprintf(_new_locator, sizeof(_new_locator), "%s%s%s",       \
			         (locator ? locator : ""),                           \
			         ((locator && locator[0] != '\0') ? "." : ""), key); \
		}                                                                \
		new_locator = _new_locator;                                      \
	} while (0)

#define SET_NEW_FUZZY_LOCATOR(new_fzlocator, fzlocator, key, isarray)      \
	char _new_fzlocator[JXS_KEY_MAXLEN] = { 0 };                           \
	do {                                                                   \
		if (isarray) {                                                     \
			snprintf(_new_fzlocator, sizeof(_new_fzlocator),               \
			         "%s[x]", (fzlocator ? fzlocator : key));              \
		}                                                                  \
		else {                                                             \
			snprintf(_new_fzlocator, sizeof(_new_fzlocator), "%s%s%s",     \
			         (fzlocator ? fzlocator : ""),                         \
			         ((fzlocator && locator[0] != '\0') ? "." : ""), key); \
		}                                                                  \
		new_fzlocator = _new_fzlocator;                                    \
	} while (0)

/* Determine the type based on the data size */
#define TYPEOF(size, type)    (size == sizeof(type))

/* Internal function declaration */
static void jxs_log_default_callback(int level, const char *fmt, va_list vl);
static jmap_list_t *get_jmlist(jxs_mapper *mapper);
static jmap_head_t *get_jmhead(jxs_mapper *mapper);
static int jmap_to_json_array(jmap_context_t *ctx, jmap_head_t *jmhead, jmap_item_t *jmitem, json_object *arrjso);
static int jmap_to_json_object(jmap_context_t *ctx, jxs_mapper *mapper, json_object *jso);
static int jmap_from_json_array(jmap_context_t *ctx, jmap_head_t *jmhead, jmap_item_t *jmitem, json_object *arrjso);
static int jmap_from_json_object(jmap_context_t *ctx, jxs_mapper *mapper, json_object *jso);
static void jmap_array_print(jmap_context_t *ctx, jmap_head_t *jmhead, jmap_item_t *jmitem, const char *locator);
static void jmap_struct_print(jmap_context_t *ctx, jxs_mapper *mapper, const char *locator);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* JSONXSTRUCT_PRIV_H */

