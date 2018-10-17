//
//  json_tools.h
//  otdb
//
//  Created by SolPad on 8/10/18.
//  Copyright © 2018 JP Norair. All rights reserved.
//

#ifndef json_tools_h
#define json_tools_h


#include <stdint.h>
#include <stdbool.h>

#include <cJSON.h>


typedef enum {
    CONTENT_hex     = 0,
    CONTENT_array   = 1,
    CONTENT_struct  = 2,
    CONTENT_MAX
} content_type_enum;

typedef enum {
    METHOD_binary   = 0,
    METHOD_string   = 1,
    METHOD_hexstring= 2,
    METHOD_MAX
} readmethod_enum;

typedef enum {
    TYPE_bitmask    = 0,
    TYPE_bit1       = 1,
    TYPE_bit2       = 2,
    TYPE_bit3       = 3,
    TYPE_bit4       = 4,
    TYPE_bit5       = 5,
    TYPE_bit6       = 6,
    TYPE_bit7       = 7,
    TYPE_bit8       = 8,
    TYPE_string     = 9,
    TYPE_hex        = 10,
    TYPE_int8       = 11,
    TYPE_uint8      = 12,
    TYPE_int16      = 13,
    TYPE_uint16     = 14,
    TYPE_int32      = 15,
    TYPE_uint32     = 16,
    TYPE_int64      = 17,
    TYPE_uint64     = 18,
    TYPE_float      = 19,
    TYPE_double     = 20,
    TYPE_MAX
} typeinfo_enum;

typedef struct {
    typeinfo_enum   index;
    int             bits;
} typeinfo_t;





int jst_extract_int(cJSON* meta, const char* name);

double jst_extract_double(cJSON* meta, const char* name);

const char* jst_extract_string(cJSON* meta, const char* name);

uint8_t jst_extract_id(cJSON* meta);
uint8_t jst_extract_mod(cJSON* meta);


uint8_t jst_blockid(cJSON* block_elem);
uint8_t jst_extract_blockid(cJSON* meta);

uint16_t jst_extract_size(cJSON* meta);

uint32_t jst_extract_time(cJSON* meta);

bool jst_extract_stock(cJSON* meta);

content_type_enum jst_tmpltype(cJSON* type_elem);

content_type_enum jst_extract_type(cJSON* meta);

int jst_extract_pos(cJSON* meta);

int jst_extract_bitpos(cJSON* meta);     

int jst_parse_arraysize(const char* bracketexp);

int jst_typesize(typeinfo_t* spec, const char* type);

int jst_extract_typesize(typeinfo_enum* type, cJSON* meta);

int jst_load_element(uint8_t* dst, int limit, unsigned int bitpos, const char* type, cJSON* value);

cJSON* jst_store_element(cJSON* parent, char* name, void* src, typeinfo_enum type, unsigned long bitpos, int bits);

int jst_aggregate_json(cJSON** tmpl, const char* path, const char* fname);

int jst_writeout(cJSON* json_obj, const char* filepath);



#endif /* json_tools_h */
