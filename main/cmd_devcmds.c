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

#if OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif




/** OTDB DB Manipulation Commands
  * -------------------------------------------------------------------------
  */

int cmd_devnew(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_JSONOUT | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, jsonout_opt, devid_man, archive_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "dev-new", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUGPRINT("cmd_devnew():\n  archive=%s\n", arglist.archive_path);
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, NULL, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_devnew_END;
            }
        }
        
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

    cmd_devnew_END:
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "dev-new");
}



int cmd_devdel(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields =  ARGFIELD_JSONOUT | ARGFIELD_DEVICEID,
    };
    void* args[] = {help_man, jsonout_opt, devid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "dev-del", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUGPRINT("cmd_devdel():\n  device_id=%016"PRIx64"\n", arglist.devid);
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, NULL, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_devdel_END;
            }
        }
        
        ///@todo delete the file!
    }

    cmd_devdel_END:
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "dev-del");
}



int cmd_devset(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields =  ARGFIELD_JSONOUT | ARGFIELD_DEVICEID,
    };
    void* args[] = {help_man, jsonout_opt, devid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "dev-set", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUGPRINT("cmd_devset():\n  device_id=%016"PRIx64"\n", arglist.devid);
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, NULL, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
            }
        }
    }

    cmd_devset_END:
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "dev-set");
}









