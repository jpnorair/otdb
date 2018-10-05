/* Copyright 2014, JP Norair
  *
  * Licensed under the OpenTag License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  */

// Local Headers
#include "cmds.h"
#include "dterm.h"
#include "cliopt.h"
#include "otdb_cfg.h"

// HB Headers/Libraries
#include <bintex.h>
#include <cmdtab.h>
#include <argtable3.h>
#include <cJSON.h>
#include <otfs.h>
#include <hbdp/hb_cmdtools.h>       ///@note is this needed?

// Standard C & POSIX Libraries
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>


// used by DB manipulation commands
extern struct arg_str*     devid_man;
extern struct arg_file*    archive_man;
extern struct arg_lit*     compress_opt;

// used by file commands
extern struct arg_str*     devid_opt;
extern struct arg_str*     devidlist_opt;
extern struct arg_str*     fileblock_opt;
extern struct arg_str*     filerange_opt;
extern struct arg_int*     fileid_man;
extern struct arg_str*     fileperms_man;
extern struct arg_int*     filealloc_man;
extern struct arg_str*     filedata_man;

// used by all commands
extern struct arg_lit*     help_man;
extern struct arg_end*     end_man;


#define INPUT_SANITIZE() do { \
    if ((src == NULL) || (dst == NULL)) {   \
        *inbytes = 0;                       \
        return -1;                          \
    }                                       \
} while(0)

#if OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif


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





static int sub_typesize(typeinfo_t* spec, const char* type) {
/// Returns 0 if type is recognized.
/// Loads 'spec' param with extracted type information.
    int offset = 0;
    const char* cursor = &type[1];
    
    spec->index = TYPE_MAX;
    
    switch (type[0]) {
        // bitX_t, bool
        case 'b': {
            if (strcmp(cursor, "ool") == 0) {
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
            if ((strcmp(cursor, "har") == 0)) {
                spec->index = TYPE_int8;
                spec->bits  = 8;
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
            if (strcmp(cursor, "ex") == 0) {
                spec->index = TYPE_hex;
                spec->bits  = -2;
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
        
        // short or string
        case 's': {
            if (strcmp(cursor, "hort") == 0) {
                spec->index = TYPE_int16;
                spec->bits  = 16;
            }
            else if (strcmp(cursor, "tring") == 0) {
                spec->index = TYPE_string;
                spec->bits  = -1;
            }
        } break;
        
        default: break;
    }
    
    return (spec->index < TYPE_MAX) - 1;
}


///@todo update this to use sub_typesize
static int sub_load_element(uint8_t* dst, size_t size, double pos, const char* type, cJSON* value) {
    typeinfo_t typeinfo;
    int bytesout;
    
    if ((dst==NULL) || (size==0) || (type==NULL) || (value==NULL)) {
        return 0;
    }
    
    // If sub_typesize returns nonzero, then type is invalid.  Write 0 bytes.
    if (sub_typesize(&typeinfo, type) != 0) {
        return 0;
    }
    
    bytesout = 0;
    switch (typeinfo.index) {
        // Bitmask type returns -1 because it is a container
        case TYPE_bitmask: return -1;
    
        // Bit types require a mask and set operation
        case TYPE_bit1:
        case TYPE_bit2:
        case TYPE_bit3:
        case TYPE_bit4:
        case TYPE_bit5:
        case TYPE_bit6:
        case TYPE_bit7:
        case TYPE_bit8: {
            long shift;
            ot_uni32 scr;
            unsigned long dat       = 0;
            unsigned long maskbits  = typeinfo.bits;
            
            if (cJSON_IsNumber(value)) {
                dat = value->valueint;
            }
            else if (cJSON_IsString(value)) {
                cmd_readhex((uint8_t*)&dat, value->valuestring, 8);
            }

            shift = lround( ((pos - floor(pos)) * 100) );
            if ((1+((shift+maskbits)/4)) > size) {
                goto sub_load_element_END;
            }
            
            ///@todo this may be endian dependent
            memcpy(&scr.ubyte[0], dst, size);
            maskbits    = ((1<<maskbits) - 1) << shift;
            dat       <<= shift;
            if (size >= 4) {
                scr.ulong  &= ~maskbits; 
                scr.ulong  |= (dat & maskbits);
            }
            else if (size >= 2) {
                scr.ushort[0] &= ~maskbits; 
                scr.ushort[0] |= (dat & maskbits);
            }
            else {
                scr.ubyte[0] &= ~maskbits; 
                scr.ubyte[0] |= (dat & maskbits);
            }
            memcpy(dst, &scr.ubyte[0], size);
            bytesout = (int)size;
        } break;
    
        // String and hex types have length determined by value test
        case TYPE_string: {
            if (cJSON_IsString(value)) {
                int len = (int)strlen((char*)value);
                if (size >= len) {
                    memcpy(dst, (char*)value, len);
                    bytesout = len;
                }
            }
        } break;
        
        case TYPE_hex: {
            if (cJSON_IsString(value)) {
                int len = (int)strlen((char*)value);
                if (size >= len/2) {
                    bytesout = cmd_readhex(dst, (char*)value, len);
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
            int len = typeinfo.bits/8;
            if (size >= len) {
                if (cJSON_IsString(value)) {
                    memset(dst, 0, len);
                    cmd_readhex(dst, (char*)value, len);
                }
                else if (cJSON_IsNumber(value)) {
                    if (typeinfo.index == TYPE_float) {
                        float tmp = (float)value->valuedouble;
                        memcpy(dst, &tmp, len);
                    }
                    else if (typeinfo.index == TYPE_double) {
                        memcpy(dst, &value->valuedouble, len);
                    }
                    else {
                        memcpy(dst, &value->valueint, len);
                    }
                }
                bytesout = len;
            } 
        } break;
    
        default: break;
    }
    
    sub_load_element_END:
    return bytesout;
}



static int sub_aggregate_json(cJSON** tmpl, const char* fname) {
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
        goto sub_aggregate_json_END;
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
        goto sub_aggregate_json_END;
    }

    /// At this point the file is closed and the json is parsed into the
    /// "local" variable.  Make sure parsing succeeded.
    if (local == NULL) {
        DEBUGPRINT("JSON parsing failed.  Exiting.\n");
        rc = -5;
        goto sub_aggregate_json_END;
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
    
    sub_aggregate_json_END:
    return rc;
}





int cmd_open(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    char pathbuf[256];
    char* cursor;
    int len;
    DIR *dir;
    struct dirent *ent;
    
    cJSON* tmpl = NULL;
    cJSON* data = NULL;
    cJSON* obj  = NULL;
    
    otfs_t tmpl_fs;
    vlFSHEADER fshdr;
    vl_header_t *gfbhdr, *isshdr, *isfhdr;
    
    
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, archive_man, end_man};

    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dth == NULL) {
        return 0;
    }
    INPUT_SANITIZE();
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "open", (const char*)src, inbytes);
    if (rc != 0) {
        goto cmd_open_END;
    }
    
    /// On successful extraction, create a new device in the database
    DEBUGPRINT("cmd_open():\n  archive=%s\n", arglist.archive_path);
    
    ///@todo implementation
    /// When opening a database, a copy of the template gets saved 
    /// temporarily as long as the Database is open.
    
    /// 1. Check if input file is compressed.  If so, decompress it to 
    ///    the local store (.activedb)
    /// 
    /// 1b.If input file is a directory (not compressed), copy it to the
    ///    local store (.activedb)
    
    /// 2. Template is stored in "_TMPL" directory within the archive 
    ///    folder.  Go through the _TMPL folder and create a single JSON
    ///    model that contains all the object data from all the files.
    ///    If any of the JSON is invalid, send an error (and say which 
    ///    where the error was, if possible).
    
    /// 3. Do a check to make sure the "_meta" object for each object has
    ///    at least "id" and "size" elements.  Optional elements are 
    ///    "block", "type", "stock," "mod", and "time".  If these elements
    ///    are missing, add defaults.  Default block is "isf".  Default 
    ///    stock is "true".  Default mod is 00110100 (octal 64, decimal 52).  
    ///    Default time is the present epoch seconds since 1.1.1970.
    /// 3b.Create the master FS-table for this template, using the data
    ///    from above.
    /// 3c.Create all the master FS metadata, using the data from above.
    
    /// 4. Data is stored in directories that have a name corresponding to
    ///    the DeviceID of the Device they are on.  Go through each of 
    ///    these files and make sure that the name of the directory is a
    ///    valid DeviceID (a number).  If there is an error, don't import
    ///    that Device.  Else, add a new FS to the database using this ID
    ///    and build a JSON data from an aggregate of all the JSON files
    ///    inside the directory. 
    /// 4b.Using the JSON data for this file, correlate it against the 
    ///    master template and write it to each file in the new FS.
    
    len     = sizeof(pathbuf) - sizeof("/_TMPL/") - 16 - 32 - 1;
    cursor  = stpncpy(pathbuf, arglist.archive_path, len);
    cursor  = stpcpy(cursor, "/_TMPL/");
    dir     = opendir(pathbuf);
    if (dir == NULL) {
        rc = -1;
        goto cmd_open_CLOSE;
    }
    
    // 2.
    tmpl = NULL;
    while (1) {
        ent = readdir(dir);
        if (ent == NULL) {
            break;
        }
        if (ent->d_type == DT_REG) {
            rc = sub_aggregate_json(&tmpl, ent->d_name);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_open_CLOSE;
            }
        }
    }
    
    // 3. 
    tmpl_fs.alloc   = 0;
    tmpl_fs.base    = NULL;
    tmpl_fs.uid.u64 = 0;
    memset(&fshdr, 0, sizeof(vlFSHEADER));
    
    if (tmpl == NULL) {
        // No template found
        rc = -2;
        goto cmd_open_CLOSE;
    }
    
    // 3a. Check template metadata.  Make sure it is valid.  Optional 
    // elements get filled with defaults if not present.
    obj = tmpl->child;
    while (obj != NULL) {
        cJSON* meta;
        cJSON* elem;
        int filesize = 0;
        int is_stock;

        meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
        if (meta != NULL) {
            // ID: mandatory
            elem = cJSON_GetObjectItemCaseSensitive(meta, "id");
            if (elem == NULL) {
                rc = -512 - 1;
                goto cmd_open_FREEJSON;
            }
            
            // Size: mandatory
            elem = cJSON_GetObjectItemCaseSensitive(meta, "size");
            if (elem == NULL) {
                rc = -512 - 2;
                goto cmd_open_FREEJSON;
            }
            filesize = elem->valueint;
            
            // Stock: optional, default=true
            elem = cJSON_GetObjectItemCaseSensitive(meta, "stock");
            if (elem == NULL) {
                elem = cJSON_CreateBool(true);
                cJSON_AddItemToObject(meta, "stock", elem);
                is_stock = 1;
            }
            else {
                is_stock = (elem->valueint != 0);
            }
            
            // Block: optional, default="isf"
            elem = cJSON_GetObjectItemCaseSensitive(meta, "block");
            if (elem == NULL) {
                fshdr.isf.alloc += filesize;
                fshdr.isf.used  += is_stock;
                fshdr.isf.files++;
                elem = cJSON_CreateString("isf");
                cJSON_AddItemToObject(meta, "block", elem);
            }
            else if (strcmp(elem->valuestring, "gfb") == 0) {
                fshdr.gfb.alloc += filesize;
                fshdr.gfb.used  += is_stock;
                fshdr.gfb.files++;
            }
            else if (strcmp(elem->valuestring, "iss") == 0) {
                fshdr.iss.alloc += filesize;
                fshdr.iss.used  += is_stock;
                fshdr.iss.files++;
            }
            else {
                fshdr.isf.alloc += filesize;
                fshdr.isf.used  += is_stock;
                fshdr.isf.files++;
            }
            
            // Type: optional, default="array"
            elem = cJSON_GetObjectItemCaseSensitive(meta, "type");
            if (elem == NULL) {
                elem = cJSON_CreateString("array");
                cJSON_AddItemToObject(meta, "type", elem);
            }
            
            // Mod: optional, default=52
            elem = cJSON_GetObjectItemCaseSensitive(meta, "mod");
            if (elem == NULL) {
                elem = cJSON_CreateNumber(52.);
                cJSON_AddItemToObject(meta, "mod", elem);
            }
            
            // Time: optional, default=now()
            elem = cJSON_GetObjectItemCaseSensitive(meta, "time");
            if (elem == NULL) {
                elem = cJSON_CreateNumber((double)time(NULL));
                cJSON_AddItemToObject(meta, "time", elem);
            }
        }
        obj = obj->next;
    }
    
    // 3b. Add overhead section to the allocation, Malloc the fs data
    fshdr.ftab_alloc    = sizeof(vlFSHEADER) + sizeof(vl_header_t)*(fshdr.isf.files+fshdr.iss.files+fshdr.gfb.files);
    fshdr.res_time0     = (uint32_t)time(NULL);
    tmpl_fs.alloc       = vworm_fsalloc(&fshdr);
    tmpl_fs.base        = calloc(tmpl_fs.alloc, sizeof(uint8_t));
    if (tmpl_fs.base == NULL) {
        rc = -3;
        goto cmd_open_FREEJSON;
    }
    memcpy(tmpl_fs.base, &fshdr, sizeof(vlFSHEADER));
    
    gfbhdr  = (fshdr.gfb.files != 0) ? tmpl_fs.base+sizeof(vlFSHEADER) : NULL;
    isshdr  = (fshdr.iss.files != 0) ? tmpl_fs.base+sizeof(vlFSHEADER)+(fshdr.gfb.files*sizeof(vl_header_t)) : NULL;
    isfhdr  = (fshdr.isf.files != 0) ? tmpl_fs.base+sizeof(vlFSHEADER)+((fshdr.gfb.files+fshdr.iss.files)*sizeof(vl_header_t)) : NULL;
    
    // 3c. Prepare the file table, except for base and length fields, which
    //     are done in 3d and 3e respectively.
    obj = tmpl->child;
    while (obj != NULL) {
        cJSON*  meta;
        vl_header_t* hdr;

        meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
        if (meta == NULL) {
            // Skip files without meta objects
            continue;
        }
        else {
            ot_uni16    idmod;
            cJSON*      elem;
            
            // IDMOD & Block
            // Find the file header position based on Block and ID
            ///@todo make this a function (used in multiple places)
            ///@todo implement way to use non-stock files
            elem            = cJSON_GetObjectItemCaseSensitive(meta, "id");
            idmod.ubyte[0]  = (uint8_t)(255 & elem->valueint);
            elem            = cJSON_GetObjectItemCaseSensitive(meta, "mod");
            idmod.ubyte[1]  = (uint8_t)(255 & elem->valueint);
            elem            = cJSON_GetObjectItemCaseSensitive(meta, "block");
            if (strcmp(elem->valuestring, "gfb") == 0) {
                if (idmod.ubyte[0] >= fshdr.gfb.used) {
                    continue;   
                }
                hdr = &gfbhdr[idmod.ubyte[0]];
            }
            else if (strcmp(elem->valuestring, "iss") == 0) {
                if (idmod.ubyte[0] >= fshdr.iss.used) {
                    continue;   
                }
                hdr = &isshdr[idmod.ubyte[0]];
            }
            else {
                if (idmod.ubyte[0] >= fshdr.isf.used) {
                    continue;   
                }
                hdr = &isfhdr[idmod.ubyte[0]];
            }
            
            // TIME: epoch seconds
            elem        = cJSON_GetObjectItemCaseSensitive(meta, "time");
            hdr->modtime= (uint32_t)elem->valueint;
            
            // MIRROR: always 0xFFFF for OTDB
            hdr->mirror = 0xFFFF;
            
            // BASE: this is derived after all tmpl files are loaded
            //hdr->base = ... ;
            
            // IDMOD: from already extracted value
            hdr->idmod  = idmod.ushort;
            
            // ALLOC
            elem        = cJSON_GetObjectItemCaseSensitive(meta, "size");
            hdr->alloc  = (uint16_t)elem->valueint;
            
            // LENGTH: this is derived later, from file contents
            //hdr->length  = ... ;
        }
        
        obj = obj->next;
    }
    
    // 3d. Derive Base values from file allocations and write them to table
    if (fshdr.gfb.used > 0) {
        gfbhdr[0].base = fshdr.ftab_alloc;
        for (int i=1; i<fshdr.gfb.used; i++) {
            gfbhdr[i].base = gfbhdr[i-1].base + gfbhdr[i-1].alloc;
        }
    }
    if (fshdr.iss.used > 0) {
        isshdr[0].base = fshdr.ftab_alloc + fshdr.gfb.alloc;
        for (int i=1; i<fshdr.iss.used; i++) {
            isshdr[i].base = isshdr[i-1].base + isshdr[i-1].alloc;
        }
    }
    if (fshdr.isf.used > 0) {
        isfhdr[0].base = fshdr.ftab_alloc + fshdr.gfb.alloc + fshdr.iss.alloc;
        for (int i=1; i<fshdr.isf.used; i++) {
            isfhdr[i].base = isfhdr[i-1].base + isfhdr[i-1].alloc;
        }
    }
    
    // 3e. Derive Length values from template and write default values to
    // the filesystem.
    obj = tmpl->child;
    while (obj != NULL) {
        cJSON*  meta;
        cJSON*  content;
        vl_header_t* hdr;
        bool    is_stock;
        int     tmpl_type;
        uint8_t* filedata;

        meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
        if (meta == NULL) {
            // Skip files without meta objects
            continue;
        }
        else {
            ot_uni16    idmod;
            cJSON*      elem;
            
            // IDMOD & Block
            // Find the file header position based on Block and ID
            ///@todo make this a function (used in multiple places)
            ///@todo implement way to use non-stock files
            elem            = cJSON_GetObjectItemCaseSensitive(meta, "id");
            idmod.ubyte[0]  = (uint8_t)(255 & elem->valueint);
            elem            = cJSON_GetObjectItemCaseSensitive(meta, "mod");
            idmod.ubyte[1]  = (uint8_t)(255 & elem->valueint);
            elem            = cJSON_GetObjectItemCaseSensitive(meta, "block");
            if (strcmp(elem->valuestring, "gfb") == 0) {
                if (idmod.ubyte[0] >= fshdr.gfb.used) {
                    continue;   
                }
                hdr = &gfbhdr[idmod.ubyte[0]];
            }
            else if (strcmp(elem->valuestring, "iss") == 0) {
                if (idmod.ubyte[0] >= fshdr.iss.used) {
                    continue;   
                }
                hdr = &isshdr[idmod.ubyte[0]];
            }
            else {
                if (idmod.ubyte[0] >= fshdr.isf.used) {
                    continue;   
                }
                hdr = &isfhdr[idmod.ubyte[0]];
            }
            
            // Implicit params stock & type
            elem        = cJSON_GetObjectItemCaseSensitive(meta, "stock");
            is_stock    = (bool)(elem->valueint != 0);
            elem        = cJSON_GetObjectItemCaseSensitive(meta, "type");
            if (strcmp(elem->valuestring, "struct") == 0) {
                tmpl_type = 2;
            }
            else if (strcmp(elem->valuestring, "array") == 0) {
                tmpl_type = 1;  // array
            }
            else {
                tmpl_type = 0;  //hex string
            }
        }
        
        content     = cJSON_GetObjectItemCaseSensitive(obj, "_content");
        hdr->length = 0;
        filedata    = (uint8_t*)tmpl_fs.base + hdr->base;
        if (content != NULL) {
            // Struct type, most involved
            if (tmpl_type == 2) {       
                content = content->child;
                while (content != NULL) {
                    cJSON* elem;
                    cJSON* submeta;
                    int offset;
                    int e_sz;
                    
                    // Nested data element has _meta field
                    ///@todo recursive treatment of nested elements
                    submeta = cJSON_GetObjectItemCaseSensitive(content, "_meta");
                    if (submeta != NULL) {
                        elem    = cJSON_GetObjectItemCaseSensitive(submeta, "pos");
                        offset  = (elem == NULL) ? 0 : elem->valueint;
                        elem    = cJSON_GetObjectItemCaseSensitive(submeta, "size");
                        e_sz    = (elem == NULL) ? 0 : elem->valueint;
                        submeta = cJSON_GetObjectItemCaseSensitive(content, "_content");
                        if (submeta != NULL) {
                            submeta = submeta->child;
                            while (submeta != NULL) {
                                double pos;
                                char* typestr;
                                elem    = cJSON_GetObjectItemCaseSensitive(submeta, "pos");
                                pos     = (elem == NULL) ? 0. : elem->valuedouble;
                                elem    = cJSON_GetObjectItemCaseSensitive(submeta, "type");
                                typestr = (elem == NULL) ? NULL : elem->valuestring;
                                sub_load_element( &filedata[offset], 
                                        (size_t)e_sz, 
                                        pos, 
                                        typestr, 
                                        cJSON_GetObjectItemCaseSensitive(content, "def"));
                                submeta = submeta->next;
                            }
                        }
                    }
                    
                    // Flat data element
                    else {
                        elem    = cJSON_GetObjectItemCaseSensitive(content, "pos");
                        offset  = (elem == NULL) ? 0 : elem->valueint;
                        elem    = cJSON_GetObjectItemCaseSensitive(content, "type");
                        e_sz    = sub_load_element( &filedata[offset], 
                                        (size_t)(hdr->alloc - offset), 
                                        0, 
                                        elem->valuestring, 
                                        cJSON_GetObjectItemCaseSensitive(content, "def"));
                    }
                    content = content->next;
                }
            }
            
            // Bytearray type, each 
            else if (tmpl_type == 1) {  
                if (cJSON_IsArray(content)) {
                    hdr->length = cJSON_GetArraySize(content);
                    if (hdr->length > hdr->alloc) {
                        hdr->length = hdr->alloc;
                    }
                    for (int i=0; i<hdr->length; i++) {
                        cJSON* array_i  = cJSON_GetArrayItem(content, i);
                        filedata[i]     = (uint8_t)(255 & array_i->valueint);
                    }
                }
            }
            
            // hexstring type
            else {                      
                if (cJSON_IsString(content)) {
                    size_t srcbytes;
                    srcbytes = strlen(content->valuestring);
                    if (srcbytes > (2*hdr->alloc)) {
                        srcbytes = (2*hdr->alloc);
                    }
                    hdr->length = cmd_readhex(filedata, content->valuestring, strlen(content->valuestring));
                }
            }
        }

        obj = obj->next;
    }
    
    // 4. By this point, the default FS is created based on the input 
    // template.  For each device in the imported JSON, we make a copy of
    // this template and apply changes that are present.
    
    // Go into each directory that isn't "_TMPL"
    len     = sizeof(pathbuf) - 16 - 32 - 1;
    cursor  = stpncpy(pathbuf, arglist.archive_path, len);
    dir     = opendir(pathbuf);
    if (dir == NULL) {
        rc = -1;
        goto cmd_open_FREEJSON;
    }
    
    while (1) {
        char* endptr;
    
        ent = readdir(dir);
        if (ent == NULL) {
            break;
        }
        if (ent->d_type == DT_DIR) {
            if (strcmp(ent->d_name, "_TMPL") == 0) {
                continue;
            }
            
            // Name of directory should be a pure hex number
            endptr = NULL;
            tmpl_fs.uid.u64 = strtoull(ent->d_name, &endptr, 16);
            if (((*ent->d_name != '\0') && (*endptr == '\0')) == 0) {
                continue;
            }
            
            // Create new FS based on device id and template FS
            rc = ofts_new(dth->ext, &tmpl_fs);
            if (rc != 0) {
                goto cmd_open_FREEJSON;
            }
            
            ///@todo leave-off point
        }
    }
    
    
    // 5. 
    
    cmd_open_FREEJSON:
    cJSON_Delete(tmpl);
    cJSON_Delete(data);
    
    cmd_open_CLOSE:
    closedir(dir);
        
    
    cmd_open_END:
    return rc;
}

