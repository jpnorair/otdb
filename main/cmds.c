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
#include <hbdp/hb_cmdtools.h>

// Standard C & POSIX Libraries
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>



/// Variables used across shell commands
#define HOME_PATH_MAX           256
char home_path[HOME_PATH_MAX]   = "~/";
int home_path_len               = 2;


static uint64_t def_id;

// used by DB manipulation commands
struct arg_str*     devid_man;
struct arg_file*    archive_man;
struct arg_lit*     compress_opt;

// used by file commands
struct arg_str*     devid_opt;
struct arg_str*     fileblock_opt;
struct arg_str*     filerange_opt;
struct arg_int*     fileid_man;
struct arg_int*     fileperms_man;
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
    size_t          filedata_max;
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
    fileperms_man   = arg_int1(NULL,NULL,"Perms",       "Octal pair of User & Guest perms");
    fileperms_man   = arg_int1(NULL,NULL,"Alloc",       "Allocation bytes for file");
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









static void sub_extract_args(cmd_arglist_t* data, void* args, const char* cmdname, const char* src, int* src_bytes) {
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
            data->devid = strtoll( devid_man->sval[0], , 10);
        }
        else {
            out_val = -4;
            goto sub_extract_args_END;
        }
    }
    
    /// Archive Path
    if (data->fields & ARGFIELD_ARCHIVE) {
        if (archive_man->count > 0) {
            data->archive_path = archive_man->filename[0];
        }
        else {
            out_val = -4;
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
            DEBUGPRINT("Block flag encountered: %s\n", fileblock_opt->sval[0]);
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
            DEBUGPRINT("Range flag encountered: %s\n", filerange_opt->sval[0]);
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


    

}











int cmd_quit(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    
    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dt == NULL) {
        return 0;
    }
    INPUT_SANITIZE();
    
    raise(SIGINT);
    return 0;
}


extern cmdtab_t* otdb_cmdtab;
int cmd_cmdlist(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {

    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dt == NULL) {
        return 0;
    }
    INPUT_SANITIZE();
    
    return cmdtab_list(otdb_cmdtab, (char*)dst, dstmax);
}










int cmd_devnew(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, archive_man, end_man};
    
    
    return 0;
}


int cmd_devdel(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID,
    };
    void* args[] = {help_man, devid_man, end_man};
    
    return 0;
}


int cmd_devset(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID,
    };
    void* args[] = {help_man, devid_man, end_man};
    
    //int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
    return 0;
}

int cmd_open(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
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
    
    
    
    return 0;
}

int cmd_save(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_COMPRESS | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, compress_opt, archive_man, end_man};
    
    return 0;
}













int cmd_del(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, end_man};
    
    
    //int otfs_del(void* handle, const otfs_t* fs, bool unload);
    
    return 0;
}

int cmd_new(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID | ARGFIELD_FILEPERMS | ARGFIELD_FILEALLOC,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, fileperms_man, filealloc_man, end_man};
    
    /// 1. Load FS template and defaults (JSON).  
    /// 2. Generate the binary from the JSON.
    /// 3. Create a new filesystem instance with this binary.
    
    /// If no files are provided, use libotfs defaults
    //int otfs_load_defaults(void* handle, otfs_t* fs, size_t maxalloc);
    
    //int otfs_new(void* handle, const otfs_t* fs);
    return 0;
}

int cmd_read(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};

    /// Set FS if one is provided
    //int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
    
    /// Read Data out from the file as needed (using veelite routines, copy from alp_filedata.c or utilize it).

    return 0;
}

int cmd_readall(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    
    /// Set FS if one is provided
    //int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
    
    /// Read Data out from the file as needed (using veelite routines, copy from alp_filedata.c or utilize it).
    
    return 0;
}

int cmd_restore(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, end_man};
    
    return 0;
}

int cmd_readhdr(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, end_man};
    
    return 0;
}

int cmd_readperms(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    
    return 0;
}

int cmd_write(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID | ARGFIELD_FILEDATA,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, filerange_opt, fileid_man, filedata_man, end_man};
    
    return 0;
}

int cmd_writeperms(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_BLOCKID | ARGFIELD_FILEID | ARGFIELD_FILEPERMS,
    };
    void* args[] = {help_man, devid_opt, fileblock_opt, fileid_man, fileperms_man, end_man};
    
    return 0;
}







