#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "jsonXstruct.h"
struct thumbs {
	char icon[1024];
	char url1[1024];
	char url2[1024];
	char url3[1024];
};
struct task {
	int           category;
	int64_t       fs_id;
	bool          isdir;
	char          md5[64];
	char          path[1024];
	int64_t       server_ctime;
	char          server_filename[1024];
	int64_t       server_mtime;
	int64_t       size;
	struct thumbs tb[2];
	char          cube[2][2][2][2][1024];
};
typedef struct task_all_t {
	struct task up;
	struct task down;
	struct task bt;
	struct task cdn;
} task_all_t;

static jxs_mapper *struct_descriptor(void *context)
{
	jxs_mapper *mapper     = NULL;
	jxs_mapper *map_task   = NULL;
	jxs_mapper *map_thumbs = NULL;
	jxs_map_new(context, task_all_t, mapper, 4);
	jxs_map_new(context, struct task, map_task, 11);
	jxs_map_new(context, struct thumbs, map_thumbs, 4);

	jxs_item_add(mapper, struct, up, map_task);
	jxs_item_add(mapper, struct, down, map_task);
	jxs_item_add(mapper, struct, bt, map_task);
	jxs_item_add(mapper, struct, cdn, map_task);

	jxs_item_add(map_task, int, category, NULL);
	jxs_item_add(map_task, int, fs_id, NULL);
	jxs_item_add(map_task, boolean, isdir, NULL);
	jxs_item_add(map_task, string, md5, NULL);
	jxs_item_add(map_task, string, path, NULL);
	jxs_item_add(map_task, int, server_ctime, NULL);
	jxs_item_add(map_task, string, server_filename, NULL);
	jxs_item_add(map_task, int, server_mtime, NULL);
	jxs_item_add(map_task, int, size, NULL);
	jxs_item_add(map_task, struct, tb, map_thumbs, JXS_NELEM(((struct task *)0)->tb));
	// or you can set directly
	// jxs_item_add(map_task, tb, struct, map_thumbs, 2);
	jxs_item_add(map_task, string, cube, NULL, 2, 2, 2, 2);

	jxs_item_add(map_thumbs, string, icon, NULL);
	jxs_item_add(map_thumbs, string, url1, NULL);
	jxs_item_add(map_thumbs, string, url2, NULL);
	jxs_item_add(map_thumbs, string, url3, NULL);
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
		const char *testname = "struct_reuse";
		snprintf(input, sizeof(input), "%s/json/%s.json", testdir, testname);
		snprintf(output, sizeof(output), "%s/%s_out.json", testdir, testname);
	}
	jxs_set_loglevel(JXS_LOG_TRACE);
	task_all_t all;
	memset(&all, 0, sizeof(task_all_t));
	jxs_struct_from_file(struct_descriptor, &all, NULL, input);
	jxs_print_struct(struct_descriptor, &all, NULL);
	jxs_struct_to_file_ext(struct_descriptor, &all, NULL, output,
	                       JSON_C_TO_STRING_PRETTY |
	                       JSON_C_TO_STRING_PRETTY_TAB |
	                       JSON_C_TO_STRING_NOSLASHESCAPE);
	return 0;
}

