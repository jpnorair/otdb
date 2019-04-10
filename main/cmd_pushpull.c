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
#include "dm_printf.h"
#include "dterm.h"
#include "cliopt.h"
#include "iterator.h"
#include "otdb_cfg.h"
#include "debug.h"

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





#if 0 //OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif





static vl_header_t* sub_resolveblock(int* num_files, const char** blkstr, cmd_arglist_t* arglist, otfs_t* devfs) {
    static const char* arg_bgfb = "gfb";
    static const char* arg_biss = "iss";
    static const char* arg_bisf = "isf";

    vl_header_t* fhdr;
    vlFSHEADER* fshdr = devfs->base;
    vlBLOCKHEADER* fsblk;
    int tab_offset;
    
    switch (arglist->block_id) {
        // GFB
        case VL_GFB_BLOCKID:
            tab_offset  = 0;
            fsblk       = &fshdr->gfb;
            *blkstr     = arg_bgfb;
            break;
        
        // ISS
        case VL_ISS_BLOCKID:
            tab_offset  = 0 + fshdr->gfb.files;
            fsblk       = &fshdr->iss;
            *blkstr     = arg_biss;
            break;
        
        // ISF0
        case VL_ISF_BLOCKID:
       default:
            arglist->block_id = VL_ISF_BLOCKID;
            tab_offset  = 0 + fshdr->gfb.files + fshdr->iss.files;
            fsblk       = &fshdr->isf;
            *blkstr     = arg_bisf;
            break;
        
        // ISF1-4:
        ///@todo not yet supported
    }
    if (fsblk != NULL) {
        *num_files  = fsblk->used;
        fhdr        = devfs->base + sizeof(vlFSHEADER) + (sizeof(vl_header_t)*tab_offset);
    }
    else {
        fhdr = NULL;
    }
    
    return fhdr;
}




static int push_action(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t** srcp, size_t dstmax,
                        int index, cmd_arglist_t* arglist, otfs_t* devfs) {
    const char* arg_b;
    
    int rc = 0;
    int dstlim;
    vl_header_t* fhdr;
    int wrbytes;
    int touched=0;
    int num_files=-1;
    
    char outbuf[513];
    
    /// 1. Do input check on devfs, which is provided by the iterator function.
    ///    The other arguments are passed-though from the caller, so it is up
    ///    to the caller to make sure they aren't out-of-bounds.
    if (devfs == NULL)          return -1;
    if (devfs->base == NULL)    return -1;
    
    /// 2. Get file information from the fs header.
    ///    This does some low-level operations on the veelite binary image.
    fhdr = sub_resolveblock(&num_files, &arg_b, arglist, devfs);
    if (fhdr != NULL) {
        /// 3. Write each file to the target.  DO NOT write to files that require
        ///    root access.
        dstlim = (int)dstmax;
        for (int i=0; i<num_files; i++) {
            AUTH_level minauth;
            ot_uni16 idmod;
            vlFILE* fp;
            uint8_t* dptr;
            
            // File Access Check: no root operation allowed
            idmod.ushort = fhdr[i].idmod;
            if ((idmod.ubyte[1] & 0x12) == 0) {
                continue;
            }
            
            // Open File pointer and check also that memptr call worked
            fp = vl_open(arglist->block_id, idmod.ubyte[0], VL_ACCESS_SU, NULL);
            if (fp != NULL) {
                dptr    = vl_memptr(fp);
                wrbytes = cmd_hexnwrite(outbuf, dptr, fp->length, 512);
                minauth = (idmod.ubyte[1] & 0x02) ? AUTH_guest : AUTH_user;
                wrbytes = dm_xnprintf(dth, dst, dstmax, minauth, devfs->uid.u64, "file w -b %s %i [%s]", arg_b, idmod.ubyte[0], outbuf);
                touched+= (wrbytes > 0);
                vl_close(fp);
            }
        }
    }
        
    push_action_END:
    /// 4. Add results to output manifest
    ///@todo add hex output option
    if (arglist->jsonout_flag) {
        rc = snprintf((char*)dst, dstmax, "{\"devid\":\"%"PRIx64"\", \"block\":%i, \"files\":%i, \"touched\":%i},",
                        devfs->uid.u64, arglist->block_id, num_files, touched);
    }
    else {
        rc = snprintf((char*)dst, dstmax, "devid:%"PRIx64", block:%i, files:%i, touched:%i\n",
                        devfs->uid.u64, arglist->block_id, num_files, touched);
    }
    
    return rc;
}





static int pull_action(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t** srcp, size_t dstmax,
                        int index, cmd_arglist_t* arglist, otfs_t* devfs) {
    const char* arg_b;
    
    int rc = 0;
    int dstlim;
    vl_header_t* fhdr;
    int touched=0;
    int num_files=-1;
    int i;
    
    /// 1. Do input check on devfs, which is provided by the iterator function.
    ///    The other arguments are passed-though from the caller, so it is up
    ///    to the caller to make sure they aren't out-of-bounds.
    if (devfs == NULL)          return -1;
    if (devfs->base == NULL)    return -1;
    
    /// 2. Get file information from the fs header.
    ///    This does some low-level operations on the veelite binary image.
    fhdr = sub_resolveblock(&num_files, &arg_b, arglist, devfs);
    if (fhdr != NULL) {
        /// 3. Read each file from the target.  Use Root if necessary.
        dstlim = (int)dstmax;
        for (i=0; i<num_files; i++) {
            AUTH_level minauth;
            vlFILE* fp;
            int wrbytes;
            
            // Open File pointer and check also that memptr call worked
            fp = vl_open(arglist->block_id, i, VL_ACCESS_R, NULL);
            if (fp != NULL) {
                ot_int binary_bytes;
                minauth = cmd_minauth_get(fp, VL_ACCESS_R);
                
                wrbytes = dm_xnprintf(dth, dst, dstmax, minauth, devfs->uid.u64, "file r -b %s %i", arg_b, i);
                if (wrbytes >= 0) {
                    vl_close(fp);
                    break;
                }
                
                pull_action_WRITESUCCESS:
                touched++;
                binary_bytes = cmd_hexread(dst, (char*)dst);
                    
                ///@todo the +9,-9 is a hack to bypass the alp & file read headers.
                ///@todo Verify the ALP ID of the return message
                ///@todo Verify the alignment of the file read vs. local file
                vl_store(fp, binary_bytes-9, dst+9);
                vl_close(fp);
            }
        }
    }
    
    pull_action_END:
    /// 4. Add results to output manifest
    ///@todo add hex output option
    if (arglist->jsonout_flag) {
        rc = snprintf((char*)dst, dstmax, "{\"devid\":\"%"PRIx64"\", \"block\":%i, \"files\":%i, \"touched\":%i},",
                        devfs->uid.u64, arglist->block_id, num_files, touched);
    }
    else {
        rc = snprintf((char*)dst, dstmax, "devid:%"PRIx64", block:%i, files:%i, touched:%i\n",
                        devfs->uid.u64, arglist->block_id, num_files, touched);
    }
    
    return rc;
}



///@todo for some reason, this doesn't work.  Many fps get returned as NULL, inexplicably
/*
static int pull_action(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t** srcp, size_t dstmax,
                        int index, cmd_arglist_t* arglist, otfs_t* devfs) {
    const char* arg_b;
 
    int rc = 0;
    int dstlim;
    vl_header_t* fhdr;
    int touched=0;
    int num_files=-1;
    int num_retries = 2;
 
    /// 1. Do input check on devfs, which is provided by the iterator function.
    ///    The other arguments are passed-though from the caller, so it is up
    ///    to the caller to make sure they aren't out-of-bounds.
    if (devfs == NULL)          return -1;
    if (devfs->base == NULL)    return -1;
 
    /// 2. Get file information from the fs header.
    ///    This does some low-level operations on the veelite binary image.
    fhdr = sub_resolveblock(&num_files, &arg_b, arglist, devfs);
    if (fhdr != NULL) {
        /// 3. Read each file from the target.  Use Root if necessary.
        dstlim = (int)dstmax;
 
        ///@todo move retries functionality to devmgr command
        for (int i=0, retries=num_retries; i<num_files; i++) {
            AUTH_level minauth;
            vlFILE* fp;
            int wrbytes;
 
            // If file cannot be opened, skip it.
            fp = vl_open(arglist->block_id, i, VL_ACCESS_R, NULL);
            if (fp == NULL) {
                continue;
            }
 
            // If file allocation (in template) is zero, then skip it
            if (fp->alloc == 0) {
                touched++;
                continue;
            }
 
            minauth = cmd_minauth_get(fp, VL_ACCESS_R);
            wrbytes = dm_xnprintf(dth, dst, dstmax, minauth, devfs->uid.u64, "file r -b %s %i", arg_b, i);
            if (wrbytes <= 0) {
                retries--;
                // retries have expired.  Move to next file.
                // If retrying, unincrement file counter, try again.
                if (retries < 0) {
                    retries = num_retries;
                }
                else {
                    i--;
                }
            }
            else {
                ot_int binary_bytes;
                touched++;
                binary_bytes = cmd_hexread(dst, (char*)dst);
 
                ///@todo the +5,-5 is a hack to bypass the file read header.  Unhackify it.
                vl_store(fp, binary_bytes-5, dst+5);
 
                // Success.  Reset retry counter
                retries = num_retries;
            }
 
            vl_close(fp);
        }
    }
 
    pull_action_END:
    /// 4. Add results to output manifest
    ///@todo add hex output option
    if (arglist->jsonout_flag) {
        rc = snprintf((char*)dst, dstmax, "{\"devid\":\"%"PRIx64"\", \"block\":%i, \"files\":%i, \"touched\":%i},",
                        devfs->uid.u64, arglist->block_id, num_files, touched);
    }
    else {
        rc = snprintf((char*)dst, dstmax, "devid:%"PRIx64", block:%i, files:%i, touched:%i\n",
                        devfs->uid.u64, arglist->block_id, num_files, touched);
    }
 
    return rc;
}
*/



static int sub_testbuffer(int segbytes, int dstlimit) {
    if (dstlimit < 0) {
        return -4;
    }
    if (segbytes < 0) {
        return -3;
    }
    return 0;
}




static int sub_pushpull(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax, const char* cmdname, iteraction_t action) {
    int rc;
    uint8_t* dstcurs;
    int dstlimit;
    int newchars;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_BLOCKID | ARGFIELD_DEVICEIDLIST,
    };
    void* args[] = {help_man, jsonout_opt, fileblock_opt, devidlist_opt, end_man};
    
    /// Parameter checks
    if ((dth->ext->tmpl == NULL) || (dth->ext->db == NULL)) {
        return -1;
    }
    if (dstmax == 0) {
        return 0;
    }
    
    /// Set cursors and limits. -1 on dstlimit accounts for null string terminator
    dstcurs     = dst;
    dstlimit    = (int)dstmax - 1;
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, cmdname, (const char*)src, inbytes);
    if (rc != 0) {
        rc = -2;
        goto sub_pushpull_END;
    }
    
    /// Preliminary writeout
    ///@todo this could be a cmd library function
    if (arglist.jsonout_flag) {
        dstlimit   -= 2;  // -2 accounts for JSON termination ("]}")
        newchars    = snprintf((char*)dstcurs, dstlimit, "{\"cmd\":\"%s\", \"fbatch\":[", cmdname);
        dstcurs    += newchars;
        dstlimit   -= newchars;
        rc          = sub_testbuffer(newchars, dstlimit);
        if (rc < 0) {
            goto sub_pushpull_END;
        }
    }
    
    rc = iterator_uids(dth, dstcurs, inbytes, &src, (size_t)dstlimit, &arglist, action);
    if (rc > 0) {
        dstcurs += rc-1;    //-1 to eat last comma
    }
    
    sub_pushpull_END:
    if (arglist.jsonout_flag) {
        if (rc < 0) {
            rc = cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, cmdname);
        }
        else {
            dstcurs+= sprintf((char*)dstcurs, "]}");
            rc      = (int)(dstcurs - dst);
        }
    }
    
    return rc;
}




int cmd_push(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return sub_pushpull(dth, dst, inbytes, src, dstmax, "push", &push_action);
}


int cmd_pull(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return sub_pushpull(dth, dst, inbytes, src, dstmax, "pull", &pull_action);
}

