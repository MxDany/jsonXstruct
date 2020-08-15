# jsonXstruct

jsonXstruct is a library for converting between json and C-struct. Describe the struct with a series of human-readable APIs, and you can quickly convert between JSON and struct.

## Feature

Support almost any form or element of c struct.

- anonymous struct.
- nested struct
- multi-dimensional arrays
- string
- int8/int16/int32/int64
- float/double
- bool
- char
- json-c object

## Limitation

Struct members must be fixed-size, and currently cannot support dynamically typed struct members. For example, the string type must be defined as `char str[len]`, not as `char *str`.

## How to build

### build json-c

First, you need to compile the [json-c](https://github.com/json-c/json-c) library, for example, we downloaded the `json-c-json-c-0.14-20200419.tar.gz` source code, we cloud build it in linux like this(cmake is required):

```shell
tar -xvf json-c-json-c-0.14-20200419.tar.gz
cd json-c-json-c-0.14-20200419
mkdir build && cd build
cmake ../ -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX="/home/lihan/workspace/json-c" -DCMAKE_BUILD_TYPE=release -DENABLE_RDRAND=ON
cmake --build .
cmake --install . --strip
```

It would install the json-c in the path '/home/lihan/workspace/json-c'.

### build jsonXstruct

After you build json-c library，you could start to build `jsonXstruct`.

Since the current cross-platform compilation script is not fully completed yet. We recommend that you create your own library compilation project according to your own system. Because there are only 3 files(`jsonXstruct.c` `jsonXstruct.h` `jsonXstruct_priv.h`), you can compile them very easily.

We will demonstrate how to compile the linux `jsonXstruct` library below:

- step 1: put the json-c install file in the project

  ```shell
  .
  ├── Makefile
  ├── README.md
  ├── deps
  │   ├── include
  │   │   └── json-c
  │   │       ├── arraylist.h
  │   │       ├── ...
  │   │       └── printbuf.h
  │   └── lib
  │       ├── libjson-c.a
  ...
  ```

- step 2: run make

  ```shell
  make
  ```

  The lib file will be generated in the current directory, and example program will generated in the `example` directory.

  ```
  .
  ├── Makefile
  ├── README.md
  ├── ...
  ├── example
  │   ├── anonymous_struct
  │   ├── ...
  ├── ...
  ├── libjsonXstruct.a
  ├── libjsonXstruct.so -> libjsonXstruct.so.0
  ├── libjsonXstruct.so.0 -> libjsonXstruct.so.0.0.1
  └── libjsonXstruct.so.0.0.1
  ```

  `jsonXstruct.h` is the external interface header file.

## Typical usage

This program reads JSON data from the `./example/json/basic.json` file, and stores it in the struct, then modifies part of the data in the struct, and then rewrites it to the JSON file. It shows the conversion between `struct` and `JSON`.

This example is a simple application of the library. As you can see, it supports a lot of structure element types.

```c
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
```

- Build this example:

  ```shell
  gcc -static -I./ -I./deps/include/json-c -L./ -L./deps/lib ./example/basic.c -ljsonXstruct -ljson-c -lm -o basic
  ```

- Input JSON file `./example/json/basic.json`

  ```json
  {"vari":9978,"vari64":123456789198,"varb":true,"vard":21.34,"path":"\/deps\/include\/json-c","matrix":[[[1,2,3],[4,5,6]],[[7,8,9],[10,11,12]]],"ta":{"icon":"qqqqq","url1":"wwwww","url2":"eeeee","url3":"rrrrr"},"tb":[{"icon":"ddddd","url1":"fffff","url2":"bbbbb","url3":"xxxxx"},{"icon":"ccccc","url1":"     ","url2":"vvvvv","url3":"hhhhh"}]}
  ```

- Output JSON file `./basic_out.json`

  ```json
  {
  	"vari":100,
  	"vari64":123456789198,
  	"varb":true,
  	"vard":21.34,
  	"path":"/deps/include/json-c",
  	"matrix":[
  		[
  			[
  				1,
  				2,
  				3
  			],
  			[
  				4,
  				5,
  				6
  			]
  		],
  		[
  			[
  				7,
  				8,
  				9
  			],
  			[
  				10,
  				11,
  				12
  			]
  		]
  	],
  	"ta":{
  		"icon":"qqqqq",
  		"url1":"wwwww",
  		"url2":"eeeee",
  		"url3":"rrrrr"
  	},
  	"tb":[
  		{
  			"icon":"ddddd",
  			"url1":"fffff",
  			"url2":"bbbbb",
  			"url3":"xxxxx"
  		},
  		{
  			"icon":"ccccc",
  			"url1":"     ",
  			"url2":"vvvvv",
  			"url3":"hhhhh"
  		}
  	]
  }
  ```

  As you can see, only the value of the `vari` variable is modified. And the output JSON has been pretty formatted.

  **Program behavior description:**

  1. `struct_descriptor()` is a callback function, used to describe the struct.

  2. `jxs_mapper` is struct mapper, each structure corresponds to a mapper. use `jxs_map_new()` to new a mapper, to associated struct and mapper.

  3. `jxs_item_add()` add struct element to the mapper corresponding to the struct. it can be (int/bool/string/struct/etc.).

     if the element is an array, you need to describe the array, such as, `matrix[2][2][3]`，you need pass `2, 2, 3`.

     ```c
     jxs_item_add(mapper, int, matrix, NULL, 2, 2, 3);
     ```

     if the element  is a struct, you need pass the sub-struct's mapper `map_thumbs`, otherwise, you should pass `NULL`.

     ```c
     jxs_item_add(mapper, struct, ta, map_thumbs);    // Describe sub struct thumbs ta;
     jxs_item_add(mapper, struct, tb, map_thumbs, 2); // Describe sub struct array thumbs tb[2];
     
     jxs_item_add(map_thumbs, string, icon, NULL); // normal element type, should pass 'NULL'.
     ```

  4. In the `main()` function. Pass `struct_descriptor()` to `jxs_struct_x_file()`, Indicates that the `bst` struct is resolved according to this `struct_descriptor()`. `JSON_C_TO_STRING_PRETTY` to pretty output json data to the file. it support by json-c.

**For more API usage, you can refer to the example and `jsonXstruct.h` function use description.**