#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "jsonXstruct.h"

struct url_set {
	char url1[1024];
	char url2[1024];
	char url3[1024];
};
struct info_set {
	char           name[512];
	int            age;
	char           address[512];
	uint64_t       id;
	struct url_set url[2][3][2][3];
};

struct mdarray_set {
	int             matrix[2][2][3][4][2][3];
	struct info_set info[2][3][2];
} mdarray_set;

typedef struct array_t {
	struct mdarray_set a;
	struct mdarray_set b[2];
	struct mdarray_set c[2][2];
	struct mdarray_set d[2][2][2];
} array_t;

static jxs_mapper *struct_descriptor(void *context)
{
	jxs_mapper *mapper      = NULL;
	jxs_mapper *map_mdarray = NULL;
	jxs_mapper *map_info    = NULL;
	jxs_mapper *map_url     = NULL;
	jxs_map_new(context, array_t, mapper, 4);
	jxs_map_new(context, struct mdarray_set, map_mdarray, 2);
	jxs_map_new(context, struct info_set, map_info, 5);
	jxs_map_new(context, struct url_set, map_url, 3);
	jxs_item_add(mapper, struct, a, map_mdarray);
	jxs_item_add(mapper, struct, b, map_mdarray, 2);
	jxs_item_add(mapper, struct, c, map_mdarray, 2, 2);
	jxs_item_add(mapper, struct, d, map_mdarray, 2, 2, 2);

	jxs_item_add(map_mdarray, int, matrix, NULL, 2, 2, 3, 4, 2, 3);
	jxs_item_add(map_mdarray, struct, info, map_info, 2, 3, 2);

	jxs_item_add(map_info, string, name, NULL);
	jxs_item_add(map_info, int, age, NULL);
	jxs_item_add(map_info, string, address, NULL);
	jxs_item_add(map_info, int, id, NULL);
	jxs_item_add(map_info, struct, url, map_url, 2, 3, 2, 3);

	jxs_item_add(map_url, string, url1, NULL);
	jxs_item_add(map_url, string, url2, NULL);
	jxs_item_add(map_url, string, url3, NULL);
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
		const char *testname = "multi_dimen_array";
		snprintf(input, sizeof(input), "%s/json/%s.json", testdir, testname);
		snprintf(output, sizeof(output), "%s/%s_out.json", testdir, testname);
	}
	jxs_set_loglevel(JXS_LOG_TRACE);
	array_t *st = NULL;
	st = (array_t *)malloc(sizeof(array_t));
	if (st == NULL) {
		fprintf(stderr, "malloc error.\n");
		return -1;
	}
	memset(st, 0, sizeof(array_t));
	jxs_struct_from_file(struct_descriptor, st, NULL, input);
	// jxs_print_struct(struct_descriptor, st, NULL);
	jxs_struct_to_file_ext(struct_descriptor, st, NULL, output,
	                       JSON_C_TO_STRING_PRETTY |
	                       JSON_C_TO_STRING_PRETTY_TAB |
	                       JSON_C_TO_STRING_NOSLASHESCAPE);
	free(st);
	return 0;
}

