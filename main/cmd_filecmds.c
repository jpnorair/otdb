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





/** OTDB Filesystem Commands
  * -------------------------------------------------------------------------
  */

int cmd_del(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileblock_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "del", (const char*)src, inbytes);
    
    /// On successful extraction, delete a file in the device fs
    if (rc == 0) {
    
        DEBUG_PRINTF("del (delete file cmd):\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n",
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
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "del");
}



int cmd_new(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_BLOCKID | ARGFIELD_FILEID | ARGFIELD_FILEPERMS | ARGFIELD_FILEALLOC,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileblock_opt, fileid_man, fileperms_man, filealloc_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "new", (const char*)src, inbytes);
    
    /// On successful extraction, create a new file in the device fs
    if (rc == 0) {
        vlFILE* fp = NULL;
        
        DEBUG_PRINTF("new (new file cmd):\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n  file_perms=%0o\n  file_alloc=%d\n",
                arglist.devid, arglist.block_id, arglist.file_id, arglist.file_perms, arglist.file_alloc);
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_new_END;
            }
        }
        
        rc = vl_new(&fp, arglist.block_id, arglist.file_id, arglist.file_perms, arglist.file_alloc, NULL);
        vl_close(fp);
        if (rc != 0) {
            rc = -512 - rc;
        }
    }
    
    cmd_new_END:
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "new");
}



int cmd_read(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_AGEMS | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileage_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    uint8_t*    dat_ptr     = NULL;
    int         span        = 0;
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "r", (const char*)src, inbytes);
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        vlFILE* fp;
        
        DEBUG_PRINTF("r (read cmd):\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n",
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
        
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            DEBUG_PRINTF("otfs_setfs() = %i, [id = %016%"PRIx64"]\n", rc, arglist.devid);
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
        
        fp = vl_open_file(header);
        if (fp != NULL) {
            span = arglist.range_hi - arglist.range_lo;
            if (dstmax < span) {
                span = (int)dstmax;
            }
            if ((fp->length-arglist.range_lo) <= 0) {
                span = 0;
            }
            else if ((fp->length-arglist.range_lo) < span) {
                span = (fp->length-arglist.range_lo);
            }
            
            /// Beginning of Read synchronization section --------------------
            /// Check if age parameter is in acceptable range.
            
            ///@todo might need to do some threaded I/O for write & ACK, but maybe not.
            ///@todo this section could be broken-out into its own function
            if ((arglist.age_ms >= 0) && (dth->devmgr != NULL)) {
                uint32_t now        = (uint32_t)time(NULL);
                uint32_t file_age   = now - vl_getmodtime(fp);
                arglist.age_ms      = (arglist.age_ms/1000);
                
                DEBUG_PRINTF("Now: %u, file-age: %u, Age-param: %u\n", now, file_age, arglist.age_ms);
                
                if (file_age > arglist.age_ms) {
                    int cmdbytes;
                    ot_uni16 frlen;
                    cmdbytes = snprintf((char*)dst, dstmax, "file r %u\n", arglist.file_id);
                    cmdbytes = cmd_devmgr(dth, dst, &cmdbytes, dst, dstmax);
                    
                    if (cmdbytes < 0) {
                        rc = cmdbytes;  ///@todo coordinate error codes with debug macros
                        goto cmd_read_CLOSE;
                    }
                    
                    // Convert to binary.
                    // 5 bytes of file header
                    cmdbytes = cmd_hexnread(dst, (const char*)dst, dstmax);
                    if (cmdbytes <= 6) {
                        ///@todo error code for file error
                        rc = -768 - 1;
                        goto cmd_read_CLOSE;
                    }
                    
                    // Read length value is big endian, bytes 3:4
                    frlen.ubyte[UPPER] = dst[4];
                    frlen.ubyte[LOWER] = dst[5];
                    
                    // store new data to the local cache file.
                    // This will also change any file attributes, such as the
                    // file modtime on close
                    rc = vl_store(fp, frlen.ushort, &dst[6]);
                    if (rc != 0) {
                        ///@todo error code for store error (means file write is too big)
                        rc = -1024 - 1;
                        goto cmd_read_CLOSE;
                    }
                }
            }
            
            /// End of Read sync ---------------------------------------------
            
            dat_ptr = vl_memptr(fp);
            if (dat_ptr != NULL) {
                rc       = span;
                dat_ptr += arglist.range_lo;
            }
            
            cmd_read_CLOSE:
            vl_close(fp);
        }
        
        if (arglist.jsonout_flag == false)  {
            if (dat_ptr != NULL) {
                memcpy(dst, dat_ptr, span);
            }
        }
    }
    
    cmd_read_END:
    if (rc < 0) {
        rc = cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "r");
    }
    else {
        rc = cmd_jsonout_fmt((char**)&dst, &dstmax, arglist.jsonout_flag, rc, "r", 
                "{\"cmd\":\"%s\", \"devid\":\"%"PRIx64"\", \"block\":%i, \"id\":%i", "r",
                arglist.devid, arglist.block_id, arglist.file_id);
        rc = cmd_jsonout_data((char**)&dst, &dstmax, arglist.jsonout_flag, rc, dat_ptr, arglist.range_lo, span);
    }
    
    return rc;
}



int cmd_readall(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    vl_header_t nullhdr     = {0};
    vl_header_t* hdr_ptr    = &nullhdr;
    uint8_t*    dat_ptr     = NULL;
    int         span        = 0;
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "r*", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        vlFILE* fp;
        void* ptr;
            
        DEBUG_PRINTF("r* (read all cmd):\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n",
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi);
        
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
        hdr_ptr = ptr;
        rc      = sizeof(vl_header_t);
        
        fp = vl_open_file(header);
        if (fp != NULL) {
            span = arglist.range_hi - arglist.range_lo;
            if (dstmax < span) {
                span = (int)dstmax;
            }
            if ((fp->length-arglist.range_lo) <= 0) {
                span = 0;
            }
            else if ((fp->length-arglist.range_lo) < span) {
                span = (fp->length-arglist.range_lo);
            }
        
            dat_ptr = vl_memptr(fp);
            if (dat_ptr != NULL) {
                rc      += span;
                dat_ptr += arglist.range_lo;
            }
            vl_close(fp);
        }
        
        if (arglist.jsonout_flag == false)  {
            memcpy(dst, hdr_ptr, sizeof(vl_header_t));
            dst += sizeof(vl_header_t);
            
            if (dat_ptr != NULL) {
                memcpy(dst, dat_ptr, span);
            }
        }
    }
    
    cmd_readall_END:
    if (rc < 0) {
        rc = cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "r*");
    }
    else {
        rc = cmd_jsonout_fmt((char**)&dst, &dstmax, arglist.jsonout_flag, rc, "r*", 
                "{\"cmd\":\"%s\", \"devid\":\"%"PRIx64"\", \"block\":%d, \"id\":%d, \"mod\":%d, \"alloc\":%d, \"length\":%d, \"time\":%u",
                "r*", arglist.devid, arglist.block_id, arglist.file_id, hdr_ptr->idmod>>8, hdr_ptr->alloc, hdr_ptr->length, hdr_ptr->modtime);
        rc = cmd_jsonout_data((char**)&dst, &dstmax, arglist.jsonout_flag, rc, dat_ptr, arglist.range_lo, span);
    }
              
    return rc;
}



int cmd_restore(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
///@todo not currently supported, always returns error.

    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileblock_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "z", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUG_PRINTF("z (restore cmd):\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n",
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
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "z");
}



int cmd_readhdr(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_BLOCKID | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileblock_opt, fileid_man, end_man};
    vl_header_t null_header = {0};
    vl_header_t* ptr        = &null_header;
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "rh", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;

        DEBUG_PRINTF("rh (read header cmd):\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n",
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

        ptr = (vl_header_t*)vworm_get(header);
        if (ptr == NULL) {
            rc = -512 - 255;
            goto cmd_readhdr_END;
        }

        rc = sizeof(vl_header_t);
        memcpy(dst, ptr, sizeof(vl_header_t));
    }
    
    cmd_readhdr_END:
    if (rc < 0) {
        rc = cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "rh");
    }
    else {
        rc = cmd_jsonout_fmt((char**)&dst, &dstmax, arglist.jsonout_flag, rc, "rh", 
                "{\"cmd\":\"%s\", \"devid\":\"%"PRIx64"\", \"block\":%d, \"id\":%d, \"mod\":%d, \"alloc\":%d, \"length\":%d, \"time\":%u}",
                "rh", arglist.devid, arglist.block_id, arglist.file_id, ptr->idmod>>8, ptr->alloc, ptr->length, ptr->modtime);
    }
    
    return rc;
}



int cmd_readperms(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileblock_opt, filerange_opt, fileid_man, end_man};
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "rp", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        
        DEBUG_PRINTF("rp (read perms cmd):\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n",
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
    if (rc < 0) {
        rc = cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "rh");
    }
    else {
        rc = cmd_jsonout_fmt((char**)&dst, &dstmax, arglist.jsonout_flag, rc, "rp", 
                "{\"cmd\":\"%s\", \"devid\":\"%"PRIx64"\", \"block\":%d, \"id\":%d, \"mod\":%d}", 
                "rp", arglist.devid, arglist.block_id, arglist.file_id, (int)*dst);
    }
    
    return rc;
}



int cmd_write(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID | ARGFIELD_FILEDATA,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileblock_opt, filerange_opt, fileid_man, filedata_man, end_man};
    
    /// Extract arguments into arglist struct
    arglist.filedata        = dst;
    arglist.filedata_size   = (int)dstmax;
    rc                      = cmd_extract_args(&arglist, args, "w", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        vlFILE*     fp;
        uint8_t*    dptr;
        int         span;
    
        DEBUG_PRINTF("w (write cmd)\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n  write_bytes=%d\n",
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
        
        ///@todo save contents to temporary buffer, which is reverted-to upon ACK failure.
        
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
            goto cmd_write_CLOSE;
        }
            
        if (arglist.range_lo >= fp->alloc) {
            rc = -512 - 7;
            goto cmd_write_CLOSE;
        }
            
        if (arglist.range_hi > fp->alloc) {
            arglist.range_hi = fp->alloc;
        }
        
        span = arglist.filedata_size;
        if (span > (arglist.range_hi-arglist.range_lo)) {
            span = (arglist.range_hi-arglist.range_lo);
        }
        if (fp->length < (span+arglist.range_lo)) {
            fp->length = (span+arglist.range_lo);
        }
        if (span > 0) {
            memcpy(&dptr[arglist.range_lo], arglist.filedata, span);
        }
        
        ///@todo Re-implement this via devmgr
        ///@todo this should be a callable function, if possible.
        if (dth->devmgr != NULL) {
            int cmdbytes;
            char outbuf[576];
//            cmdbytes = snprintf(outbuf, 576-512-2, "file w -r %u:%u %u [",
//                                  arglist.range_lo, arglist.range_hi, arglist.file_id);
//            cmdbytes+= cmd_hexwrite(&outbuf[cmdbytes], arglist.filedata, span);
//            outbuf[cmdbytes] = ']';  cmdbytes++;
//            outbuf[cmdbytes] = 0;    cmdbytes++;
//            DEBUG_PRINTF("to smut: %.*s\n", cmdbytes, outbuf);
//            write(dth->devmgr->fd_writeto, outbuf, cmdbytes);
            
            cmdbytes            = snprintf(outbuf, 576-512-2, "file w -r %u:%u %u [",
                                        arglist.range_lo, arglist.range_hi, arglist.file_id);
            cmdbytes           += cmd_hexwrite(&outbuf[cmdbytes], arglist.filedata, span);
            outbuf[cmdbytes++]  = ']';
            outbuf[cmdbytes++]  = 0;
            cmdbytes            = cmd_devmgr(dth, dst, &cmdbytes, (uint8_t*)outbuf, dstmax);
            if (cmdbytes < 0) {
                ///@todo this means there's a write error.  Could try again, or
                /// flag some type of error.
            }
        }
        
        /// Closing the file will update its modification and access timestamps
        cmd_write_CLOSE:
        vl_close(fp);
    }
    
    cmd_write_END:
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "w");
}



int cmd_writeperms(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_BLOCKID | ARGFIELD_FILEID | ARGFIELD_FILEPERMS,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileblock_opt, fileid_man, fileperms_man, end_man};
    
    /// Writeperms requires an initialized OTFS handle
    if (dth->ext == NULL) {
        rc = -1;
    }
    else {
        /// Extract arguments into arglist struct
        rc = cmd_extract_args(&arglist, args, "wp", (const char*)src, inbytes);
    
        /// On successful extraction, write permissions to the specified file,
        /// on the specified device, within the device database.
        if (rc == 0) {
            DEBUG_PRINTF("wp (write perms cmd):\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n  write_perms=%0o\n",
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
            if (rc == 0) {
                ///@todo update the timestamp on this file, and on the device instance.
            }
            else {
                rc = -512 - rc;
            }
        }
    }
    
    cmd_writeperms_END:
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "wp");
}





int cmd_pub(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDOPT | ARGFIELD_BLOCKID | ARGFIELD_FILERANGE | ARGFIELD_FILEID | ARGFIELD_FILEDATA,
    };
    void* args[] = {help_man, jsonout_opt, devid_opt, fileblock_opt, filerange_opt, fileid_man, filedata_man, end_man};
    
    /// Extract arguments into arglist struct
    arglist.filedata        = dst;
    arglist.filedata_size   = (int)dstmax;
    rc                      = cmd_extract_args(&arglist, args, "pub", (const char*)src, inbytes);
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        vaddr header;
        vlFILE*     fp;
        uint8_t*    dptr;
        int         span;
    
        DEBUG_PRINTF("w (write cmd)\n  device_id=%016"PRIx64"\n  block=%d\n  file_id=%d\n  file_range=%d:%d\n  write_bytes=%d\n",
                arglist.devid, arglist.block_id, arglist.file_id, arglist.range_lo, arglist.range_hi, arglist.filedata_size);
                
        if (arglist.devid != 0) {
            rc = otfs_setfs(dth->ext, (uint8_t*)&arglist.devid);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_pub_END;
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
            goto cmd_pub_END;
        }
        
        fp = vl_open_file(header);
        if (fp == NULL) {
            rc = -512 - 255;
            goto cmd_pub_END;
        }
            
        dptr = vl_memptr(fp);
        if (dptr == NULL) {
            rc = -512 - 255;
            goto cmd_pub_CLOSE;
        }
            
        if (arglist.range_lo >= fp->alloc) {
            rc = -512 - 7;
            goto cmd_pub_CLOSE;
        }
            
        if (arglist.range_hi > fp->alloc) {
            arglist.range_hi = fp->alloc;
        }
        
        span = arglist.filedata_size;
        if (span > (arglist.range_hi-arglist.range_lo)) {
            span = (arglist.range_hi-arglist.range_lo);
        }
        if (fp->length < (span+arglist.range_lo)) {
            fp->length = (span+arglist.range_lo);
        }
        if (span > 0) {
            memcpy(&dptr[arglist.range_lo], arglist.filedata, span);
        }
        
        /// Closing the file will update its modification and access timestamps
        cmd_pub_CLOSE:
        vl_close(fp);
        
    }
    
    cmd_pub_END:
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "pub");
}





