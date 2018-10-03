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


/// Variables used across shell commands
#define HOME_PATH_MAX           256
char home_path[HOME_PATH_MAX]   = "~/";
int home_path_len               = 2;



// used by DB manipulation commands
struct arg_str*     devid_man;
struct arg_file*    archive_man;
struct arg_lit*     compress_opt;

// used by file commands
struct arg_str*     devid_opt;
struct arg_str*     devidlist_opt;
struct arg_str*     fileblock_opt;
struct arg_str*     filerange_opt;
struct arg_int*     fileid_man;
struct arg_str*     fileperms_man;
struct arg_int*     filealloc_man;
struct arg_str*     filedata_man;

// used by all commands
struct arg_lit*     help_man;
struct arg_end*     end_man;



#define ARGFIELD_DEVICEID       (1<<0)
#define ARGFIELD_DEVICEIDOPT    (1<<1)
#define ARGFIELD_DEVICEIDLIST   (1<<2)
#define ARGFIELD_ARCHIVE        (1<<3)
#define ARGFIELD_COMPRESS       (1<<4)
#define ARGFIELD_BLOCKID        (1<<5)
#define ARGFIELD_FILEID         (1<<6)
#define ARGFIELD_FILEPERMS      (1<<7)
#define ARGFIELD_FILEALLOC      (1<<8)
#define ARGFIELD_FILERANGE      (1<<9) 
#define ARGFIELD_FILEDATA       (1<<10) 

typedef struct {
    unsigned int    fields;
    const char*     archive_path;
    uint8_t*        filedata;
    int             filedata_size;
    uint64_t        devid;
    const char**    devid_strlist;
    int             devid_strlist_size;
    uint8_t         compress_flag;
    uint8_t         block_id;
    uint8_t         file_id;
    uint8_t         file_perms;
    uint16_t        file_alloc;
    uint16_t        range_lo;
    uint16_t        range_hi;
} cmd_arglist_t;



void cmd_init_args(void) {
    /// Initialize the argtable structs
    devid_man       = arg_str1(NULL,NULL,"DeviceID",    "Device ID as HEX");
    archive_man     = arg_file1(NULL,NULL,"file",       "Archive file or directory");
    compress_opt    = arg_lit0("c","compress",          "Use compression on output (7z)");
    devidlist_opt   = arg_strn(NULL,NULL,"DeviceID List", 0, 256, "Batch of up to 256 Device IDs");
    devid_opt       = arg_str0("i","id","DeviceID",     "Device ID as HEX");
    fileblock_opt   = arg_str0("b","block","isf|iss|gfb", "File Block to search in");
    filerange_opt   = arg_str0("r","range","X:Y",       "Access File bytes between offsets X:Y");
    fileid_man      = arg_int1(NULL,NULL,"FileID",      "File ID, 0-255.");
    fileperms_man   = arg_str1(NULL,NULL,"Perms",       "Octal pair of User & Guest perms");
    filealloc_man   = arg_int1(NULL,NULL,"Alloc",       "Allocation bytes for file");
    filedata_man    = arg_str1(NULL,NULL,"Bintex",      "File data supplied as Bintex");
    help_man        = arg_lit0("h","help",              "Print this help and exit");
    end_man         = arg_end(10);
}




static int sub_readhex(uint8_t* dst, char* src, size_t src_bytes) {
        static const uint8_t hexlut0[128] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 16, 32, 48, 64, 80, 96,112,128,144, 0, 0, 0, 0, 0, 0, 
        0,160,176,192,208,224,240,  0,  0,  0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,160,176,192,208,224,240,  0,  0,  0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static const uint8_t hexlut1[128] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 
        0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    uint8_t* start = dst;

    while (src_bytes > 1) {
        uint8_t byte;
        byte    = hexlut0[ *src++ & 0x7f];
        byte   += hexlut1[ *src++ & 0x7f];
        *dst++  = byte;
    }

    return (int)(dst - start);
}


static int sub_load_element(uint8_t* dst, size_t max, double pos, const char* type, void* value) {
    int bytesout;
    int byteoffset;
    uint32_t bitmask;
    
    switch (type[0]) {
        // bitX_t, bool
        case 'b': {
            int shift;
            int dat;
            int mask;
            
            if (strcmp(&type[1], "ool") == 0) {
                dat = *(int*)value & 1;
            }

            
            
            shift = round( ((pos - floor(pos)) * 100) );
            
            
            
        
        } break;
        
        // char
        case 'c':
        case 'd':
        case 'h':
        case 'f':
        case 'i':
        case 'l':
        case 's':
        case 'u':
        default:
    }
    

    return bytesout;
}





uint8_t* goto_eol(uint8_t* src) {
    uint8_t* end = src;
    
    while ((*end != 0) && (*end != '\n')) {
        end++;
    }
    
    return end;
}


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







static int sub_extract_args(cmd_arglist_t* data, void* args, const char* cmdname, const char* src, int* src_bytes) {
    int     out_val;
    int     argc;
    char**  argv;
    int     nerrors;
    
    /// First, create an argument vector from the input string.
    /// hb_tools_parsestring will treat all bintex containers as whitespace-safe.
    argc = hb_tools_parsestring(&argv, cmdname, (char*)src, (char*)src, (size_t)*src_bytes);
    if (argc <= 1) {
        out_val = -2;
        goto sub_extract_args_END;
    }
    nerrors = arg_parse(argc, argv, args);
    
    /// Print command specific help
    /// @todo this is currently just generic help
    if (help_man->count > 0) {
        fprintf(stderr, "Usage: %s [cmd]", cmdname);
        arg_print_syntax(stderr, args, "\n");
        arg_print_glossary(stderr, args, "  %-25s %s\n");
        out_val = 0;
        goto sub_extract_args_END;
    }
    
    /// Print errors, if errors exist
    if (nerrors > 0) {
        char printbuf[32];
        snprintf(printbuf, sizeof(printbuf), "%s [cmd]", cmdname);
        arg_print_errors(stderr, end_man, printbuf);
        out_val = -3;
        goto sub_extract_args_END;
    }
    
    /// Device ID convert to uint64
    if (data->fields & ARGFIELD_DEVICEID) {
        if (devid_man->count > 0) {
            data->devid = strtoull(devid_man->sval[0], NULL, 16);
        }
        else {
            out_val = -4;
            goto sub_extract_args_END;
        }
    }
    else if (data->fields & ARGFIELD_DEVICEIDOPT) {
        if (devid_opt->count > 0) {
            data->devid = strtoull(devid_opt->sval[0], NULL, 16);
        }
        else {
            data->devid = 0;
        }
    }
    
    /// Archive Path
    if (data->fields & ARGFIELD_ARCHIVE) {
        if (archive_man->count > 0) {
            data->archive_path = archive_man->filename[0];
        }
        else {
            out_val = -5;
            goto sub_extract_args_END;
        }
    }
    
    /// Compression Flag
    if (data->fields & ARGFIELD_COMPRESS) {
        data->compress_flag = (compress_opt->count > 0);
    }
    
    /// List of Device IDs
    if (data->fields & ARGFIELD_DEVICEIDLIST) {
        data->devid_strlist_size    = devidlist_opt->count;
        data->devid_strlist         = devidlist_opt->sval;
    }
    
    /// Check for block flag (-b, --block), which specifies fs block
    /// Default is isf.
    if (data->fields & ARGFIELD_BLOCKID) {
        data->block_id = 3 << 4;      // default: isf
        if (fileblock_opt->count > 0) {
            DEBUGPRINT("Block arg encountered: %s\n", fileblock_opt->sval[0]);
            if (strncmp(fileblock_opt->sval[0], "isf", 3) == 0) {
                data->block_id = 3 << 4;
            }
            else if (strncmp(fileblock_opt->sval[0], "iss", 3) == 0) {
                data->block_id = 2 << 4;
            }
            else if (strncmp(fileblock_opt->sval[0], "gfb", 3) == 0) {
                data->block_id = 1 << 4;
            }
            else {
                out_val = -6;
                goto sub_extract_args_END;
            }
        }
    }
    
    /// Range is optional.  Default is maximum file range (0:)
    if (data->fields & ARGFIELD_FILERANGE) {
        data->range_lo  = 0;
        data->range_hi  = 65535;
        if (filerange_opt->count > 0) {
            char *start, *end, *ctx;
            DEBUGPRINT("Range arg encountered: %s\n", filerange_opt->sval[0]);
            start   = strtok_r((char*)filerange_opt->sval[0], ":", &ctx);
            end     = strtok_r(NULL, ":", &ctx);
            if (start != NULL) {
                data->range_lo = (uint16_t)atoi(start);
            }
            if (end != NULL) {
                data->range_hi = (uint16_t)atoi(end);
            }
        }
    }
    
    /// File ID is simply copied from the args
    if (data->fields & ARGFIELD_FILEID) {
        if (fileid_man->count > 0) {
            DEBUGPRINT("ID arg encountered: %d\n", fileid_man->ival[0]);
            data->file_id = (uint8_t)(fileid_man->ival[0] & 255);
        }
        else {
            out_val = -7;
            goto sub_extract_args_END;
        }
    }
    
    /// File ID is simply copied from the args
    if (data->fields & ARGFIELD_FILEPERMS) {
        if (fileperms_man->count > 0) {
            DEBUGPRINT("Permissions arg encountered: %d\n", fileperms_man->sval[0]);
            data->file_perms = (uint8_t)(63 & strtoul(fileperms_man->sval[0], NULL, 8));
        }
        else {
            out_val = -8;
            goto sub_extract_args_END;
        }
    }
    
    /// File ID is simply copied from the args
    if (data->fields & ARGFIELD_FILEALLOC) {
        if (filealloc_man->count > 0) {
            DEBUGPRINT("Alloc arg encountered: %d\n", fileid_man->ival[0]);
            data->file_id = (uint8_t)(255 & fileid_man->ival[0]);
        }
        else {
            out_val = -9;
            goto sub_extract_args_END;
        }
    }
    
    /// Filedata field is converted from bintex and stored to data->filedata
    if (data->fields & ARGFIELD_FILEDATA) {
        if ((filedata_man->count > 0) && (data->filedata != NULL)) {
            DEBUGPRINT("Filedata arg encountered: %s\n", filedata_man->sval[0]);
            data->filedata_size = bintex_ss((unsigned char*)filedata_man->sval[0], 
                                            (unsigned char*)data->filedata, 
                                            data->filedata_size             );
        }
        else {
            out_val = -10;
            goto sub_extract_args_END;
        }
    }

    /// Done!  Return 0 for success.
    out_val = 0;

    sub_extract_args_END:
    return out_val;
}









/** OTDB Internal Commands
  * -------------------------------------------------------------------------
  */

extern cmdtab_t* otdb_cmdtab;
int cmd_cmdlist(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {

    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dth == NULL) {
        return 0;
    }
    INPUT_SANITIZE();
    
    return cmdtab_list(otdb_cmdtab, (char*)dst, dstmax);
}



int cmd_quit(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    
    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dth == NULL) {
        return 0;
    }
    INPUT_SANITIZE();
    
    raise(SIGINT);
    return 0;
}









/** OTDB DB Manipulation Commands
  * -------------------------------------------------------------------------
  */

int cmd_devnew(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEIDOPT |ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, devid_opt, archive_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "dev-new", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUGPRINT("cmd_devnew():\n  archive=%s\n", arglist.archive_path);
        
        ///@todo implementation
        ///
        /// Only works with an open database
        
        /// 1. Load FS template and defaults (JSON).  
        /// 2. Generate the binary from the JSON.
        /// 3. Create a new filesystem instance with this binary.
    
        /// If no files are provided, use libotfs defaults
        //int otfs_load_defaults(void* handle, otfs_t* fs, size_t maxalloc);
        //int otfs_new(void* handle, const otfs_t* fs);
    }

    return rc;
}



int cmd_devdel(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID,
    };
    void* args[] = {help_man, devid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "dev-del", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUGPRINT("cmd_devdel():\n  device_id=%016llX\n", arglist.devid);
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_devdel_END;
            }
        }
        
        
    }

    cmd_devdel_END:
    return rc;
}



int cmd_devset(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID,
    };
    void* args[] = {help_man, devid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "dev-set", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUGPRINT("cmd_devset():\n  device_id=%016llX\n", arglist.devid);
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
            }
        }
    }

    return rc;
}


int cmd_open(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    char pathbuf[256];
    char* cursor;
    int len;
    DIR *dir;
    struct dirent *ent;
    
    cJSON* tmpl;
    cJSON* data;
    cJSON* obj;
    
    otfs_t tmpl_fs;
    vlFSHEADER fshdr;
    //uint16_t* gfb_base;
    //uint16_t* iss_base;
    //uint16_t* isf_base;
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
    rc = sub_extract_args(&arglist, args, "open", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
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
                rc = sub_aggregate_json(tmpl, ent->d_type);
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
                        int len;
                        
                        submeta = cJSON_GetObjectItemCaseSensitive(content, "_meta");
                        if (submeta != NULL) {
                            elem    = cJSON_GetObjectItemCaseSensitive(submeta, "pos");
                            offset  = (elem == NULL) ? 0 : elem->valueint;
                            elem    = cJSON_GetObjectItemCaseSensitive(submeta, "size");
                            len     = (elem == NULL) ? 0 : elem->valueint;
                        }
                        else {
                            elem    = cJSON_GetObjectItemCaseSensitive(content, "pos");
                            offset  = (elem == NULL) ? 0 : elem->valueint;
                            elem    = cJSON_GetObjectItemCaseSensitive(content, "type");
                            
                            if (
                            
                        }
                        elem = cJSON_GetObjectItemCaseSensitive(obj, "pos");
                        if (elem == NULL) {
                            offset = 0;
                        }
                        else {
                            offset = elem->valueint;
                        }
                        
                        elem = cJSON_GetObjectItemCaseSensitive(obj, "pos");
                        
                        
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
                        hdr->length = sub_readhex(filedata, content->valuestring, strlen(content->valuestring));
                    }
                }
            }

            obj = obj->next;
        }
        
        
        
        
        
        // Derive length from counting the content children
            content     = cJSON_GetObjectItemCaseSensitive(obj, "_content");
            hdr->length = 0;

            

        
        
        obj = tmpl->child;
        while (obj != NULL) {
            cJSON* meta;
            cJSON* elem;
            uint8_t block;
            vl_header_t fhdr;
            

            meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
            if (meta != NULL) {
                // ID: mandatory
                elem = cJSON_GetObjectItemCaseSensitive(obj, "id");
                if (elem == NULL) {
                    rc = -512 - 1;
                    goto cmd_open_FREEJSON;
                }
                
                // Size: mandatory
                elem = cJSON_GetObjectItemCaseSensitive(obj, "size");
                if (elem == NULL) {
                    rc = -512 - 2;
                    goto cmd_open_FREEJSON;
                }
                tmpl_fs.alloc += elem->valueint;
                
                // Block: optional, default="isf"
                elem = cJSON_GetObjectItemCaseSensitive(obj, "block");
                if (elem == NULL) {
                    elem = cJSON_CreateString("isf");
                    cJSON_AddItemToObject(meta, "block", elem);
                }
                
                // Type: optional, default="array"
                elem = cJSON_GetObjectItemCaseSensitive(obj, "type");
                if (elem == NULL) {
                    elem = cJSON_CreateString("array");
                    cJSON_AddItemToObject(meta, "type", elem);
                }
                
                // Stock: optional, default=true
                elem = cJSON_GetObjectItemCaseSensitive(obj, "stock");
                if (elem == NULL) {
                    elem = cJSON_CreateBool(true);
                    cJSON_AddItemToObject(meta, "stock", elem);
                }
                
                // Mod: optional, default=52
                elem = cJSON_GetObjectItemCaseSensitive(obj, "mod");
                if (elem == NULL) {
                    elem = cJSON_CreateNumber(52.);
                    cJSON_AddItemToObject(meta, "mod", elem);
                }
                
                // Time: optional, default=now()
                elem = cJSON_GetObjectItemCaseSensitive(obj, "time");
                if (elem == NULL) {
                    elem = cJSON_CreateNumber((double)time(NULL));
                    cJSON_AddItemToObject(meta, "time", elem);
                }
            }
            obj = obj->next;
        }
        
        
        
        // 5. 
        
        cmd_open_FREEJSON:
        
        
        cmd_open_CLOSE:
        closedir(dir);
    }
    
    cmd_open_END:
    return rc;
}


int cmd_save(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEIDLIST | ARGFIELD_COMPRESS | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, compress_opt, archive_man, devidlist_opt, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "save", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUGPRINT("cmd_open():\n  compress=%d\n  archive=%s\n", 
                arglist.compress_flag, arglist.archive_path);
                
        ///@todo implementation
        ///
        /// Step 1: Read the FS table descriptor to determine which elements
        /// are stored in the FS.  The table descriptor looks like this
        /// B0:1    Allocation Bytes of Metadata
        /// ---> File Action Fields not used in OTDB
        /// B2:3    GFB Bytes Allocated
        /// B4:5    GFB Bytes Used
        /// B6:7    GFB Stock Files
        /// B8:9    ISS Bytes Allocated
        /// B10:11  ISS Bytes Used
        /// B12:13  ISS Stock Files
        /// B14:15  ISF Bytes Allocated
        /// B16:17  ISF Bytes Used
        /// B18:19  ISF Stock Files
        /// B20:23  Modification Time 0
        /// B24:27  Modification Time 1
        ///
        /// Step 2: Get the optional file lists
        /// ID's for ISF files beyond the stock files are stored in ISF-7
        /// ID's for ISS files beyond the stock files are stored in ISF-8
        /// ID's for GFB files beyond the stock files are stored in ISF-9
        ///
        /// Step 3: For each file
        /// - Get the contents as a binary byte array.
        /// - Load the internal JSON template for this file and get the structure
        /// - Write an output JSON file using the structure from the internal template
        ///
        /// Step 4: Repeat for all files in DB, or for list of Device IDs supplied
        ///
        /// Step 5: Compress the output directory if required
        ///
    }
    
    return rc;
}








/** OTDB Filesystem Commands
  * -------------------------------------------------------------------------
  * @todo re-implement the functions from alp_filedata.
  */

int cmd_del(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "del", (const char*)src, inbytes);
    
    /// On successful extraction, delete a file in the device fs
    if (rc == 0) {
    
        DEBUGPRINT("cmd_del():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id);
                
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_del_END;
            }
        }
        
        rc = vl_delete(arglist.block_id, arglist.file_id, NULL);
        if (rc != 0) {
            rc = -512 - rc;
        }
    }
    
    cmd_del_END:
    return rc;
}



int cmd_new(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID | ARGFIELD_FILEPERMS | ARGFIELD_FILEALLOC,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, fileperms_man, filealloc_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "new", (const char*)src, inbytes);
    
    /// On successful extraction, create a new file in the device fs
    if (rc == 0) {
        vlFILE* fp = NULL;
        
        DEBUGPRINT("cmd_new():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_perms=%0o\n  file_alloc=%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.file_perms, arglist.file_alloc);
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_new_END;
            }
        }
        
        rc = vl_new(&fp, arglist.block_id, arglist.file_id, arglist.file_perms, arglist.file_alloc, NULL);
        if (rc != 0) {
            rc = -512 - rc;
        }
    }
    
    cmd_new_END:
    return rc;
}



int cmd_read(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    int span;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};

    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "r", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        void* ptr;
        vlFILE* fp;
        
        DEBUGPRINT("cmd_read():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
        
        span = arglist.range_hi - arglist.range_lo;
        if (dstmax < span) {
            rc = -5;
            goto cmd_read_END;
        }
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_read_END;
            }
        }
        
        rc = vl_getheader_vaddr(&header, arglist.block_id, arglist.file_id, VL_ACCESS_R, NULL);
        if (rc != 0) {
            rc = -512 - rc;
            goto cmd_read_END;
        }
        
        ptr = vl_memptr(fp);
        if (ptr != NULL) {
            rc = span;
            memcpy(dst, ptr, span);
        }
    }
    
    cmd_read_END:
    return rc;
}



int cmd_readall(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    int span;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "r*", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        void* ptr;
        vlFILE* fp;
            
        DEBUGPRINT("cmd_readall():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
        
        span = arglist.range_hi - arglist.range_lo;
        if (dstmax < (sizeof(vl_header_t) + span)) {
            rc = -5;
            goto cmd_readall_END;
        }
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_readall_END;
            }
        }
        
        rc = vl_getheader_vaddr(&header, arglist.block_id, arglist.file_id, VL_ACCESS_R, NULL);
        if (rc != 0) {
            rc = -512 - rc;
            goto cmd_readall_END;
        }
        
        ptr = vworm_get(header);
        if (ptr == NULL) {
            rc = -512 - 255;
            goto cmd_readall_END;
        }
            
        rc = sizeof(vl_header_t);
        memcpy(dst, ptr, sizeof(vl_header_t));
        
        ptr = vl_memptr(fp);
        if (ptr != NULL) {
            dst += sizeof(vl_header_t);
            rc  += span;
            memcpy(dst, ptr, span);
        }
    }
    
    cmd_readall_END:
    return rc;
}



int cmd_restore(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
///@todo not currently supported, always returns error.

    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "z", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUGPRINT("cmd_restore():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_restore_END;
            }
        }
        
        ///@todo not currently supported, always returns error.
        rc = -1;
    }
    
    cmd_restore_END:
    return rc;
}



int cmd_readhdr(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "rh", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        void* ptr;
        
        DEBUGPRINT("cmd_readhdr():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id);
        
        if (dstmax < sizeof(vl_header_t)) {
            rc = -5;
            goto cmd_readhdr_END;
        }
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_readhdr_END;
            }
        }

        rc = vl_getheader_vaddr(&header, arglist.block_id, arglist.file_id, VL_ACCESS_R, NULL);
        if (rc != 0) {
            rc = -512 - rc;
            goto cmd_readhdr_END;
        }

        ptr = vworm_get(header);
        if (ptr == NULL) {
            rc = -512 - 255;
            goto cmd_readhdr_END;
        }

        rc = sizeof(vl_header_t);
        memcpy(dst, ptr, sizeof(vl_header_t));
    }
    
    cmd_readhdr_END:
    return rc;
}



int cmd_readperms(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "rp", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        
        DEBUGPRINT("cmd_restore():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
                
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_readperms_END;
            }
        }
        
        ///
        ///@note OTDB works entirely as the root user (NULL user_id)
        rc = vl_getheader_vaddr(&header, arglist.block_id, arglist.file_id, VL_ACCESS_R, NULL);
        if (rc != 0) {
            rc = -512 - rc;
            goto cmd_readperms_END;
        } 

        rc = 1;
        *dst = vworm_read(header + 4) >> 8;
    }
    
    cmd_readperms_END:
    return rc;
}



int cmd_write(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID | ARGFIELD_FILEDATA,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, filedata_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "w", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        vlFILE*     fp;
        uint8_t*    dptr;
        int         span;
    
        DEBUGPRINT("cmd_restore():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n  write_bytes=%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi, arglist.filedata_size);
                
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_write_END;
            }
        }

        /// The write operation for OTDB is a direct access to RAM, once
        /// getting the hardware address of the data element.  This works
        /// when the data is stored in RAM, which is always the case for 
        /// OTDB even though it is often not the case for embedded OTFS 
        /// implementations.
        ///
        ///@note OTDB works entirely as the root user (NULL user_id)
        rc = vl_getheader_vaddr(&header, arglist.block_id, arglist.file_id, VL_ACCESS_W, NULL);
        if (rc != 0) {
            rc = 512 - rc;
            goto cmd_write_END;
        }
        
        fp = vl_open_file(header);
        if (fp == NULL) {
            rc = -512 - 255;
            goto cmd_write_END;
        }
            
        dptr = vl_memptr(fp);
        if (dptr == NULL) {
            rc = -512 - 255;
            goto cmd_write_END;
        }
            
        if (arglist.range_lo >= fp->alloc) {
            rc = -512 - 7;
            goto cmd_write_END;
        }
            
        if (arglist.range_hi > fp->alloc) {
            arglist.range_hi = fp->alloc;
        }
            
        fp->length  = arglist.range_hi;
        span        = arglist.range_hi - arglist.range_lo;
        if (span > 0) {
            memcpy(&dptr[arglist.range_lo], src, span);
        }
            
        ///@todo update the timestamp on this file, and on the device instance.
    }
    
    cmd_write_END:
    return rc;
}



int cmd_writeperms(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID | ARGFIELD_FILEPERMS,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, fileperms_man, end_man};
    
    /// Writeperms requires an initialized OTFS handle
    if (dth->ext == NULL) {
        rc = -1;
    }
    else {
        /// Extract arguments into arglist struct
        rc = sub_extract_args(&arglist, args, "wp", (const char*)src, inbytes);
    
        /// On successful extraction, write permissions to the specified file,
        /// on the specified device, within the device database.
        if (rc == 0) {
            DEBUGPRINT("cmd_restore():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  write_perms=%0o\n", 
                    arglist.devid, arglist.block_id, arglist.file_id, arglist.file_perms);
                    
            if (arglist.devid != 0) {
                rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
                if (rc != 0) {
                    rc = -256 + rc;
                    goto cmd_writeperms_END;
                }
            }

            /// Run the chmod and return the error code (0 is no error)
            /// The error code from OTFS is positive.
            ///@note OTDB works entirely as the root user (NULL user_id)
            rc = vl_chmod( (vlBLOCK)arglist.block_id, 
                            (uint8_t)arglist.file_id, 
                            (uint8_t)arglist.file_perms, 
                            NULL    );
            if (rc != 0) {
                rc = -512 - rc;
            }
        }
    }
    
    cmd_writeperms_END:
    return rc;
}








