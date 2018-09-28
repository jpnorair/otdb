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
#include <otfs.h>
#include <hbdp/hb_cmdtools.h>       ///@note is this needed?

// Standard C & POSIX Libraries
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>



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
struct arg_str*     fileblock_opt;
struct arg_str*     filerange_opt;
struct arg_int*     fileid_man;
struct arg_str*     fileperms_man;
struct arg_int*     filealloc_man;
struct arg_str*     filedata_man;

// used by all commands
struct arg_lit*     help_man;
struct arg_end*     end_man;



#define ARGFIELD_DEVICEID   (1<<0)
#define ARGFIELD_DEVICEIDOPT (1<<1)
#define ARGFIELD_ARCHIVE    (1<<2)
#define ARGFIELD_COMPRESS   (1<<3)
#define ARGFIELD_BLOCKID    (1<<4)
#define ARGFIELD_FILEID     (1<<5)
#define ARGFIELD_FILEPERMS  (1<<6)
#define ARGFIELD_FILEALLOC  (1<<7)
#define ARGFIELD_FILERANGE  (1<<8) 
#define ARGFIELD_FILEDATA   (1<<9) 

typedef struct {
    unsigned int    fields;
    const char*     archive_path;
    uint8_t*        filedata;
    int             filedata_size;
    uint64_t        devid;
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

#if OTDB_FEATURE(DEBUG)
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
    if (dt == NULL) {
        return 0;
    }
    INPUT_SANITIZE();
    
    return cmdtab_list(otdb_cmdtab, (char*)dst, dstmax);
}



int cmd_quit(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    
    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dt == NULL) {
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
        ///@todo implementation
        fprintf(stderr, "cmd_devnew():\n  archive=%s\n", arglist.archive_path);
        
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
        ///@todo implementation
        fprintf(stderr, "cmd_devdel():\n  device_id=%016llX\n", arglist.devid);
    }

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
        ///@todo implementation
        fprintf(stderr, "cmd_devset():\n  device_id=%016llX\n", arglist.devid);
        //int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
    }

    return rc;
}


int cmd_open(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, archive_man, end_man};

    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dt == NULL) {
        return 0;
    }
    INPUT_SANITIZE();
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "open", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        ///@todo implementation
        fprintf(stderr, "cmd_open():\n  archive=%s\n", arglist.archive_path);
    }
    
    return rc;
}


int cmd_save(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_COMPRESS | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, compress_opt, archive_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "save", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        ///@todo implementation
        fprintf(stderr, "cmd_open():\n  compress=%d\n  archive=%s\n", 
                arglist.compress_flag, arglist.archive_path);
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
        ///@todo implementation
        fprintf(stderr, "cmd_del():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id);
                
        if (arglist.devid != 0) {
            ///@todo otfs_setfs()
        }
        
        //int otfs_del(void* handle, const otfs_t* fs, bool unload);
    }
    
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
        ///@todo implementation
        fprintf(stderr, "cmd_new():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_perms=%0o\n  file_alloc=%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.file_perms, arglist.file_alloc);
                
        if (arglist.devid != 0) {
            ///@todo int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
        }
        
        //int otfs_del(void* handle, const otfs_t* fs, bool unload);
    }
    
    return rc;
}



int cmd_read(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};

    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "r", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        ///@todo implementation
        fprintf(stderr, "cmd_read():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
                
        if (arglist.devid != 0) {
            ///@todo int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
        }
        
        //int otfs_read(void* handle, const otfs_t* fs, bool unload);
    }
    
    return rc;
}



int cmd_readall(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "r*", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        ///@todo implementation
        fprintf(stderr, "cmd_readall():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
                
        if (arglist.devid != 0) {
            ///@todo int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
        }
        
        //int otfs_readall(void* handle, const otfs_t* fs, bool unload);
    }
    
    return rc;
}



int cmd_restore(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = sub_extract_args(&arglist, args, "z", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        ///@todo implementation
        fprintf(stderr, "cmd_restore():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
                
        if (arglist.devid != 0) {
            ///@todo int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
        }
        
        //int otfs_readall(void* handle, const otfs_t* fs, bool unload);
    }
    
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
        ///@todo implementation
        fprintf(stderr, "cmd_readhdr():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id);
                
        if (arglist.devid != 0) {
            rc = 256 * otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
        }
        
        //int otfs_readhdr(void* handle, const otfs_t* fs, bool unload);
    }
    
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
        ///@todo THIS LINE ONLY FOR DEBUG
        fprintf(stderr, "cmd_restore():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
                
        if (arglist.devid != 0) {
            rc = 256 * otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
        }
        if (rc == 0) {
            vaddr header;
        
            ///
            ///@note OTDB works entirely as the root user (NULL user_id)
            rc = vl_getheader_vaddr(&header, arglist.block_id, arglist.file_id, VL_ACCESS_R, NULL);
            if (rc == 0) {
                rc = vworm_read(header + 4) >> 8;
            }
            else {
                rc = -2;
            }
        }
    }
    
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
        ///@todo THIS LINE ONLY FOR DEBUG
        fprintf(stderr, "cmd_restore():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n  write_bytes=%d\n", 
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi, arglist.filedata_size);
                
        if (arglist.devid != 0) {
            rc = 256 * otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
        }
        if (rc == 0) {
            vaddr header;
            
            /// The write operation for OTDB is a direct access to RAM, once
            /// getting the hardware address of the data element.  This works
            /// when the data is stored in RAM, which is always the case for 
            /// OTDB even though it is often not the case for embedded OTFS 
            /// implementations.
            ///
            ///@note OTDB works entirely as the root user (NULL user_id)
            rc = vl_getheader_vaddr(&header, arglist.block_id, arglist.file_id, VL_ACCESS_W, NULL);
            if (rc == 0) {
                vlFILE*     fp;
                uint8_t*    dptr;
                int         span;
                
                fp = vl_open_file(header);
                if (fp == NULL) {
                    rc = 0xFF;
                    goto cmd_write_END;
                }
                
                dptr = vl_memptr(fp);
                if (dptr == NULL) {
                    rc = 0xFF;
                    goto cmd_write_END;
                }
                
                if (arglist.range_lo >= fp->alloc) {
                    rc = 7;
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
        }
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
            ///@todo THIS LINE ONLY FOR DEBUG
            fprintf(stderr, "cmd_restore():\n  device_id=%016llX\n  block=%d\n  file_id=%d\n  write_perms=%0o\n", 
                    arglist.devid, arglist.block_id, arglist.file_id, arglist.file_perms);
                    
            if (arglist.devid != 0) {
                rc = 256 * otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            }
            if (rc == 0) {
                /// Run the chmod and return the error code (0 is no error)
                /// The error code from OTFS is positive.
                ///@note OTDB works entirely as the root user (NULL user_id)
                rc  = vl_chmod((vlBLOCK)arglist.block_id, 
                                (uint8_t)arglist.file_id, 
                                (uint8_t)arglist.file_perms, 
                                NULL    );
            }
        }
    }
    
    return rc;
}








