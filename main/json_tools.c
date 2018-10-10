//
//  json_tools.c
//  otdb
//
//  Created by SolPad on 8/10/18.
//  Copyright © 2018 JP Norair. All rights reserved.
//

#include "json_tools.h"
#include "cmds.h"
#include "otdb_cfg.h"

#include <otfs.h>
#include <cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>


#if OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif




int jst_extract_int(cJSON* meta, const char* name) {
    int value = 0;
    cJSON* elem;
    if (meta != NULL) {
        elem = cJSON_GetObjectItemCaseSensitive(meta, name);
        if (elem != NULL) {
            if (cJSON_IsNumber(elem)) {
                value = elem->valueint;
            }
        }
    }
    return value;
}

double jst_extract_double(cJSON* meta, const char* name) {
    double value = 0;
    cJSON* elem;
    if (meta != NULL) {
        elem = cJSON_GetObjectItemCaseSensitive(meta, name);
        if (elem != NULL) {
            if (cJSON_IsNumber(elem)) {
                value = elem->valuedouble;
            }
        }
    }
    return value;
}

const char* jst_extract_string(cJSON* meta, const char* name) {
    const char* value = NULL;
    cJSON* elem;
    if (meta != NULL) {
        elem = cJSON_GetObjectItemCaseSensitive(meta, name);
        if (elem != NULL) {
            if (cJSON_IsString(elem)) {
                value = elem->valuestring;
            }
        }
    }
    return value;
}


uint8_t jst_extract_id(cJSON* meta) {
    return (uint8_t)(255 & jst_extract_int(meta, "id"));
}


uint8_t jst_extract_mod(cJSON* meta) {
    return (uint8_t)(255 & jst_extract_int(meta, "mod"));
}


uint8_t jst_blockid(cJSON* block_elem) {
    char* elemstr;
    elemstr = (block_elem != NULL) ? block_elem->valuestring : NULL;

    if (elemstr != NULL) {
        if (strcmp(elemstr, "gfb") == 0) {
            return 1;
        }
        if (strcmp(elemstr, "iss") == 0) {
            return 2;
        }
    }
    return 3;   // isf --> default
}
uint8_t jst_extract_blockid(cJSON* meta) {
    cJSON* block_elem;
    block_elem = (meta != NULL) ? cJSON_GetObjectItemCaseSensitive(meta, "block") : NULL;
    return jst_blockid(block_elem);
}


uint16_t jst_extract_size(cJSON* meta) {
    return (uint16_t)(65535 & jst_extract_int(meta, "size"));
}


uint16_t jst_extract_time(cJSON* meta) {
    return (uint32_t)(jst_extract_int(meta, "time"));
}


bool jst_extract_stock(cJSON* meta) {
    return (bool)(jst_extract_int(meta, "stock") != 0);
}


content_type_enum jst_tmpltype(cJSON* type_elem) {
    char* elemstr;
    elemstr = (type_elem != NULL) ? type_elem->valuestring : NULL;

    if (elemstr != NULL) {
        if (strcmp(elemstr, "struct") == 0) {
            return CONTENT_struct;
        }
        if (strcmp(elemstr, "array") == 0) {
            return CONTENT_array;
        }
    }
    return CONTENT_hex;       // hex string --> default
}
content_type_enum jst_extract_type(cJSON* meta) {
    cJSON* type_elem;
    type_elem = (meta != NULL) ? cJSON_GetObjectItemCaseSensitive(meta, "type") : NULL;
    return jst_tmpltype(type_elem);
}


unsigned int jst_extract_pos(cJSON* meta) {
    return (unsigned int)(jst_extract_int(meta, "pos") != 0);
}


unsigned long jst_extract_bitpos(cJSON* meta) {
    unsigned long value = 0;
    cJSON* elem;
    if (meta != NULL) {
        elem = cJSON_GetObjectItemCaseSensitive(meta, "pos");
        if (elem != NULL) {
            if (cJSON_IsNumber(elem)) {
                value = lround( ((elem->valuedouble - floor(elem->valuedouble)) * 100) );
            }
        }
    }
    return value;
}


            


unsigned long jst_parse_arraysize(const char* bracketexp) {

    if (bracketexp == NULL) {
        return 0;
    }
    
    bracketexp = strchr(bracketexp, '[');
    if (bracketexp == NULL) {
        return 0;
    }
    
    bracketexp++;
    if (strchr(bracketexp, ']') == NULL) {
        return 0;
    }
    
    return strtoul(bracketexp, NULL, 10);
}





int jst_typesize(typeinfo_t* spec, const char* type) {
/// Returns 0 if type is recognized.
/// Loads 'spec' param with extracted type information.
    int offset = 0;
    const char* cursor = &type[1];
    
    spec->index = TYPE_MAX;
    
    switch (type[0]) {
        // bitmask, bitX_t, bool
        case 'b': {
            if (strcmp(cursor, "itmask") == 0) {
                spec->index = TYPE_bitmask;
                spec->bits  = 0;
            }
            else if (strcmp(cursor, "ool") == 0) {
                spec->index = TYPE_bit1;
                spec->bits  = 1; 
            }
            else if ((strncmp(cursor, "it", 2) == 0) && (strcmp(&cursor[3], "_t") == 0)) {
                spec->bits = cursor[2] - '0';
                if (spec->bits > 8) {
                    spec->bits = 8;
                }
                spec->index = (typeinfo_enum)spec->bits;
            }
        } break;
        
        // char
        case 'c': {
            if ((strncmp(cursor, "har", 3) == 0)) {
                if (cursor[3] == '[') {
                    spec->index = TYPE_string;
                    spec->bits  = (int)(8 * jst_parse_arraysize(&cursor[3]));
                }
                else {
                    spec->index = TYPE_int8;
                    spec->bits  = 8;
                }
            }
        } break;
        
        // double
        case 'd': {
            if ((strcmp(cursor, "ouble") == 0)) {
                spec->index = TYPE_double;
                spec->bits  = 64;
            }
        } break;
        
        // hex
        case 'h': {
            if (strncmp(cursor, "ex", 2) == 0) {
                if (cursor[3] == '[') {
                    spec->index = TYPE_hex;
                    spec->bits  = (int)(8 * jst_parse_arraysize(&cursor[2]));
                }
                else {
                    spec->index = TYPE_hex;
                    spec->bits  = 8;
                }
            }
        } break;
        
        // float
        case 'f': {
            if ((strcmp(cursor, "loat") == 0)) {
                spec->index = TYPE_float;
                spec->bits  = 32;
            }
        } break;
        
        // integers
        case 'i': cursor = &type[0];
                  offset = -1;
        case 'u': {
            if (strncmp(cursor, "int", 3)) {
                if ((cursor[3]==0) || (strcmp(&cursor[3], "32_t")==0)) {
                    spec->index = TYPE_uint32 + offset;
                    spec->bits  = 32;
                }
                else if ((strcmp(cursor, "16_t") == 0)) {
                    spec->index = TYPE_uint16 + offset;
                    spec->bits  = 16;
                }
                else if ((strcmp(cursor, "8_t") == 0)) {
                    spec->index = TYPE_uint8 + offset;
                    spec->bits  = 8;
                }
                else if ((strcmp(cursor, "64_t") == 0)) {
                    spec->index = TYPE_uint64 + offset;
                    spec->bits  = 64;
                }
            }
        } break;
        
        // long
        case 'l': {
            if (strcmp(cursor, "ong") == 0) {
                spec->index = TYPE_int32;
                spec->bits  = 32;
            }
        } break;
        
        // short
        case 's': {
            if (strcmp(cursor, "hort") == 0) {
                spec->index = TYPE_int16;
                spec->bits  = 16;
            }
        } break;
        
        default: break;
    }
    
    return (spec->index < TYPE_MAX) - 1;
}



int jst_extract_typesize(typeinfo_enum* type, cJSON* meta) {
    cJSON* elem;
    typeinfo_t spec;
    
    // Defaults
    spec.bits   = 0;
    spec.index  = TYPE_MAX;
    
    if (meta != NULL) {
        elem = cJSON_GetObjectItemCaseSensitive(meta, "type");
        if (cJSON_IsString(elem)) {
            jst_typesize(&spec, elem->valuestring);
        }
    }
    
    if (type != NULL) {
        *type = spec.index;
    }
    
    return spec.bits;
}




int jst_load_element(uint8_t* dst, size_t limit, unsigned int bitpos, const char* type, cJSON* value) {
    typeinfo_t typeinfo;
    int bytesout;
    
    if ((dst==NULL) || (limit==0) || (type==NULL) || (value==NULL)) {
        return 0;
    }
    
    // If jst_typesize returns nonzero, then type is invalid.  Write 0 bytes.
    if (jst_typesize(&typeinfo, type) != 0) {
        return 0;
    }
    
    bytesout = 0;
    switch (typeinfo.index) {
        // Bitmask type is a container that holds non-byte contents
        // It returns a negative number of its size in bytes
        case TYPE_bitmask: {
            return -(typeinfo.bits/8);
        }
        
        // Bit types require a mask and set operation
        // They return 0
        case TYPE_bit1:
        case TYPE_bit2:
        case TYPE_bit3:
        case TYPE_bit4:
        case TYPE_bit5:
        case TYPE_bit6:
        case TYPE_bit7:
        case TYPE_bit8: {
            ot_uni32 scr;
            unsigned long dat       = 0;
            unsigned long maskbits  = typeinfo.bits;
            
            if (cJSON_IsNumber(value)) {
                dat = value->valueint;
            }
            else if (cJSON_IsString(value)) {
                cmd_hexnread((uint8_t*)&dat, value->valuestring, sizeof(unsigned long));
            }
            if ((1+((bitpos+maskbits)/8)) > limit) {
                goto jst_load_element_END;
            }
            
            ///@todo this may be endian dependent
            memcpy(&scr.ubyte[0], dst, 4);
            maskbits    = ((1<<maskbits) - 1) << bitpos;
            dat       <<= bitpos;
            scr.ulong  &= ~maskbits; 
            scr.ulong  |= (dat & maskbits);
            memcpy(dst, &scr.ubyte[0], 4);
            bytesout = 0;
        } break;
    
        // String and hex types have length determined by value test
        case TYPE_string: {
            if (cJSON_IsString(value)) {
                bytesout = typeinfo.bits/8;
                if (bytesout <= limit) {
                    memcpy(dst, (char*)value, bytesout);
                }
            }
        } break;
        
        case TYPE_hex: {
            if (cJSON_IsString(value)) {
                bytesout = typeinfo.bits/8;
                if (bytesout <= limit) {
                    cmd_hexnread(dst, (char*)value, bytesout);
                }
            }
        } break;
    
        // Fixed length data types
        case TYPE_int8:
        case TYPE_uint8:
        case TYPE_int16:
        case TYPE_uint16:
        case TYPE_int32:
        case TYPE_uint32:
        case TYPE_int64:
        case TYPE_uint64: 
        case TYPE_float:
        case TYPE_double: {
            bytesout = typeinfo.bits/8;
            if (bytesout <= limit) {
                if (cJSON_IsString(value)) {
                    memset(dst, 0, bytesout);
                    cmd_hexread(dst, (char*)value);
                }
                else if (cJSON_IsNumber(value)) {
                    if (typeinfo.index == TYPE_float) {
                        float tmp = (float)value->valuedouble;
                        memcpy(dst, &tmp, bytesout);
                    }
                    else if (typeinfo.index == TYPE_double) {
                        memcpy(dst, &value->valuedouble, bytesout);
                    }
                    else {
                        memcpy(dst, &value->valueint, bytesout);
                    }
                }
            } 
        } break;
    
        default: break;
    }
    
    jst_load_element_END:
    return bytesout;
}



int jst_aggregate_json(cJSON** tmpl, const char* fname) {
    FILE*       fp;
    uint8_t*    fbuf;
    cJSON*      local;
    cJSON*      obj;
    long        flen;
    int         rc = 0;
    
    if ((tmpl == NULL) || (fname == NULL)) {
        return -1;
    }
    
    fp = fopen(fname, "r");
    if (fp == NULL) {
        return -2;
    }

    fseek(fp, 0L, SEEK_END);
    flen = ftell(fp);
    rewind(fp);
    fbuf = calloc(1, flen+1);
    if (fbuf == NULL) {
        fclose(fp);
        rc = -3;
        goto jst_aggregate_json_END;
    }

    if(fread(fbuf, flen, 1, fp) == 1) {
        local = cJSON_Parse((const char*)fbuf);
        free(fbuf);
        fclose(fp);
    }
    else {
        free(fbuf);
        fclose(fp);
        DEBUGPRINT("read to %s fails\n", fname);
        rc = -4;
        goto jst_aggregate_json_END;
    }

    /// At this point the file is closed and the json is parsed into the
    /// "local" variable.  Make sure parsing succeeded.
    if (local == NULL) {
        DEBUGPRINT("JSON parsing failed.  Exiting.\n");
        rc = -5;
        goto jst_aggregate_json_END;
    }
    
    /// Link together the new JSON to the old.
    /// If *tmpl is NULL, the tmpl is empty, and we only need to link the local.
    /// If *tmpl not NULL, we need to move the local children into the tmpl.
    /// The head of the local JSON must be freed without touching the children.
    if (*tmpl == NULL) {
        *tmpl = local;
    }
    else {
        obj = (*tmpl)->child;
        while (obj != NULL) {
            obj = obj->next;
        }
        obj = local->child;
        
        // Free the head of the local without touching children.
        // Adapted from cJSON_Delete()
        obj = NULL;
        while (local != NULL) {
            obj = local->next;
            if (!(local->type & cJSON_IsReference) && (local->valuestring != NULL)) {
                cJSON_free(local->valuestring);
            }
            if (!(local->type & cJSON_StringIsConst) && (local->string != NULL)) {
                cJSON_free(local->string);
            }
            cJSON_free(local);
            local = obj;
        }
    }
    
    jst_aggregate_json_END:
    return rc;
}



