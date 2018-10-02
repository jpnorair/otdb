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
        
        obj = tmpl->child;
        while (obj != NULL) {
            cJSON* meta;
            cJSON* elem;
            int filesize = 0;
            int is_stock;

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
                filesize = elem->valueint;
                
                // Stock: optional, default=true
                elem = cJSON_GetObjectItemCaseSensitive(obj, "stock");
                if (elem == NULL) {
                    elem = cJSON_CreateBool(true);
                    cJSON_AddItemToObject(meta, "stock", elem);
                    is_stock = 1;
                }
                else {
                    is_stock = (elem->valueint != 0);
                }
                
                // Block: optional, default="isf"
                elem = cJSON_GetObjectItemCaseSensitive(obj, "block");
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
                elem = cJSON_GetObjectItemCaseSensitive(obj, "type");
                if (elem == NULL) {
                    elem = cJSON_CreateString("array");
                    cJSON_AddItemToObject(meta, "type", elem);
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
        
        // 3c. Write the file headers for GFB, ISS, ISF
        obj = tmpl->child;
        while (obj != NULL) {
            cJSON* meta;
            cJSON* elem;
            int filesize = 0;
            int is_stock;

            meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
            if (meta != NULL) {
                vl_header_t hdr;
                uint8_t*    insertion;
                
                // ID: mandatory
                elem = cJSON_GetObjectItemCaseSensitive(obj, "id");

                // Size: mandatory
                elem = cJSON_GetObjectItemCaseSensitive(obj, "size");


                
                // Stock: optional, default=true
                elem = cJSON_GetObjectItemCaseSensitive(obj, "stock");
                if (elem == NULL) {
                    elem = cJSON_CreateBool(true);
                    cJSON_AddItemToObject(meta, "stock", elem);
                    is_stock = 1;
                }
                else {
                    is_stock = (elem->valueint != 0);
                }
                
                // Block: optional, default="isf"
                elem = cJSON_GetObjectItemCaseSensitive(obj, "block");
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
                elem = cJSON_GetObjectItemCaseSensitive(obj, "type");
                if (elem == NULL) {
                    elem = cJSON_CreateString("array");
                    cJSON_AddItemToObject(meta, "type", elem);
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








