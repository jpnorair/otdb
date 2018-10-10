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
extern struct arg_str*     devid_man;
extern struct arg_file*    archive_man;
extern struct arg_lit*     compress_opt;

// used by file commands
extern struct arg_str*     devid_opt;
extern struct arg_str*     devidlist_opt;
extern struct arg_str*     fileblock_opt;
extern struct arg_str*     filerange_opt;
extern struct arg_int*     fileid_man;
extern struct arg_str*     fileperms_man;
extern struct arg_int*     filealloc_man;
extern struct arg_str*     filedata_man;

// used by all commands
extern struct arg_lit*     help_man;
extern struct arg_end*     end_man;


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


static int sub_nextdevice(void* handle, uint8_t* uid, int* devid_i, const char** strlist, size_t listsz) {
    int devtest = 1;

    for (; (devtest!=0) && (*devid_i<listsz); (*devid_i)++) {
        memset(uid, 0, 8);
        cmd_hexnread(uid, strlist[*devid_i], 8);
        devtest = otfs_setfs(handle, uid);
    }
    
    return devtest;
}

static int sub_json_writeout(cJSON* json_obj, const char* filepath) {
    FILE* fjson;
    char* output;

    fjson = fopen(filepath, "w");
    if (fjson != NULL) {
        output = cJSON_Print(json_obj);
        fwrite(output, sizeof(char), strlen(output), fjson);
        fclose(fjson);
        free(output);
        return 0;
    }
    
    return -1;
}





int cmd_save(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    
    // POSIX Filesystem and JSON handles
    char pathbuf[256];
    char* rtpath;
    //char* output;
    DIR* dir        = NULL;
    cJSON* tmpl     = NULL;
    cJSON* obj      = NULL;
    
    // Device OTFS
    int devtest;
    int devid_i;
    otfs_t* devfs;
    otfs_id_union uid;
    

    cmd_arglist_t arglist = {
        .fields = ARGFIELD_DEVICEIDLIST | ARGFIELD_COMPRESS | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, compress_opt, archive_man, devidlist_opt, end_man};
    
    /// Make sure there is something to save.
    if ((dth->tmpl == NULL) || (dth->ext == NULL)) {
        return -1;
    }
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "save", (const char*)src, inbytes);
    if (rc != 0) {
        goto cmd_save_END;
    }
    DEBUGPRINT("cmd_open():\n  compress=%d\n  archive=%s\n", 
            arglist.compress_flag, arglist.archive_path);
    
    /// Make sure that archive path doesn't already exist
    ///@todo error code & reporting for directory access errors (dir already
    /// exists, or any error that is not "dir doesn't exist")
    rtpath  = stpncpy(pathbuf, arglist.archive_path, sizeof(pathbuf)-(16+32+1));
    dir     = opendir(pathbuf);
    if (dir != NULL) {
        closedir(dir);
        rc = -2;
        goto cmd_save_END;
    }
    if (errno != ENOENT) {
        closedir(dir);
        rc = -3;
        goto cmd_save_END;
    }
    
    /// Try to create a directory at the archive path.
    ///@todo error reporting for inability to create the directory.
    if (mkdir(pathbuf, 0700) != 0) {
        rc = -4;
        goto cmd_save_END;
    }
    
    /// Add trailing path separator if not already present
    if (rtpath[-1] != '/') {
        rtpath = stpcpy(rtpath, "/");
    }
    
    /// Write the saved tmpl to the output folder (pathbuf/_TMPL/tmpl.json)
    strcpy(rtpath, "_TMPL");
    if (mkdir(pathbuf, 0700) != 0) {
        rc = -5;
        goto cmd_save_END;
    }
    
    strcpy(rtpath, "_TMPL/tmpl.json");
    if (sub_json_writeout(dth->tmpl, pathbuf) != 0) {
        rc = -6;
        goto cmd_save_END;
    }
    
    
    /// If there is a list of Device IDs supplied in the command, we use these.
    /// Else, we dump all the devices present in the OTDB.
    if (arglist.devid_strlist_size > 0) {
        devtest = sub_nextdevice(dth->ext, &uid.u8[0], &devid_i, arglist.devid_strlist, arglist.devid_strlist_size);
    }
    else {
        devtest = otfs_iterator_start(dth->ext, &devfs, &uid.u8[0]);
    }
    
    while (devtest == 0) {
        char* dev_rtpath;
        
        /// Create new directory for the device
        dev_rtpath  = rtpath;
        dev_rtpath += snprintf(rtpath, 17, "%16llX", uid.u64);
        if (mkdir(pathbuf, 0700) != 0) {
            rc = -4;
            ///@todo close necessary memory
            goto cmd_save_END;
        }
        
        /// Export each file in the Device FS to a JSON file in the dev root.
        /// Only file elements from the tmpl get exported.
        tmpl    = dth->tmpl;
        obj     = tmpl->child;
        while (obj != NULL) {
            cJSON*      meta;
            cJSON*      content;
            cJSON*      output;
            cJSON*      cursor;
            vlFILE*     fp;
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
            output = cJSON_CreateObject();
            if (output == NULL) {
                goto cmd_save_LOOPCLOSE;
            }
            
            /// Drill into contents
            c_type = jst_extract_type(meta);
            
            /// Hex output option: just a hex string
            if (c_type == CONTENT_hex) {
                char* hexstr = malloc((2*output_sz) + 1);
                if (hexstr == NULL) {
                    goto cmd_save_LOOPFREE;
                }
                hexstr[fp->length] = 0;
                cmd_hexwrite(hexstr, fdat, output_sz);
                cursor = cJSON_AddStringToObject(output, obj->string, hexstr);
                free(hexstr);
                if (cursor == NULL) {
                    goto cmd_save_LOOPFREE;
                }
            }
            
            /// Array output option: integer for each byte
            else if (c_type == CONTENT_array) {
                int* intarray = malloc(sizeof(int)*output_sz);
                if (intarray == NULL) {
                    goto cmd_save_LOOPFREE;
                }
                for (int i; i<output_sz; i++) {
                    intarray[i] = fdat[i];
                }
                
                cursor = cJSON_CreateIntArray(intarray, output_sz);
                free(intarray);
                if (cursor == NULL) {
                    goto cmd_save_LOOPFREE;
                }
                cJSON_AddItemReferenceToObject(output, obj->string, cursor);
            }
            
            /// Struct output option: structured data elements based on template
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
                    int pos, bitpos, bits;
                    
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
                                ///@todo leave-off point.  Write bit type to nest_output
                                jst_store_element(nest_output, nest_tmpl->string, &fdat[pos], type, bitpos, bits);
                            }
                        }
                    }

                    content = content->next;
                }
                
                ///@todo is something needed here?
            }
            
            /// Writeout JSON 
            snprintf(dev_rtpath, 31, "/%s.json", obj->string);
            if (sub_json_writeout(output, pathbuf) != 0) {
                goto cmd_save_LOOPCLOSE;
            }

            cmd_save_LOOPFREE:
            cJSON_Delete(output);
            
            cmd_save_LOOPCLOSE:
            vl_close(fp);
            
            cmd_save_LOOPEND:
            obj = obj->next;
        }
    
    
    
        /// Fetch next device 
        if (arglist.devid_strlist_size > 0) {
            devtest = sub_nextdevice(dth->ext, &uid.u8[0], &devid_i, arglist.devid_strlist, arglist.devid_strlist_size);
        }
        else {
            devtest = otfs_iterator_next(dth->ext, &devfs, &uid.u8[0]);
        }
    }
        /// Step 1: Read the FS table descriptor to determine which elements
        /// are stored in the FS.  The table descriptor looks like this
        /// B0:1    Allocation Bytes of Metadata
        /// ---> File Action Fields not used in OTDB
        /// B2:3    GFB Bytes Allocated
        /// B4:5    GFB Bytes Used
        /// B6:7    GFB Stock Files
        /// B8:9    ISS Bytes Allocated
        /// B10:11  ISS Bytes Used
        /// B12:13  ISS Stock Files
        /// B14:15  ISF Bytes Allocated
        /// B16:17  ISF Bytes Used
        /// B18:19  ISF Stock Files
        /// B20:23  Modification Time 0
        /// B24:27  Modification Time 1
        ///
        ///
        /// Step 3: For each file
        /// - Get the contents as a binary byte array.
        /// - Load the internal JSON template for this file and get the structure
        /// - Write an output JSON file using the structure from the internal template
        ///
        /// Step 4: Repeat for all files in DB, or for list of Device IDs supplied
        ///
        /// Step 5: Compress the output directory if required
        ///

        
        
    
    }

    // 2. 

    // 3. 

    // 4. 

    // 5. Compress the directory structure and delete it.
    if (arglist.compress_flag == true) {
        ///@todo integrate 7z-lib
    }
    
    cmd_save_END:
    return rc;
}



