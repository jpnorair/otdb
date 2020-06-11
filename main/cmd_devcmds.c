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
#include "debug.h"

// HB Headers/Libraries
#include <bintex.h>
#include <cmdtab.h>
#include <argtable3.h>
#include <cJSON.h>
#include <otfs.h>

// Standard C & POSIX Libraries
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
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

#if OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif



static void sub_tfree(void* ctx) {
    talloc_free(ctx);
}



/** OTDB DB Manipulation Commands
  * -------------------------------------------------------------------------
  */

int cmd_devnew(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    otfs_t newfs;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEID | ARGFIELD_JSONOUT | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, jsonout_opt, devid_man, archive_man, end_man};
    
    /// Only works with an open database
    if (dth->ext->tmpl_fs == NULL) {
        rc = -1;
        goto cmd_devnew_END;
    }
    
    /// Extract arguments into arglist struct
    /// On successful extraction, create a new device in the database
    rc = cmd_extract_args(&arglist, args, "dev-new", (const char*)src, inbytes);
    if (rc != 0) {
        goto cmd_devnew_END;
    }
    DEBUGPRINT("cmd_devnew():\n  archive=%s\n", arglist.archive_path);
    
    /// Make sure the device doesn't already exist
    if (arglist.devid != 0) {
        rc = otfs_setfs(dth->ext->db, NULL, (uint8_t*)&arglist.devid);
        if (rc == 0) {
            rc = ERRCODE(otfs, otfs_setfs, rc);
            goto cmd_devnew_END;
        }
    }
    
    /// Create the new device FS in the DB
    
    /// Check if pathbuf is set to "NULL" which uses default data
    if (strcmp(arglist.archive_path, "NULL") == 0) {
        newfs.uid.u64   = arglist.devid;
        newfs.alloc     = ((otfs_t*)dth->ext->tmpl_fs)->alloc;
        newfs.base      = talloc_size(dth->pctx, newfs.alloc);
        if (newfs.base == NULL) {
            ///@todo error casting
            rc = -2;
            goto cmd_devnew_END;
        }
        
        memcpy(newfs.base, ((otfs_t*)dth->ext->tmpl_fs)->base, newfs.alloc);
        rc = otfs_new(dth->ext->db, &newfs);
        if (rc != 0) {
            talloc_free(newfs.base);
            rc = ERRCODE(otfs, otfs_new, rc);
            goto cmd_devnew_END;
        }
    }
    else {
        struct stat st;
        rc = stat(arglist.archive_path, &st);
        if (rc != 0) {
            rc = -3;
            goto cmd_devnew_END;
        }
        if (!S_ISDIR(st.st_mode)) {
            rc = -4;
            goto cmd_devnew_END;
        }
        rc = cmdsub_datafile(dth, dst, dstmax, dth->ext->tmpl, NULL, arglist.archive_path, arglist.devid, false);
    }

    cmd_devnew_END:
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "dev-new");
}



int cmd_devdel(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    otfs_t delfs;
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
            rc = otfs_setfs(dth->ext->db, &delfs, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = ERRCODE(otfs, otfs_setfs, rc);
                goto cmd_devdel_END;
            }
            
            ///@todo delete the file!
            rc = otfs_del(dth->ext->db, &delfs, &sub_tfree);
            if (rc != 0) {
                rc = ERRCODE(otfs, otfs_del, rc);
                goto cmd_devdel_END;
            }
        }
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
            rc = otfs_setfs(dth->ext->db, NULL, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
            }
        }
    }

    cmd_devset_END:
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "dev-set");
}









