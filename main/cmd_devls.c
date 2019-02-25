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
#include "iterator.h"
#include "otdb_cfg.h"
#include "json_tools.h"

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
#include <errno.h>
#include <sys/stat.h>


// used by DB manipulation commands
extern struct arg_str*  devid_man;
extern struct arg_file* archive_man;
extern struct arg_lit*  compress_opt;
extern struct arg_lit*  jsonout_opt;

// used by file commands
extern struct arg_str*  devid_opt;
extern struct arg_str*  devidlist_opt;
extern struct arg_int*  fileage_opt;
extern struct arg_str*  fileblock_opt;
extern struct arg_str*  filerange_opt;
extern struct arg_int*  fileid_man;
extern struct arg_str*  fileperms_man;
extern struct arg_int*  filealloc_man;
extern struct arg_str*  filedata_man;

// used by all commands
extern struct arg_lit*  help_man;
extern struct arg_end*  end_man;


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


static int devls_action(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t** srcp, size_t dstmax,
                        int index, cmd_arglist_t* arglist, otfs_t* devfs) {
    int rc;
    
    if (arglist->jsonout_flag) {
        rc = snprintf((char*)dst, dstmax, "\"%"PRIx64"\",", devfs->uid.u64);
    }
    else {
        rc = snprintf((char*)dst, dstmax, "%i. %"PRIx64"\n", index, devfs->uid.u64);
    }
    DEBUGPRINT("UID=\"%"PRIx64"\", dstcurs=%016llX, rc=%i, dstmax=%zu\n", devfs->uid.u64, (uint64_t)dst, rc, dstmax);

    return rc;
}






int cmd_devls(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    uint8_t* dstcurs;
    int dstlimit;
    int newchars;
    
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDLIST,
    };
    void* args[] = {help_man, jsonout_opt, devidlist_opt, end_man};
    
    /// Make sure there is something to save.
    if ((dth->ext->tmpl == NULL) || (dth->ext->db == NULL)) {
        return -1;
    }
    
    if (dstmax == 0) {
        return 0;
    }
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "save", (const char*)src, inbytes);
    if (rc != 0) {
        rc = -2;
        goto cmd_devls_END;
    }
    DEBUGPRINT("cmd_devls()\n");
    
    /// Start formatting of command output.
    dstcurs = dst;
    dstlimit = (int)dstmax - 1;     // -1 accounts for null string terminator
    DEBUGPRINT("dstcurs=%016llX, newchars=%i, dstlimit=%i\n", (uint64_t)dstcurs, 0, dstlimit);
    
    if (arglist.jsonout_flag) {
        dstlimit   -= 2;  // -2 accounts for JSON termination ("]}")
        newchars    = snprintf((char*)dstcurs, dstlimit, "{\"cmd\":\"dev-ls\", \"idlist\":[");
        dstcurs    += newchars;
        dstlimit   -= newchars;
        DEBUGPRINT("dstcurs=%016llX, newchars=%i, dstlimit=%i\n", (uint64_t)dstcurs, newchars, dstlimit);
        
        if (newchars < 0) {
            rc = -3;        ///@todo change to write error
            goto cmd_devls_END;
        }
        if (dstlimit < 0) {
            rc = -4;        ///@todo change to buffer overflow error
            goto cmd_devls_END;
        }
    }
    
    rc = iterator_uids(dth, dstcurs, inbytes, &src, (size_t)dstlimit, &arglist, &devls_action);
    if (rc < 0) {
        goto cmd_devls_END;
    }
    if (rc > 0) {
        dstcurs += rc - 1;      // -1 eats last comma
    }
    if (arglist.jsonout_flag) {
        dstcurs += sprintf((char*)dstcurs, "]}");
        DEBUGPRINT("dstcurs=%016llX, newchars=%i, dstlimit=%i\n", (uint64_t)dstcurs, 2, dstlimit-2);
    }
    
    rc = (int)(dstcurs - dst);
    
    cmd_devls_END:
    if ((rc < 0) && arglist.jsonout_flag) {
        rc = snprintf((char*)dst, dstmax-1, "{\"type\":\"otdb\", \"cmd\":\"dev-ls\", \"err\":%d}", rc);
    }
    
    return rc;
}



