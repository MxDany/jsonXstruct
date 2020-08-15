#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "jsonXstruct.h"

typedef struct td_cfg_t {
	struct {
		char     cdn_dir[1024];
		char     dev_sn[512];
		uint32_t cdn_size;
		uint32_t remain_size;
		uint64_t speed_limit;
		char     download_dir[1024];
		char     token_path[1024];
		int      matrix[5][4][3];
	}    tdcfg;
	char tdcfg_path[1024];
	struct {
		int  onflag;
		char data_dir[1024];
		char all_path[20][1024];
		struct {
			char    name[1024];
			double  pi;
			bool    is_new;
			int64_t size;
			int32_t limit;
		}    dev_info[2][2][2];
		struct {
			int64_t code;
			char    err[10];
		}    errmsg[4];
	}    modcfg;
	char modcfg_path[1024];
} td_cfg_t;

static jxs_mapper *struct_descriptor(void *context)
{
	td_cfg_t   *cfg       = NULL;
	jxs_mapper *mapper    = NULL;
	jxs_mapper *jm_tdcfg  = NULL;
	jxs_mapper *jm_modcfg = NULL;
	jxs_mapper *jm_dinfo  = NULL;
	jxs_mapper *jm_errmsg = NULL;
	jxs_anon_map_new(context, mapper, 4);
	jxs_anon_map_new(context, jm_modcfg, 5);
	jxs_anon_map_new(context, jm_errmsg, 2);
	jxs_anon_map_new(context, jm_tdcfg, 8);
	jxs_anon_map_new(context, jm_dinfo, 5);

	jxs_anon_item_add(mapper, cfg, struct, tdcfg, jm_tdcfg);
	jxs_anon_item_add(mapper, cfg, string, tdcfg_path, NULL);
	jxs_anon_item_add(mapper, cfg, struct, modcfg, jm_modcfg);
	jxs_anon_item_add(mapper, cfg, string, modcfg_path, NULL);

	jxs_anon_item_add(jm_tdcfg, &cfg->tdcfg, string, cdn_dir, NULL);
	jxs_anon_item_add(jm_tdcfg, &cfg->tdcfg, string, dev_sn, NULL);
	jxs_anon_item_add(jm_tdcfg, &cfg->tdcfg, int, cdn_size, NULL);
	jxs_anon_item_add(jm_tdcfg, &cfg->tdcfg, int, remain_size, NULL);
	jxs_anon_item_add(jm_tdcfg, &cfg->tdcfg, int, speed_limit, NULL);
	jxs_anon_item_add(jm_tdcfg, &cfg->tdcfg, string, download_dir, NULL);
	jxs_anon_item_add(jm_tdcfg, &cfg->tdcfg, string, token_path, NULL);
	jxs_anon_item_add(jm_tdcfg, &cfg->tdcfg, int, matrix, NULL, 5, 4, 3);

	jxs_anon_item_add(jm_modcfg, &cfg->modcfg, int, onflag, NULL);
	jxs_anon_item_add(jm_modcfg, &cfg->modcfg, string, data_dir, NULL);
	jxs_anon_item_add(jm_modcfg, &cfg->modcfg, string, all_path, NULL, JXS_NELEM(cfg->modcfg.all_path));
	jxs_anon_item_add(jm_modcfg, &cfg->modcfg, struct, dev_info, jm_dinfo, 2, 2, 2);
	jxs_anon_item_add(jm_modcfg, &cfg->modcfg, struct, errmsg, jm_errmsg, JXS_NELEM(cfg->modcfg.errmsg));

	jxs_anon_item_add(jm_dinfo, cfg->modcfg.dev_info[0][0], string, name, NULL);
	jxs_anon_item_add(jm_dinfo, cfg->modcfg.dev_info[0][0], double, pi, NULL);
	jxs_anon_item_add(jm_dinfo, cfg->modcfg.dev_info[0][0], boolean, is_new, NULL);
	jxs_anon_item_add(jm_dinfo, cfg->modcfg.dev_info[0][0], int, size, NULL);
	jxs_anon_item_add(jm_dinfo, cfg->modcfg.dev_info[0][0], int, limit, NULL);

	jxs_anon_item_add(jm_errmsg, cfg->modcfg.errmsg, int, code, NULL);
	jxs_anon_item_add(jm_errmsg, cfg->modcfg.errmsg, string, err, NULL);
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
		const char *testname = "anonymous_struct";
		snprintf(input, sizeof(input), "%s/json/%s.json", testdir, testname);
		snprintf(output, sizeof(output), "%s/%s_out.json", testdir, testname);
	}
	jxs_set_loglevel(JXS_LOG_TRACE);
	td_cfg_t cfg;
	memset(&cfg, 0, sizeof(td_cfg_t));
	jxs_struct_from_file(struct_descriptor, &cfg, NULL, input);
	jxs_print_struct(struct_descriptor, &cfg, NULL);
	jxs_struct_to_file_ext(struct_descriptor, &cfg, NULL, output,
	                       JSON_C_TO_STRING_PRETTY |
	                       JSON_C_TO_STRING_PRETTY_TAB |
	                       JSON_C_TO_STRING_NOSLASHESCAPE);
	return 0;
}

