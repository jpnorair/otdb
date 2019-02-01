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


static int sub_nextdevice(void* handle, uint8_t* uid, int* devid_i, const char** strlist, size_t listsz) {
    int devtest = 1;

    for (; (devtest!=0) && (*devid_i<listsz); (*devid_i)++) {
        DEBUGPRINT("%s %d :: devid[%i] = %s\n", __FUNCTION__, __LINE__, *devid_i, strlist[*devid_i]);
        memset(uid, 0, 8);
        *((uint64_t*)uid) = strtoull(strlist[*devid_i], NULL, 16);
        devtest = otfs_setfs(handle, uid);
    }
    
    return devtest;
}







int cmd_devls(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    char* outcurs;
    int outlimit;
    int newchars;
    
    // Device OTFS
    int devtest;
    int devid_i = 0;
    int count;
    otfs_t* devfs;
    otfs_id_union uid;
    
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDLIST,
    };
    void* args[] = {help_man, jsonout_opt, devidlist_opt, end_man};
    
    ///@todo do input checks!!!!!!
    
    /// Make sure there is something to save.
    if ((dth->tmpl == NULL) || (dth->ext == NULL)) {
        return -1;
    }
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "save", (const char*)src, inbytes);
    if (rc != 0) {
        rc = -2;
        goto cmd_devls_END;
    }
    DEBUGPRINT("cmd_devls()\n");
    
    /// Start formatting of command output.
    outcurs = (char*)dst;
    outlimit = (int)dstmax - 1;     // -1 accounts for null string terminator
    DEBUGPRINT("outcurs=%016llX, newchars=%i, outlimit=%i\n", (uint64_t)outcurs, 0, outlimit);
    
    if (arglist.jsonout_flag) {
        outlimit   -= 2;  // -2 accounts for JSON termination ("]}")
        newchars    = snprintf(outcurs, outlimit, "{\"cmd\":\"dev-ls\", \"idlist\":[");
        outcurs    += newchars;
        outlimit   -= newchars;
        DEBUGPRINT("outcurs=%016llX, newchars=%i, outlimit=%i\n", (uint64_t)outcurs, newchars, outlimit);
        
        if (newchars < 0) {
            rc = -3;        ///@todo change to write error
            goto cmd_devls_END;
        }
        if (outlimit < 0) {
            rc = -4;        ///@todo change to buffer overflow error
            goto cmd_devls_END;
        }
        
    }
    
    ///@todo this iteration pattern is also used in cmd_save(), and perhaps
    /// there's a way to have an iterator function used by all such commands.
    
    /// If there is a list of Device IDs supplied in the command, we use these.
    /// Else, we dump all the devices present in the OTDB.
    if (arglist.devid_strlist_size > 0) {
        devtest = sub_nextdevice(dth->ext, &uid.u8[0], &devid_i, arglist.devid_strlist, arglist.devid_strlist_size);
    }
    else {
        devtest = otfs_iterator_start(dth->ext, &devfs, &uid.u8[0]);
    }

    count = 0;
    while (devtest == 0) {
        // Loop body: put logic in here that needs device IDs from the DB
        // --------------------------------------------------------------------
        count++;
        
        if (arglist.jsonout_flag) {
            newchars = snprintf(outcurs, outlimit, "\"%"PRIx64"\",", uid.u64);
        }
        else {
            newchars = snprintf(outcurs, outlimit, "%i. %"PRIx64"\n", count, uid.u64);
        }
        
        outcurs    += newchars;
        outlimit   -= newchars;
        DEBUGPRINT("outcurs=%016llX, newchars=%i, outlimit=%i\n", (uint64_t)outcurs, newchars, outlimit);
        
        if (newchars < 0) {
            rc = -3;        ///@todo change to write error
            goto cmd_devls_END;
        }
        if (outlimit < 0) {
            rc = -4;        ///@todo change to buffer overflow error
            goto cmd_devls_END;
        }
        
        // --------------------------------------------------------------------
        // End of Loop body: Fetch next device
        if (arglist.devid_strlist_size > 0) {
            devtest = sub_nextdevice(dth->ext, &uid.u8[0], &devid_i, arglist.devid_strlist, arglist.devid_strlist_size);
        }
        else {
            devtest = otfs_iterator_next(dth->ext, &devfs, &uid.u8[0]);
        }
    }
    
    if (arglist.jsonout_flag) {
        if (count > 0) {
            outcurs--;  // eat last comma
        }
        outcurs += sprintf(outcurs, "]}");
        DEBUGPRINT("outcurs=%016llX, newchars=%i, outlimit=%i\n", (uint64_t)outcurs, 2, outlimit-2);
    }
    
    rc = outcurs - (char*)dst;
    
    cmd_devls_END:
    if ((rc < 0) && arglist.jsonout_flag) {
        rc = snprintf((char*)dst, dstmax-1, "{\"type\":\"otdb\", \"cmd\":\"dev-ls\", \"err\":%d}", rc);
    }
    
    return rc;
}



