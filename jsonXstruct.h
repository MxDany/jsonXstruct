/**
 * @file jsonXstruct.h
 * @brief A library for converting between json and C-Struct, based on json-c, easy to use.
 *
 * @author lihan(1104616563@qq.com)
 * @date 2019-12-11
 *
 */
#ifndef JSONXSTRUCT_H
#define JSONXSTRUCT_H

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdarg.h>
#include "json.h"

/* Cross-platform definition */
#if defined(_MSC_VER) || defined(_WIN32)
#ifdef JSONXSTRUCT_EXPORTS
#define JSONXSTRUCT_API    __declspec(dllexport)
#else
#define JSONXSTRUCT_API    __declspec(dllimport)
#endif
#else //Other
#define JSONXSTRUCT_API
#endif

/* Calculating the number of array elements */
#define JXS_NELEM(a)    (sizeof(a) / sizeof(a[0]))
#define JXS_TYPE(t)     jxs_type_ ## t

/* define print log level */
enum {
	JXS_LOG_QUIET = 0, /**< Print no output */
	JXS_LOG_FATAL,     /**< Something went really wrong and we will crash now */
	JXS_LOG_ERROR,
	JXS_LOG_WARN,
	JXS_LOG_INFO,
	JXS_LOG_DEBUG,
	JXS_LOG_TRACE,
};

/* rule action */
typedef enum jxs_rule {
	JXS_RULE_KEEP_RAW = 1,
	JXS_RULE_SET_NULL,
	JXS_RULE_DROP_SELF,
} jxs_rule;

/* jmap type */
typedef enum jxs_type {
	jxs_type_null,    /**< null type */
	jxs_type_boolean, /**< boolean type, can be 'bool/int' */
	jxs_type_double,  /**< double type, can be 'float/double' */
	jxs_type_int,     /**< Integer type, can be 'int8/int16/int32/int64' */
	// TODO: jxs_type_uint type is not currently supported
	jxs_type_uint,    /**< Unsigned integer type, can be 'uint8/uint16/uint32/uint64' */
	jxs_type_string,  /**< string type, should be 'char [x]' */
	jxs_type_struct,  /**< struct type */
	jxs_type_object,  /**< json_object type */
	jxs_type_array,   /**< array type(Internal type, you should never use it) */
} jxs_type;

/* mapper item, corresponds to a member of the struct. */
typedef struct _jmap_item   jxs_item;

/* struct mapper, corresponds to a struct. */
typedef union jxs_mapper    jxs_mapper;

/**
 * @brief struct descriptor callback function, it will be called before the
 * conversion starts. It must be implemented to describe the struct.
 * @param context   jsonXstruct context.
 * @return struct top-level's mapper.
 */
typedef jxs_mapper * (*jxs_descriptor)(void *context);

/**
 * @brief set jsonXstruct library loglevel. It will take effect globally. Call it
 * before you use all the features.
 * @param  level  loglevel, support: 'off', 'fatal', 'error', 'warn' ,
 * 'info', 'debug', 'trace'.
 */
JSONXSTRUCT_API void jxs_set_loglevel(int level);
JSONXSTRUCT_API int jxs_get_loglevel(void);
JSONXSTRUCT_API void jxs_set_log_callback(void (*callback)(int, const char *, va_list));

/**
 * @brief get opaque userdata.
 * @param  context   jsonXstruct context.
 * @return opaque userdata(void *)
 */
JSONXSTRUCT_API void *jxs_get_userdata(void *context);

/**
 * @brief JSON to struct conversion functions, you can use these functions to
 * dynamically determine the generated JSON.
 * @param context  jsonXstruct context.
 * @param cb       convert callback, this callback's param is jsonXstruct context.
 * @param locator  a string like 'st.a[0][1]' to locate the location of the current
 *                 conversion process.
 * @param fuzzy_locator a string like 'st.a[x][x]' to locate the fuzzy location of
 *                 the current conversion process.
 * @param rule     processing rules of the current item.
 */
JSONXSTRUCT_API void jxs_set_convert_callback(void *context, void (*cb)(void *));
JSONXSTRUCT_API const char *jxs_cvt_get_locator(void *context);
JSONXSTRUCT_API const char *jxs_cvt_get_fuzzy_locator(void *context);
JSONXSTRUCT_API void *jxs_cvt_get_item_each(void *context);
JSONXSTRUCT_API void *jxs_cvt_get_item_fuzzy(void *context, const char *fuzzy_locator);
JSONXSTRUCT_API void *jxs_cvt_get_item(void *context, const char *locator);
JSONXSTRUCT_API void jxs_cvt_set_item_rule(void *context, jxs_rule rule);

/**
 * @brief New a mapper for your struct. If you add item more than 'num', it
 * will be discarded. It will use local stack buffer first. malloc() will be used
 * only if there is no buffer left. This can avoid malloc and free memory frequently
 * and improve the performance of jsonXstruct.
 * You don't need to delete mapper manually, jsonXstruct will handle it internally.
 * @param context  jsonXstruct context.
 * @param num      number of mapper item, Usually equal to the number of struct
 *                 members. Larger value is allowed, but may waste limited stack buffer.
 */
JSONXSTRUCT_API jxs_mapper *jxs_map_basic_new(void *context, size_t num);

/**
 * @brief Add a jmap item into the mapper, if you add item more than 'num', it
 * will be discarded.
 * @param mapper   mapper, must have been initialized with @ref jxs_map_basic_new().
 * @param type     jmap item basic type(never should be jxs_type_array).
 * @param key      key string, must be in constant memory, such as, input "mykey".
 *                 It is acceptable to use key in non-constant memory blocks if
 *                 the caller ensure that the memory holding the key lives longer
 *                 than the corresponding mapper. However, this is somewhat
 *                 dangerous and should only be done if really justified.
 * @param offset   struct member's start address offset.
 * @param mbsize   struct member's size.
 * @param subjm    sub-struct's Mapper, if type=jxs_type_struct, a initialized
 *                 mapper is required. for other type, it should be set to NULL.
 * @param ...      array record table variable parameter list, 'int' is required.
 *                 used to record the construction of a multi-dimen array. it
 *                 always end with 0, to mark parameter end. If there is no other
 *                 parameter except 0, it means it is not an array.
 *                 If array is a[10][5][2], it should input '10, 5, 2, 0' in
 *                 the varlist.
 * @return mapper item, or NULL if an error occurred.
 */
JSONXSTRUCT_API jxs_item *jxs_item_basic_add(jxs_mapper *mapper, jxs_type type,
                                             const char *key,
                                             ptrdiff_t offset, size_t mbsize,
                                             jxs_mapper *subjm, ...);

/**
 * @brief New a mapper for your anonymous struct. If you add item more than 'num', it
 * will be discarded. It will use local stack buffer first. malloc() will be used
 * only if there is no buffer left. This can avoid malloc and free memory frequently
 * and improve the performance of jsonXstruct.
 * You don't need to delete mapper manually, jsonXstruct will handle it internally.
 * @param ctx      jsonXstruct context.
 * @param mapper   mapper output.
 * @param num      number of mapper item, Usually equal to the number of struct
 *                 members. Larger value is allowed, but may waste limited stack buffer.
 */
#define jxs_anon_map_new(ctx, mapper, num) \
	mapper = jxs_map_basic_new(ctx, num)
/**
 * @brief Add a jmap item into the mapper, Use macros to simplify @ref jxs_item_basic_add().
 * read @ref jxs_item_basic_add() comment to learn how to use it.
 * For anonymous struct, the offset cannot be calculated from the struct
 * prototype. use struct definition to calculate it.
 * @param mapper  mapper, must have been initialized with @ref jxs_map_basic_new().
 * @param stptr   struct start address
 * @param stmb    struct member name (Must be exactly the same as json key name).
 * @param type    can be 'boolean'(bool/int), 'double'(double/float),
 *                'int'(int8/int16/int32/int64), 'string'(char [x]),
 *                'object'(json_object), 'struct'(c struct).
 *                It must be the datatype recommended in brackets, otherwise, an
 *                error will occur.
 * @param subjm   sub-struct's Mapper, if type=struct, a initialized mapper is
 *                required. for other type, it should be set to NULL.
 * @param ...     Array depth description, such as, a[a][b][c][d], the variable
 *                argument list be 'a, b, c, d'. and 'int' variable tpye is
 *                required, otherwise, unknown error will occur.
 * @return mapper item, or NULL if an error occurred.
 * @note If type=object, It will get the json_object reference internally by
 *       simple call @ref json_object_get(), So the json_object's reference count
 *       will increase by one. anyway, You need to manage this json_object yourself.
 *       Typically, you should call @ref json_object_put() to decrease its
 *       reference count after you call the conversion function.
 */
#define jxs_anon_item_add(mapper, stptr, type, stmb, subjm, ...)     \
	jxs_item_basic_add(mapper, JXS_TYPE(type), # stmb,               \
	                   (intptr_t)(&(stptr)->stmb) - (intptr_t)stptr, \
	                   sizeof((stptr)->stmb), subjm, ## __VA_ARGS__, 0)

/**
 * @brief New a mapper for your struct. If you add item more than 'num', it
 * will be discarded. It will use local stack buffer first. malloc() will be used
 * only if there is no buffer left. This can avoid malloc and free memory frequently
 * and improve the performance of jsonXstruct.
 * You don't need to delete mapper manually, jsonXstruct will handle it internally.
 * @param ctx      jsonXstruct context.
 * @param sttype   struct prototype.
 * @param mapper   mapper output.
 * @param num      number of mapper item, Usually equal to the number of struct
 *                 members. Larger value is allowed, but may waste limited stack buffer.
 */
#define jxs_map_new(ctx, sttype, mapper, num) \
	typedef sttype __ ## mapper ## _t;        \
	mapper = jxs_map_basic_new(ctx, num)
/**
 * @brief Add a jmap item into the mapper, Use macros to simplify @ref jxs_item_basic_add().
 * read @ref jxs_item_basic_add() comment to learn how to use it.
 * Use struct prototype to calculate offset.
 * @param mapper  mapper, must have been initialized with @ref jxs_map_basic_new().
 * @param stmb    struct member name (Must be exactly the same as json key name).
 * @param type    can be 'boolean'(bool/int), 'double'(double/float),
 *                'int'(int8/int16/int32/int64), 'string'(char [x]),
 *                'object'(json_object), 'struct'(c struct).
 *                It must be the datatype recommended in brackets, otherwise, an
 *                error will occur.
 * @param subjm   sub-struct's Mapper, if type=struct, a initialized mapper is
 *                required. for other type, it should be set to NULL.
 * @param ...     Array depth description, such as, a[a][b][c][d], the variable
 *                argument list be 'a, b, c, d'. and 'int' variable tpye is
 *                required, otherwise, unknown error will occur.
 * @return mapper item, or NULL if an error occurred.
 * @note If type=object, It will get the json_object reference internally by
 *       simple call @ref json_object_get(), So the json_object's reference count
 *       will increase by one. anyway, You need to manage this json_object yourself.
 *       Typically, you should call @ref json_object_put() to decrease its
 *       reference count after you call the conversion function.
 */
#define jxs_item_add(mapper, type, stmb, subjm, ...)            \
	jxs_item_basic_add(mapper, JXS_TYPE(type), # stmb,          \
	                   offsetof(__ ## mapper ## _t, stmb),      \
	                   sizeof(((__ ## mapper ## _t *)0)->stmb), \
	                   subjm, ## __VA_ARGS__, 0)

/**
 * @brief set mapper item rule, In some special scenarios, you can use it to control
 * the rules of json to structure.
 * For example, when you set the rflag bit to 'JXS_RULE_IF_BLANK | JXS_RULE_SET_NULL',
 * it means that if the current data is empty, set the json_object equal to NULL.
 * @param  mid     mapper item id
 * @param  rflag   rule flags
 */
JSONXSTRUCT_API void jxs_item_set_rule(jxs_item *item, jxs_rule rule);

/**
 * @brief Set custom json key, Using the above simplified macro definition method
 * will use your struct member name as the json key by default, you can use this
 * function to modify the default key.
 * @param  mid     mapper item id.
 * @param  key     const key string, must be in constant memory.
 * @note Actually, it is acceptable to use key in non-constant memory blocks if
 * the caller ensure that the memory holding the key lives longer than the
 * corresponding mapper. However, this is somewhat dangerous and should only be
 * done if really justified.
 */
JSONXSTRUCT_API void jxs_item_set_constkey(jxs_item *item, const char *key);

/**
 * @brief convert struct to json_object, you must implement the jxs_descriptor
 * callback function to describe your struct construction. It will new json_object
 * internally.
 * @param func    struct descriptor, it will be called before the conversion
 *                starts. it must be implemented and cannot be null, otherwise,
 *                an error will occur. you must use @ref jxs_map_basic_new() and
 *                @ref jxs_anon_item_add() to describe your own struct construction
 *                inside this callback function, and return top-level's mapper.
 * @param stptr   struct pointer, Require initialized.
 * @param opaque  user opaque data, use @ref jxs_get_userdata() to get it.
 * @return json_object instance if success, or NULL is returned. it need free by
 * yourself, such as calling @ref json_object_put().
 */
JSONXSTRUCT_API json_object *jxs_struct_to_json_object(jxs_descriptor func,
                                                       void *stptr, void *opaque);

/**
 * @brief parse struct from json_object, you must implement the jxs_descriptor
 * callback function to describe your struct construction.
 * @param func    struct descriptor, it will be called before the conversion
 *                starts. it must be implemented and cannot be null, otherwise,
 *                an error will occur. you must use @ref jxs_map_basic_new() and
 *                @ref jxs_anon_item_add() to describe your own struct construction
 *                inside this callback function, and return top-level's mapper.
 * @param stptr   struct pointer, Require initialized.
 * @param opaque  user opaque data, use @ref jxs_get_userdata() to get it.
 * @param jso     json_object, Require initialized.
 * @return 0 for success, -1 for error.
 * @note It won't modify the reference count of this json_object.
 */
JSONXSTRUCT_API int jxs_struct_from_json_object(jxs_descriptor func, void *stptr,
                                                void *opaque, json_object *jso);

/**
 * @brief write struct to file as json format, you must implement the jxs_descriptor
 * callback function to describe your struct construction.
 * @param func     struct descriptor, it will be called before the conversion
 *                 starts. it must be implemented and cannot be null, otherwise,
 *                 an error will occur. you must use @ref jxs_map_basic_new() and
 *                 @ref jxs_anon_item_add() to describe your own struct construction
 *                 inside this callback function, and return top-level's mapper.
 * @param stptr    struct pointer, Require initialized.
 * @param opaque   user opaque data, use @ref jxs_get_userdata() to get it.
 * @param filename json file path.
 * @param flags    formatting options, see JSON_C_TO_STRING_PRETTY and other
 *                 constants.
 * @return 0 for success, -1 for error.
 */
JSONXSTRUCT_API int jxs_struct_to_file_ext(jxs_descriptor func, void *stptr,
                                           void *opaque, const char *filename, int flags);
JSONXSTRUCT_API int jxs_struct_to_file(jxs_descriptor func, void *stptr,
                                       void *opaque, const char *filename);

/**
 * @brief parse struct from json format file, you must implement the jxs_descriptor
 * callback function to describe your struct construction.
 * @param func     struct descriptor, it will be called before the conversion
 *                 starts. it must be implemented and cannot be null, otherwise,
 *                 an error will occur. you must use @ref jxs_map_basic_new() and
 *                 @ref jxs_anon_item_add() to describe your own struct construction
 *                 inside this callback function, and return top-level's mapper.
 * @param stptr    struct pointer, Require initialized.
 * @param opaque   user opaque data, use @ref jxs_get_userdata() to get it.
 * @param filename json file path.
 * @return 0 for success, -1 for error.
 */
JSONXSTRUCT_API int jxs_struct_from_file(jxs_descriptor func,
                                         void *stptr, void *opaque,
                                         const char *filename);

/**
 * @brief convert struct to json string, you must implement the jxs_descriptor
 * callback function to describe your struct construction. It will new json string
 * internally.
 * @param func    struct descriptor, it will be called before the conversion
 *                starts. it must be implemented and cannot be null, otherwise,
 *                an error will occur. you must use @ref jxs_map_basic_new() and
 *                @ref jxs_anon_item_add() to describe your own struct construction
 *                inside this callback function, and return top-level's mapper.
 * @param stptr   struct pointer, Require initialized.
 * @param opaque  user opaque data, use @ref jxs_get_userdata() to get it.
 * @param flags   formatting options, see JSON_C_TO_STRING_PRETTY and other
 *                constants.
 * @return json string instance if success, or NULL is returned. it need free by
 * yourself, call @ref jxs_free_json_string() to free it.
 */
JSONXSTRUCT_API const char *jxs_struct_to_json_string(jxs_descriptor func,
                                                      void *stptr, void *opaque);
JSONXSTRUCT_API const char *jxs_struct_to_json_string_ext(jxs_descriptor func, void *stptr,
                                                          void *opaque, int flags);
JSONXSTRUCT_API void jxs_free_json_string(char *jstring);

/**
 * @brief parse struct from json string, you must implement the jxs_descriptor
 * callback function to describe your struct construction.
 * @param func    struct descriptor, it will be called before the conversion
 *                starts. it must be implemented and cannot be null, otherwise,
 *                an error will occur. you must use @ref jxs_map_basic_new() and
 *                @ref jxs_anon_item_add() to describe your own struct construction
 *                inside this callback function, and return top-level's mapper.
 * @param stptr   struct pointer, Require initialized.
 * @param opaque  user opaque data, use @ref jxs_get_userdata() to get it.
 * @param jstring json string, Require initialized.
 * @return 0 for success, -1 for error.
 */
JSONXSTRUCT_API int jxs_struct_from_json_string(jxs_descriptor func, void *stptr,
                                                void *opaque, const char *jstring);

/**
 * @brief print struct data by mapper as 'INFO' level. It won't be affected by
 * @ref jxs_set_loglevel().
 * @param func    struct descriptor, it will be called before the conversion
 *                starts. it must be implemented and cannot be null, otherwise,
 *                an error will occur. you must use @ref jxs_map_basic_new() and
 *                @ref jxs_anon_item_add() to describe your own struct construction
 *                inside this callback function, and return top-level's mapper.
 * @param stptr   struct pointer, Require initialized.
 * @param opaque  user opaque data, use @ref jxs_get_userdata() to get it.
 */
JSONXSTRUCT_API void jxs_print_struct(jxs_descriptor func, void *stptr, void *opaque);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* JSONXSTRUCT_H */

