#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "jsonXstruct.h"

// sub struct
struct thumbs {
	char icon[1024];
	char url1[1024];
	char url2[1024];
	char url3[1024];
};

// top struct
struct basic {
	int           vari;
	int64_t       vari64;
	bool          varb;
	double        vard;
	char          path[1024];
	int           matrix[2][2][3];
	struct thumbs ta;
	struct thumbs tb[2];
};

// Structure descriptor, used to describe the composition of the structure.
static jxs_mapper *struct_descriptor(void *context)
{
	// structure mapper
	jxs_mapper *mapper     = NULL; // struct basic
	jxs_mapper *map_thumbs = NULL; // struct thumbs
	// Create structure mapper
	jxs_map_new(context, struct basic, mapper, 8);
	jxs_map_new(context, struct thumbs, map_thumbs, 4);
	// Describe 'struct basic' member types
	jxs_item_add(mapper, int, vari, NULL);
	jxs_item_add(mapper, int, vari64, NULL);
	jxs_item_add(mapper, boolean, varb, NULL);
	jxs_item_add(mapper, double, vard, NULL);
	jxs_item_add(mapper, string, path, NULL);
	jxs_item_add(mapper, int, matrix, NULL, 2, 2, 3);
	jxs_item_add(mapper, struct, ta, map_thumbs);    // Describe sub struct thumbs ta;
	jxs_item_add(mapper, struct, tb, map_thumbs, 2); // Describe sub struct array thumbs tb[2];

	// Describe 'struct thumbs' member types
	jxs_item_add(map_thumbs, string, icon, NULL);
	jxs_item_add(map_thumbs, string, url1, NULL);
	jxs_item_add(map_thumbs, string, url2, NULL);
	jxs_item_add(map_thumbs, string, url3, NULL);
	return mapper;
}

int main(void)
{
	struct basic bst;
	memset(&bst, 0, sizeof(struct basic));
	// read json data from the file and write it to struct
	jxs_struct_from_file(struct_descriptor, &bst, NULL, "./example/json/basic.json");
	// Change the value of some variables in the structure
	bst.vari = 100;
	// save the struct data into json
	jxs_struct_to_file_ext(struct_descriptor, &bst, NULL, "./basic_out.json",
	                       JSON_C_TO_STRING_PRETTY |
	                       JSON_C_TO_STRING_PRETTY_TAB |
	                       JSON_C_TO_STRING_NOSLASHESCAPE);
	return 0;
}

