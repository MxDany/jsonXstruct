#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "jsonXstruct.h"

typedef struct bd_listall_t {
	int  cursor;
	char errmsg[128];
	int  _errno;
	int  has_more;
	struct {
		int     category;
		int64_t fs_id;
		bool    isdir;
		char    md5[64];
		char    path[1024];
		int64_t server_ctime;
		char    server_filename[1024];
		int64_t server_mtime;
		int64_t size;
		struct {
			char icon[1024];
			char url1[1024];
			char url2[1024];
			char url3[1024];
		}       thumbs[2];
	}    list[10];
	char request_id[128];
} bd_listall_t;

static void convert_callback(void *context)
{
	void *vptr = NULL;
	if ((vptr = jxs_cvt_get_item_fuzzy(context, "list[x].thumbs[x].url1")) != NULL) {
		printf("%s ---> %p\n", jxs_cvt_get_locator(context), vptr);
		const char *url1 = (char *)vptr;
		if (url1[0] == '\0') {
			jxs_cvt_set_item_rule(context, JXS_RULE_KEEP_RAW);
		}
	}
	if ((vptr = jxs_cvt_get_item(context, "list[1].thumbs[1].url3")) != NULL) {
		char *url3 = (char *)vptr;
		strncpy(url3, "https://translate.google.cn/", 1024 - 1);
		printf("Modify %s to %s\n", jxs_cvt_get_locator(context), url3);
	}
}

static jxs_mapper *struct_descriptor(void *context)
{
	bd_listall_t *listall    = NULL;
	jxs_item     *item       = NULL;
	jxs_mapper   *mapper     = NULL;
	jxs_mapper   *jmp_list   = NULL;
	jxs_mapper   *jmp_thumbs = NULL;
	jxs_anon_map_new(context, mapper, 6);
	jxs_anon_map_new(context, jmp_list, 10);
	jxs_anon_map_new(context, jmp_thumbs, 4);
	jxs_set_convert_callback(context, convert_callback);

	jxs_anon_item_add(mapper, listall, int, cursor, NULL);
	jxs_anon_item_add(mapper, listall, string, errmsg, NULL);
	item = jxs_anon_item_add(mapper, listall, int, _errno, NULL);
	jxs_item_set_constkey(item, "errno");
	jxs_anon_item_add(mapper, listall, int, has_more, NULL);
	jxs_anon_item_add(mapper, listall, struct, list, jmp_list, JXS_NELEM(listall->list));
	jxs_anon_item_add(mapper, listall, string, request_id, NULL);

	jxs_anon_item_add(jmp_list, listall->list, int, category, NULL);
	jxs_anon_item_add(jmp_list, listall->list, int, fs_id, NULL);
	jxs_anon_item_add(jmp_list, listall->list, int, isdir, NULL);
	jxs_anon_item_add(jmp_list, listall->list, string, md5, NULL);
	jxs_anon_item_add(jmp_list, listall->list, string, path, NULL);
	jxs_anon_item_add(jmp_list, listall->list, int, server_ctime, NULL);
	jxs_anon_item_add(jmp_list, listall->list, string, server_filename, NULL);
	jxs_anon_item_add(jmp_list, listall->list, int, server_mtime, NULL);
	jxs_anon_item_add(jmp_list, listall->list, int, size, NULL);
	jxs_anon_item_add(jmp_list, listall->list, struct, thumbs, jmp_thumbs, JXS_NELEM(listall->list->thumbs));
	item = jxs_anon_item_add(jmp_thumbs, listall->list->thumbs, string, icon, NULL);
	jxs_item_set_rule(item, JXS_RULE_SET_NULL);
	item = jxs_anon_item_add(jmp_thumbs, listall->list->thumbs, string, url1, NULL);
	jxs_item_set_rule(item, JXS_RULE_SET_NULL);
	item = jxs_anon_item_add(jmp_thumbs, listall->list->thumbs, string, url2, NULL);
	jxs_item_set_rule(item, JXS_RULE_SET_NULL);
	item = jxs_anon_item_add(jmp_thumbs, listall->list->thumbs, string, url3, NULL);
	jxs_item_set_rule(item, JXS_RULE_SET_NULL);
	return mapper;
}

int main(int argc, char *argv[])
{
	char input[1024]  = { 0 };
	char output[1024] = { 0 };
	if (argc) {
		char testdir[512] = { 0 };
		strncpy(testdir, argv[0], sizeof(testdir) - 1);
		char *s = strrchr(testdir, '/');
		if (s) {
			s[0] = '\0';
		}
		const char *testname = "dynamic_modify";
		snprintf(input, sizeof(input), "%s/json/%s.json", testdir, testname);
		snprintf(output, sizeof(output), "%s/%s_out.json", testdir, testname);
	}
	jxs_set_loglevel(JXS_LOG_TRACE);
	bd_listall_t cfg;
	memset(&cfg, 0, sizeof(bd_listall_t));
	snprintf(cfg.list[0].thumbs[0].icon, 1024, "Hello");
	jxs_struct_from_file(struct_descriptor, &cfg, NULL, input);
	// jxs_print_struct(struct_descriptor, &cfg, NULL);
	for (int ii = 0; ii < 10; ii++) {
		for (int jj = 0; jj < 2; jj++) {
			printf("> list[%d].thumbs[%d].url1 ---> %p\n", ii, jj, cfg.list[ii].thumbs[jj].url1);
		}
	}
	jxs_struct_to_file_ext(struct_descriptor, &cfg, NULL, output,
	                       JSON_C_TO_STRING_PRETTY |
	                       JSON_C_TO_STRING_PRETTY_TAB |
	                       JSON_C_TO_STRING_NOSLASHESCAPE);
	return 0;
}

