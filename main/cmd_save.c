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
        devtest = otfs_setfs(handle, NULL, uid);
    }
    
    return devtest;
}







int cmd_save(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    
    // Local Talloc Context
    TALLOC_CTX* cmd_save_heap;
    
    // POSIX Filesystem and JSON handles
    char pathbuf[256];
    char* rtpath;
    //char* output;
    DIR* dir        = NULL;
    cJSON* tmpl     = NULL;
    cJSON* obj      = NULL;
    
    // Device OTFS
    int devtest;
    int devid_i = 0;
    otfs_id_union uid;
    
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDLIST | ARGFIELD_COMPRESS | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, jsonout_opt, compress_opt, archive_man, devidlist_opt, end_man};
    
    ///@todo do input checks!!!!!!
    
    /// Make sure there is something to save.
    if ((dth->ext->tmpl == NULL) || (dth->ext->db == NULL)) {
        return -1;
    }
    
    cmd_save_heap = talloc_new(dth->tctx);
    if (cmd_save_heap == NULL) {
    ///@todo better error messaging for out of memory
        return -1;
    }
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "save", (const char*)src, inbytes);
    if (rc != 0) {
        rc = -2;
        goto cmd_save_END;
    }
    DEBUGPRINT("cmd_open():\n  compress=%d\n  archive=%s\n", arglist.compress_flag, arglist.archive_path);
    
    /// Make sure that archive path doesn't already exist
    ///@todo error code & reporting for directory access errors (dir already
    /// exists, or any error that is not "dir doesn't exist")
    rtpath = stpncpy(pathbuf, arglist.archive_path, sizeof(pathbuf)-(16+32+1));
    DEBUGPRINT("%s %d :: check dir at %s\n", __FUNCTION__, __LINE__, pathbuf);
    cmd_rmdir(pathbuf);
    dir = opendir(pathbuf);
    if (dir != NULL) {
        rc = -3;
        goto cmd_save_END;
    }
    if (errno != ENOENT) {
        rc = -4;
        goto cmd_save_END;
    }
    
    /// Try to create a directory at the archive path.
    ///@todo error reporting for inability to create the directory.
    if (mkdir(pathbuf, 0700) != 0) {
        rc = -5;
        goto cmd_save_END;
    }
    
    /// Add trailing path separator if not already present
    if (rtpath[-1] != '/') {
        rtpath = stpcpy(rtpath, "/");
    }
    
    /// Write the saved tmpl to the output folder (pathbuf/_TMPL/tmpl.json)
    strcpy(rtpath, "_TMPL");
    DEBUGPRINT("%s %d :: create tmpl dir at %s\n", __FUNCTION__, __LINE__, pathbuf);
    if (mkdir(pathbuf, 0700) != 0) {
        rc = -6;
        goto cmd_save_END;
    }

    strcpy(rtpath, "_TMPL/tmpl.json");
    DEBUGPRINT("%s %d :: writing tmpl (%016llx) at %s\n", __FUNCTION__, __LINE__, dth->ext->tmpl, pathbuf);
    if (jst_writeout(dth->ext->tmpl, pathbuf) != 0) {
        rc = -7;
        goto cmd_save_END;
    }

    /// If there is a list of Device IDs supplied in the command, we use these.
    /// Else, we dump all the devices present in the OTDB.
    DEBUGPRINT("%s %d\n", __FUNCTION__, __LINE__);
    if (arglist.devid_strlist_size > 0) {
        devtest = sub_nextdevice(dth->ext->db, &uid.u8[0], &devid_i, arglist.devid_strlist, arglist.devid_strlist_size);
    }
    else {
        devtest = otfs_iterator_start(dth->ext->db, /*&devfs*/ NULL, &uid.u8[0]);
    }

    DEBUGPRINT("%s %d\n", __FUNCTION__, __LINE__);
    while (devtest == 0) {
        char* dev_rtpath;
        char hexuid[17];
        
        /// Create new directory for the device.
        /// New directory does not use leading zeros, but for implantation into
        /// files, it must have leading zeros.
        snprintf(hexuid, 17, "%"PRIx64, uid.u64);
        dev_rtpath  = stpcpy(rtpath, hexuid);
        snprintf(hexuid, 17, "%016"PRIx64, uid.u64);
        
        DEBUGPRINT("%s %d :: new dir at %s\n", __FUNCTION__, __LINE__, rtpath);
        if (mkdir(pathbuf, 0700) != 0) {
            rc = -8;
            ///@todo close necessary memory
            goto cmd_save_END;
        }

        /// Export each file in the Device FS to a JSON file in the dev root.
        /// Only file elements from the tmpl get exported.
        tmpl    = dth->ext->tmpl;
        obj     = tmpl->child;
        while (obj != NULL) {
            cJSON*      meta;
            cJSON*      content;
            cJSON*      cursor;
            cJSON*      output  = NULL;
            cJSON*      head    = NULL;
            vlFILE*     fp      = NULL;
            uint8_t*    fdat;
            uint8_t     file_id;
            uint8_t     block_id;
            uint16_t    output_sz;
            content_type_enum c_type;

            /// Template files must contain metadata to be considered
            meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
            if (meta == NULL) {
                // Skip files without meta objects
                goto cmd_save_LOOPEND;
            }
          
            /// Grab Block & ID of the file about to be exported, and open it.
            ///@todo make this a function (used in multiple places)
            ///@todo implement way to use non-stock files
            file_id     = jst_extract_id(meta);
            block_id    = jst_extract_blockid(meta);
            fp          = vl_open(block_id, file_id, VL_ACCESS_R, NULL);
            if (fp == NULL) {
                goto cmd_save_LOOPEND;
            }
            fdat = vl_memptr(fp);
            if (fdat == NULL) {
                goto cmd_save_LOOPCLOSE;
            }
            output_sz = jst_extract_size(meta);
            if (fp->length < output_sz) {
                output_sz = fp->length;
            }
          
            /// Create JSON object top level depth, for output
            head = cJSON_CreateObject();
            if (head == NULL) {
                goto cmd_save_LOOPCLOSE;
            }
            output = cJSON_AddObjectToObject(head, obj->string);
            if (output == NULL) {
                goto cmd_save_LOOPFREE;
            }
            
            /// Add modtime to the metadata, and copy all metadata to output
            {   cJSON *outmeta, *outtime, *outcontent, *outdevid;
                outmeta     = cJSON_Duplicate(meta, true);
                outdevid    = cJSON_CreateString(hexuid);
                outtime     = cJSON_GetObjectItemCaseSensitive(outmeta, "modtime");
                cJSON_SetIntValue(outtime, vl_getmodtime(fp));
                cJSON_AddItemReferenceToObject(outmeta, "devid", outdevid);
                cJSON_AddItemReferenceToObject(output, "_meta", outmeta);
                outcontent  = cJSON_CreateObject();
                cJSON_AddItemReferenceToObject(output, "_content", outcontent);
            }

            /// Add _content to output.  Content Data will get stored in here.
            output = cJSON_GetObjectItemCaseSensitive(output, "_content");
            if (output == NULL) {
                goto cmd_save_LOOPFREE;
            }
         
            /// Drill into contents -- three types
            /// 1. Hex output option: just a hex string
            /// 2. Array output option: integer for each byte
            /// 3. Struct output option: structured data elements based on template
            c_type = jst_extract_type(meta);
            
            if (c_type == CONTENT_hex) {
                char* hexstr;
                
                hexstr = talloc_size(cmd_save_heap, (2*output_sz) + 1);
                if (hexstr == NULL) {
                    goto cmd_save_LOOPFREE;
                }
                hexstr[fp->length] = 0;
                
                cmd_hexwrite(hexstr, fdat, output_sz);
                cursor = cJSON_AddStringToObject(output, obj->string, hexstr);
                if (cursor == NULL) {
                    goto cmd_save_LOOPFREE;
                }
                
                talloc_free(hexstr);
            }
            
            else if (c_type == CONTENT_array) {
                int* intarray;
                
                intarray = talloc_size(cmd_save_heap, sizeof(int)*output_sz);
                if (intarray == NULL) {
                    goto cmd_save_LOOPFREE;
                }
                
                for (int i=0; i<output_sz; i++) {
                    intarray[i] = fdat[i];
                }
                
                cursor = cJSON_CreateIntArray(intarray, output_sz);
                if (cursor == NULL) {
                    goto cmd_save_LOOPFREE;
                }
                
                talloc_free(intarray);
                
                cJSON_AddItemReferenceToObject(output, obj->string, cursor);
            }
            
            else { 
                // In struct type, the "_content" field must be an object.
                content = cJSON_GetObjectItemCaseSensitive(obj, "_content");
                if (cJSON_IsObject(content) == false) {
                    goto cmd_save_LOOPFREE;
                }
             
                // If content is empty, don't export this file
                content = content->child;
                if (content == NULL) {
                    goto cmd_save_LOOPFREE;
                }
             
                // Loop through the template, export flat items, drill into
                // nested items.
                ///@todo make this recursive
                while (content != NULL) {
                    cJSON* nest_tmpl;
                    cJSON* nest_output;
                    typeinfo_enum type;
                    int pos, bits;
                    unsigned long bitpos;
               
                    pos         = jst_extract_pos(content);
                    bits        = jst_extract_typesize(&type, content);
                    nest_output = jst_store_element(output, content->string, &fdat[pos], type, 0, bits);
                    nest_tmpl   = cJSON_GetObjectItemCaseSensitive(content, "_meta");
                 
                    // This is a nested data type (namely, a bitmask)
                    // Could be recursive, currently hardcoded for bitmask
                    if (nest_tmpl != NULL) {
                        nest_tmpl = cJSON_GetObjectItemCaseSensitive(nest_tmpl, "_content");
                        if (nest_tmpl != NULL) {
                            nest_tmpl = nest_tmpl->child;
                            while (nest_tmpl != NULL) {
                                bitpos = jst_extract_bitpos(nest_tmpl);
                                jst_store_element(nest_output, nest_tmpl->string, &fdat[pos], type, bitpos, bits);                              
                                nest_tmpl = nest_tmpl->next;
                            }
                        }
                    }

                    content = content->next;
                }
            }
            
            /// Writeout JSON 
            snprintf(dev_rtpath, 31, "/%u-%s.json", file_id, obj->string);
            DEBUGPRINT("%s %d :: new json file at %s\n", __FUNCTION__, __LINE__, &dev_rtpath[1]);
            if (jst_writeout(head, pathbuf) != 0) {
                goto cmd_save_LOOPCLOSE;
            }

            cmd_save_LOOPFREE:
            cmd_save_LOOPCLOSE:
            cmd_save_LOOPEND:
            //cJSON_Delete(output);
            cJSON_Delete(head);
            
            //cmd_save_LOOPCLOSE:
            vl_close(fp);
            
            //cmd_save_LOOPEND:
            obj = obj->next;
        }

        /// Fetch next device 
        DEBUGPRINT("%s %d :: fetch next device\n", __FUNCTION__, __LINE__);
        if (arglist.devid_strlist_size > 0) {
            devtest = sub_nextdevice(dth->ext->db, &uid.u8[0], &devid_i, arglist.devid_strlist, arglist.devid_strlist_size);
        }
        else {
            devtest = otfs_iterator_next(dth->ext->db, /*&devfs*/ NULL, &uid.u8[0]);
        }
 
    }

    /// Compress the directory structure and delete it.
    if (arglist.compress_flag == true) {
        ///@todo integrate 7z-lib
    }
    
    cmd_save_END:
    if (dir != NULL)
        closedir(dir);
    
    talloc_free(cmd_save_heap);
    
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "save");
}



