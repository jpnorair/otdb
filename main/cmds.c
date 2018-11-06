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
#include <fts.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


/// Variables used across shell commands
#define HOME_PATH_MAX           256
char home_path[HOME_PATH_MAX]   = "~/";
int home_path_len               = 2;



// used by DB manipulation commands
struct arg_str*     devid_man;
struct arg_file*    archive_man;
struct arg_lit*     compress_opt;
struct arg_lit*     jsonout_opt;

// used by file commands
struct arg_str*     devid_opt;
struct arg_str*     devidlist_opt;
struct arg_int*     fileage_opt;
struct arg_str*     fileblock_opt;
struct arg_str*     filerange_opt;
struct arg_int*     fileid_man;
struct arg_str*     fileperms_man;
struct arg_int*     filealloc_man;
struct arg_str*     filedata_man;

// used by all commands
struct arg_lit*     help_man;
struct arg_end*     end_man;




#define INPUT_SANITIZE() do { \
    if ((src == NULL) || (dst == NULL)) {   \
        *inbytes = 0;                       \
        return -1;                          \
    }                                       \
} while(0)

#if 0 //OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif







void cmd_init_args(void) {
    /// Initialize the argtable structs
    devid_man       = arg_str1(NULL,NULL,"DeviceID",    "Device ID as HEX");
    archive_man     = arg_file1(NULL,NULL,"file",       "Archive file or directory");
    jsonout_opt     = arg_lit0("j","json",              "Use JSON as output");
    compress_opt    = arg_lit0("c","compress",          "Use compression on output (7z)");
    devidlist_opt   = arg_strn(NULL,NULL,"DeviceID List", 0, 256, "Batch of up to 256 Device IDs");
    devid_opt       = arg_str0("i","id","DeviceID",     "Device ID as HEX");
    fileage_opt     = arg_int0("a","age","ms",          "Maximum age of file, in ms. Default:-1 (infinity).");
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




void cmd_printhex() {

}



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

int cmd_hexread(uint8_t* dst, const char* src) {
    uint8_t* start;
    
    if ((dst == NULL) || (src == NULL)) {
        return 0;
    }
    
    start = dst;
    while (src[0] && src[1]) {
        uint8_t byte;
        byte    = hexlut0[ *src++ & 0x7f];
        byte   += hexlut1[ *src++ & 0x7f];
        *dst++  = byte;
    }
    return (int)(dst - start);
}

int cmd_hexnread(uint8_t* dst, const char* src, size_t dst_max) {
    uint8_t* start = dst;
    
    if ((dst == NULL) || (src == NULL)) {
        return 0;
    }
    
    start = dst;
    while (src[0] && src[1] && dst_max) {
        uint8_t byte;
        byte    = hexlut0[ *src++ & 0x7f];
        byte   += hexlut1[ *src++ & 0x7f];
        *dst++  = byte;
        dst_max--;
    }
    return (int)(dst - start);
}



static inline char* sub_hexwrite(char* dst, const uint8_t input) {
    static const char convert[] = "0123456789ABCDEF";
    dst[0]  = convert[input >> 4];
    dst[1]  = convert[input & 0x0f];
    return dst;
}


int cmd_hexwrite(char* dst, const uint8_t* src, size_t src_bytes) {
    char* start = dst;
    while (src_bytes != 0) {
        src_bytes--;
        sub_hexwrite(dst, *src++);
        dst += 2;
    }
    *dst = 0;
    return (int)(dst - start);
}

int cmd_hexnwrite(char* dst, const uint8_t* src, size_t src_bytes, size_t dst_max) {
    char* end = &dst[dst_max-1];
    char* start = dst;
    while ((src_bytes != 0) && (dst < end)) {
        src_bytes--;
        sub_hexwrite(dst, *src++);
        dst += 2;
    }
    *dst = 0;
    return (int)(dst - start);
}




int cmd_jsonout_err(char* dst, size_t dstmax, bool jsonflag, int errcode, const char* cmdname) {
    if (jsonflag) {
        errcode = snprintf(dst, dstmax-1, "{\"type\":\"otdb\", \"cmd\":\"%s\", \"err\":%d}", cmdname, errcode);
    }
    return errcode;
}


int cmd_jsonout_fmt(char** dst, size_t* dstmax, bool jsonflag, int errcode, const char* cmdname, const char* fmt, ...) {
    va_list args;

    if (jsonflag) {
        int psize;
        va_start(args, fmt);
        psize    = vsnprintf(*dst, *dstmax-1, fmt, args);
        va_end(args);
        *dst    += psize;
        *dstmax -= psize;
        errcode  = psize;
    }
    
    return errcode;
}


int cmd_jsonout_data(char** dst, size_t* dstmax, bool jsonflag, int errcode, uint8_t* src, uint16_t offset, size_t srcbytes) {
    if (jsonflag) {
        int psize;
        if (srcbytes <= 0) {
            psize       = snprintf(*dst, *dstmax-1, "}");
        }
        else {
            psize       = snprintf(*dst, *dstmax-1, ", \"d_offset\":%i", offset);
            *dstmax    -= psize;
            *dst       += psize;
            errcode    += psize;
            psize       = snprintf(*dst, *dstmax-1, ", \"d_size\":%zu", srcbytes);
            *dstmax    -= psize;
            *dst       += psize;
            errcode    += psize;
            psize       = snprintf(*dst, *dstmax-1, ", \"d_hex\":\"");
            *dstmax    -= psize;
            *dst       += psize;
            errcode    += psize;
            psize       = cmd_hexnwrite(*dst, src, srcbytes, *dstmax-1);
            *dstmax    -= psize;
            *dst       += psize;
            errcode    += psize;
            psize       = snprintf(*dst, *dstmax-1, "\"}");
        }
        *dst       += psize;
        *dstmax    -= psize;
        errcode    += psize;
    }
    return errcode;
}



int cmd_extract_args(cmd_arglist_t* data, void* args, const char* cmdname, const char* src, int* src_bytes) {
    int     out_val;
    int     argc;
    char**  argv;
    int     nerrors;
    
    /// First, create an argument vector from the input string.
    /// hb_tools_parsestring will treat all bintex containers as whitespace-safe.
    argc = hb_tools_parsestring(&argv, cmdname, (char*)src, (char*)src, (size_t)*src_bytes);  

    nerrors = arg_parse(argc, argv, args);
  
    /// Print command specific help
    /// @todo this is currently just generic help
    if (help_man->count > 0) {
        fprintf(stderr, "Usage: %s", cmdname);
        arg_print_syntax(stderr, args, "\n");
        arg_print_glossary(stderr, args, "  %-25s %s\n");
        out_val = -1;
        goto sub_extract_args_END;
    }
 
    /// Print errors, if errors exist
    if (nerrors > 0) {
        char printbuf[32];
        snprintf(printbuf, sizeof(printbuf), "%s", cmdname);
        arg_print_errors(stderr, end_man, printbuf);
        out_val = -3;
        goto sub_extract_args_END;
    }
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
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
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
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
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    /// JSON-out Flag
    if (data->fields & ARGFIELD_JSONOUT) {
        data->jsonout_flag = (jsonout_opt->count > 0);
    }
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    /// Compression Flag
    if (data->fields & ARGFIELD_COMPRESS) {
        data->compress_flag = (compress_opt->count > 0);
    }
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    /// List of Device IDs
    if (data->fields & ARGFIELD_DEVICEIDLIST) {
        data->devid_strlist_size    = devidlist_opt->count;
        data->devid_strlist         = devidlist_opt->sval;
    }
    //fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
    /// Check for Age flag (-a, --age), which specifies maximum file
    /// modification/access delta from present time.
    if (data->fields & ARGFIELD_AGEMS) {
        data->age_ms = -1;      // default: Infinity = -1
        if (fileage_opt->count > 0) {
            DEBUGPRINT("Age arg encountered: %i\n", fileblock_opt->sval[0]);
            data->age_ms = fileage_opt->ival[0];
        else {
            out_val = -7;
            goto sub_extract_args_END;
        }
    }
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    /// Check for block flag (-b, --block), which specifies fs block
    /// Default is isf.
    if (data->fields & ARGFIELD_BLOCKID) {
        data->block_id = VL_ISF_BLOCKID;      // default: isf
        if (fileblock_opt->count > 0) {
            DEBUGPRINT("Block arg encountered: %s\n", fileblock_opt->sval[0]);
            if (strncmp(fileblock_opt->sval[0], "isf", 3) == 0) {
                data->block_id = VL_ISF_BLOCKID;
            }
            else if (strncmp(fileblock_opt->sval[0], "iss", 3) == 0) {
                data->block_id = VL_ISS_BLOCKID;
            }
            else if (strncmp(fileblock_opt->sval[0], "gfb", 3) == 0) {
                data->block_id = VL_GFB_BLOCKID;
            }
            else {
                out_val = -8;
                goto sub_extract_args_END;
            }
        }
    }
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);     
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
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);     
    /// File ID is simply copied from the args
    if (data->fields & ARGFIELD_FILEID) {
        if (fileid_man->count > 0) {
            DEBUGPRINT("ID arg encountered: %d\n", fileid_man->ival[0]);
            data->file_id = (uint8_t)(fileid_man->ival[0] & 255);
        }
        else {
            out_val = -9;
            goto sub_extract_args_END;
        }
    }
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);     
    /// File ID is simply copied from the args
    if (data->fields & ARGFIELD_FILEPERMS) {
        if (fileperms_man->count > 0) {
            DEBUGPRINT("Permissions arg encountered: %s\n", fileperms_man->sval[0]);
            data->file_perms = (uint8_t)(63 & strtoul(fileperms_man->sval[0], NULL, 8));
        }
        else {
            out_val = -10;
            goto sub_extract_args_END;
        }
    }
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);     
    /// File ID is simply copied from the args
    if (data->fields & ARGFIELD_FILEALLOC) {
        if (filealloc_man->count > 0) {
            DEBUGPRINT("Alloc arg encountered: %d\n", fileid_man->ival[0]);
            data->file_id = (uint8_t)(255 & fileid_man->ival[0]);
        }
        else {
            out_val = -11;
            goto sub_extract_args_END;
        }
    }
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);     
    /// Filedata field is converted from bintex and stored to data->filedata
    if (data->fields & ARGFIELD_FILEDATA) {
        DEBUGPRINT("DataCount=%i, Filedata=%016llX\n", filedata_man->count, (uint64_t)data->filedata);
    
        if ((filedata_man->count > 0) && (data->filedata != NULL)) {
            DEBUGPRINT("Filedata arg encountered: %s\n", filedata_man->sval[0]);
            data->filedata_size = bintex_ss((unsigned char*)filedata_man->sval[0], 
                                            (unsigned char*)data->filedata, 
                                            data->filedata_size             );
        }
        else {
            out_val = -12;
            goto sub_extract_args_END;
        }
    }
//fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__); 
    /// Done!  Return 0 for success.
    out_val = 0;

    sub_extract_args_END:
    hb_tools_freeargv(argv);
    return out_val;
}




int cmd_rmdir(const char *dir) {
    int ret = 0;
    FTS *ftsp = NULL;
    FTSENT *curr;

    // Cast needed (in C) because fts_open() takes a "char * const *", instead
    // of a "const char * const *", which is only allowed in C++. fts_open()
    // does not modify the argument.
    char *files[] = { (char *) dir, NULL };

    // FTS_NOCHDIR  - Avoid changing cwd, which could cause unexpected behavior
    //                in multithreaded programs
    // FTS_PHYSICAL - Don't follow symlinks. Prevents deletion of files outside
    //                of the specified directory
    // FTS_XDEV     - Don't cross filesystem boundaries
    ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
    if (!ftsp) {
        //fprintf(stderr, "%s: fts_open failed: %s\n", dir, strerror(errno));
        ret = -1;
        goto cmd_rmdir_FINISH;
    }

    while ((curr = fts_read(ftsp))) {
        switch (curr->fts_info) {
        case FTS_NS:
        case FTS_DNR:
        case FTS_ERR:
            //fprintf(stderr, "%s: fts_read error: %s\n",
            //        curr->fts_accpath, strerror(curr->fts_errno));
            break;

        case FTS_DC:
        case FTS_DOT:
        case FTS_NSOK:
            // Not reached unless FTS_LOGICAL, FTS_SEEDOT, or FTS_NOSTAT were
            // passed to fts_open()
            break;

        case FTS_D:
            // Do nothing. Need depth-first search, so directories are deleted
            // in FTS_DP
            break;

        case FTS_DP:
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            if (remove(curr->fts_accpath) < 0) {
                //fprintf(stderr, "%s: Failed to remove: %s\n",
                //        curr->fts_path, strerror(errno));
                ret = -1;
            }
            break;
        }
    }

    cmd_rmdir_FINISH:
    if (ftsp) {
        fts_close(ftsp);
    }

    return ret;
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










