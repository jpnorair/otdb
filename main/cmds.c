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




void cmd_init_args(void) {
    /// Initialize the argtable structs
    devid_man       = arg_str1(NULL,NULL,"DeviceID","Device ID as HEX");
    archive_man     = arg_file1(NULL,NULL,"file",   "Archive file or directory");
    compress_opt    = arg_lit0("c","compress",      "Use compression on output (7z)");
    devid_opt       = arg_str0("i","id","DeviceID", "Device ID as HEX");
    fileblock_opt   = arg_str0("b","block","isf|iss|gf", "File Block to search in");
    filerange_opt   = arg_str0("r","range","X:Y",   "Access File bytes between offsets X:Y");
    fileid_man      = arg_int1(NULL,NULL,"FileID",  "File ID, 0-255.");
    fileperms_man   = arg_int1(NULL,NULL,"Perms",   "Octal pair of User & Guest perms");
    fileperms_man   = arg_int1(NULL,NULL,"Alloc",   "Allocation bytes for file");
    filedata_man    = arg_str1(NULL,NULL,"Bintex",  "File data supplied as Bintex");
    help_man        = arg_lit0("h","help",          "Print this help and exit");
    end_man         = arg_end(10);
}











uint8_t* sub_markstring(uint8_t** psrc, int* search_limit, int string_limit) {
    size_t      code_len;
    size_t      code_max;
    uint8_t*    cursor;
    uint8_t*    front;
    
    /// 1. Set search limit on the string to mark within the source string
    code_max    = (*search_limit < string_limit) ? *search_limit : string_limit; 
    front       = *psrc;
    
    /// 2. Go past whitespace in the front of the source string if there is any.
    ///    This updates the position of the source string itself, so the caller
    ///    must save the position of the source string if it wishes to go back.
    while (isspace(**psrc)) { 
        (*psrc)++; 
    }
    
    /// 3. Put a Null Terminator where whitespace is found after the marked
    ///    string.
    for (code_len=0, cursor=*psrc; (code_len < code_max); code_len++, cursor++) {
        if (isspace(*cursor)) {
            *cursor = 0;
            cursor++;
            break;
        }
    }
    
    /// 4. Go past any whitespace after the cursor position, and update cursor.
    while (isspace(*cursor)) { 
        cursor++; 
    }
    
    /// 5. reduce the message limit counter given the bytes we've gone past.
    *search_limit -= (cursor - front);
    
    return cursor;
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









static void sub_extract_args(void* args, const char* cmdname, const char* src, int* src_bytes) {
    int     out_val;
    int     argc;
    char**  argv;
    
    int nerrors;
    
    int block_id;

/// 3. First, create an argument vector from the input string.
    ///    hb_tools_parsestring will treat all bintex containers as whitespace-safe.

    argc = hb_tools_parsestring(&argv, cmdname, (char*)src, (char*)src, (size_t)*src_bytes);
    if (argc <= 1) {
        out_val = -2;
        goto sub_extract_args_END;
    }
    nerrors = arg_parse(argc, argv, args);
    
    /// 4. Print command specific help
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
    
    /// Check for block flag (-b, --block), which specifies fs block
    /// Default is isf.
    block_id = 3 << 4;      // default: isf
    if (fileblock_opt->count > 0) {
        DEBUGPRINT("Block flag encountered: %s\n", fileblock_opt->sval[0]);
        if (strncmp(fileblock_opt->sval[0], "isf", 3) == 0) {
            block_id = 3 << 4;
        }
        else if (strncmp(fileblock_opt->sval[0], "iss", 3) == 0) {
            block_id = 2 << 4;
        }
        else if (strncmp(fileblock_opt->sval[0], "gfb", 3) == 0) {
            block_id = 1 << 4;
        }
        else {
            out_val = -6;
            goto sub_extract_args_END;
        }
    }
    
    /// File ID must exist if "prot" flag is not used
    /// We want to sanitize the input IDs to make sure they fit inside the rules.
    /// The write command (7) only accepts a single ID
    if (fileid_man->count > 0) {
        DEBUGPRINT("ID arg encountered: %d\n", fileid_man->ival[0]);
        file_id = fileid_man->ival[0];
    }
    else {
        out_val = -7;
        goto sub_extract_args_END;
    }
    
    /// Range is optional.  Default is maximum file range (0:)
    r_start = 0;
    r_end   = 65535;
    if (RANGE_ELEMENT(args)->count > 0) {
        char *start, *end, *ctx;
        DEBUGPRINT("Range flag encountered: %s\n", RANGE_ELEMENT(args)->sval[0]);
        start   = strtok_r((char*)RANGE_ELEMENT(args)->sval[0], ":", &ctx);
        end     = strtok_r(NULL, ":", &ctx);

        if (start != NULL) {
            r_start = (uint16_t)atoi(start);
        }
        if (end != NULL) {
            r_end = (uint16_t)atoi(end);
        }
    }

    /// Load CLI inputs into appropriate output offsets of binary protocol.
    /// Some commands take additional loose data inputs, which get placed into
    /// the binary protocol output after this stage
    for (int i=0; i<id_list_size; i++) {
        if (fdp->bundle.argbytes == 5) {
            dst[4] = (uint8_t)(r_end & 255);
            dst[3] = (uint8_t)(r_end >> 8);
            dst[2] = (uint8_t)(r_start & 255);
            dst[1] = (uint8_t)(r_start >> 8);
        }
        dst[0]  = (uint8_t)id_list[i];
        dst    += fdp->bundle.argbytes;
        dstmax -= fdp->bundle.argbytes;
    }
   
    /// Commands with b11 in the low 2 bits require an additional data parameter
    /// 3 - write perms
    /// 7 - write data          : Special, has an additional data payload
    /// 11 - create empty file
    if ((fdp->bundle.index & 3) == 3) {
        uint8_t datbuffer[256];
        int     datbytes = 0;
        
        if (DATA_ELEMENT(args)->count > 0) {
            DEBUGPRINT("Data: %s\n", DATA_ELEMENT(args)->sval[0]);
            datbytes = sub_bintex_proc( (bool)(FILE_ELEMENT(args)->count > 0), 
                                        DATA_ELEMENT(args)->sval[0], 
                                        (char*)datbuffer, 250 );
        }
        if (datbytes <= 0) {
            out_val = -8;
            goto sub_extract_args_END;
        }
        
        if (fdp->bundle.index == 7) {
            void* dst0;
            
            if (RANGE_ELEMENT(args)->count > 0) {
                if ((r_end - r_start) < datbytes) {
                    datbytes = (r_end - r_start);
                }
            }
            if (datbytes > dstmax) {
                datbytes = (int)dstmax;
            }
            
            r_end       = r_start + (uint16_t)datbytes;
            dst[-1]     = (uint8_t)(r_end & 255);
            dst[-2]     = (uint8_t)(r_end >> 8);
            dstmax     -= datbytes;
            dst0        = (void*)dst;
            dst        += datbytes;
            
            memcpy(dst0, datbuffer, datbytes);
        }
        else {
            uint8_t* p;
            int i, j;
            
            for (i=0, j=0, p=payload; i<id_list_size; ) {
                // payload[0] is always location of file-id, already loaded
                p[1] = datbuffer[j++];
                if (fdp->bundle.index == 11) {
                    p[2] = 0;
                    p[3] = 0;
                    p[4] = datbuffer[j++];
                    p[5] = datbuffer[j++];
                }
                p += fdp->bundle.argbytes;
            }
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
    void* args[] = {devid_man, archive_man, end_man};
    
    
    return 0;
}


int cmd_devdel(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_man, end_man};
    
    return 0;
}


int cmd_devset(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_man, archive_man, end_man};
    
    //int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
    return 0;
}

int cmd_open(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {archive_man, end_man};

    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dt == NULL) {
        return 0;
    }
    INPUT_SANITIZE();
    
    
    
    return 0;
}

int cmd_save(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {compress_opt, archive_man, end_man};
    
    return 0;
}













int cmd_del(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_opt, fileblock_opt, fileid_man, end_man};
    
    
    //int otfs_del(void* handle, const otfs_t* fs, bool unload);
    
    return 0;
}

int cmd_new(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_opt, fileblock_opt, fileid_man, fileperms_man, filealloc_man, end_man};
    
    /// 1. Load FS template and defaults (JSON).  
    /// 2. Generate the binary from the JSON.
    /// 3. Create a new filesystem instance with this binary.
    
    /// If no files are provided, use libotfs defaults
    //int otfs_load_defaults(void* handle, otfs_t* fs, size_t maxalloc);
    
    //int otfs_new(void* handle, const otfs_t* fs);
    return 0;
}

int cmd_read(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};

    /// Set FS if one is provided
    //int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
    
    /// Read Data out from the file as needed (using veelite routines, copy from alp_filedata.c or utilize it).

    return 0;
}

int cmd_readall(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    
    /// Set FS if one is provided
    //int otfs_setfs(void* handle, const uint8_t* eui64_bytes);
    
    /// Read Data out from the file as needed (using veelite routines, copy from alp_filedata.c or utilize it).
    
    return 0;
}

int cmd_restore(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_opt, fileblock_opt, fileid_man, end_man};
    
    return 0;
}

int cmd_readhdr(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_opt, fileblock_opt, fileid_man, end_man};
    
    return 0;
}

int cmd_readperms(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    
    return 0;
}

int cmd_write(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_opt, fileblock_opt, filerange_opt, fileid_man, filedata_man, end_man};
    
    return 0;
}

int cmd_writeperms(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    void* args[] = {devid_opt, fileblock_opt, fileid_man, fileperms_man, end_man};
    
    return 0;
}







