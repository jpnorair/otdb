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


static void sub_nextdevice(void* handle, int* devtest, int* devid_i, const char** strlist, size_t listsz) {
    for (*devtest=1, *devid_i=0; (*devtest!=0) && (*devid_i<listsz); (*devid_i)++) {
        uint8_t devid[8] = {0,0,0,0,0,0,0,0};
        cmd_hexnread(devid, strlist[*devid_i], 8);
        *devtest = otfs_setfs(handle, devid);
    }
}


int cmd_save(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    
    // POSIX Filesystem and JSON handles
    char pathbuf[256];
    char* rtpath;
    char* output;
    FILE* fjson             = NULL;
    DIR* dir                = NULL;
    cJSON* tmpl             = NULL;
    cJSON* obj              = NULL;
    
    // ...
    int devid_i = 0;
    int devtest;

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
    
    /// Write the saved tmpl to the output folder (pathbuf/_TMPL/tmpl.json)
    strcpy(rtpath, "/_TMPL");
    if (mkdir(pathbuf, 0700) != 0) {
        rc = -5;
        goto cmd_save_END;
    }
    
    strcpy(rtpath, "/_TMPL/tmpl.json");
    fjson = fopen(pathbuf, "w");
    if (fjson == NULL) {
        rc = -5;
        goto cmd_save_END;
    }
    
    output = cJSON_Print(dth->tmpl);
    fwrite(output, sizeof(char), strlen(output), fjson);
    fclose(fjson);
    free(output);
    
    /// If there is a list of Device IDs supplied in the command, we use these.
    /// Else, we dump all the devices present in the OTDB.
    if (arglist.devid_strlist_size > 0) {
        sub_nextdevice(dth->ext, &devtest, &devid_i, arglist.devid_strlist, arglist.devid_strlist_size);
    }
    else {
        devtest = otfs_iterator_start(dth->ext);
    }
    
    while (devtest == 0) {
    
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

        /// Determine elements in the FS, via the tmpl.  Only file elements from 
        /// the tmpl get exported.
        tmpl    = dth->tmpl;
        obj     = tmpl->child;
        while (obj != NULL) {
            cJSON*      meta;
            cJSON*      content;
            cJSON*      output;
            vl_header_t* hdr;
            
            vlFILE*     fp;
            uint8_t*    fdat;
            uint8_t     file_id;
            uint8_t     block_id;
            uint16_t    output_sz;
            content_type_enum c_type;

            meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
            if (meta == NULL) {
                // Skip files without meta objects
                continue;
            }
            
            // ID & Block
            // Find the file header position based on Block and ID
            ///@todo make this a function (used in multiple places)
            ///@todo implement way to use non-stock files
            file_id     = jst_extract_id(meta);
            block_id    = jst_extract_blockid(meta);
            fp          = vl_open(block_id, file_id, VL_ACCESS_R, NULL);
            if (fp == NULL) {
                continue;
            }
            fdat = vl_memptr(fp);
            if (fdat == NULL) {
                vl_close(fp);
                continue;
            }
            
            // Create husk of data file
            
            
            // Drill into contents
            c_type = jst_extract_type(meta);
            if (c_type == CONTENT_hex) {
                
                
                
                output_sz = jst_extract_size(meta);
                
            }
            else if (c_type == CONTENT_hex) {
                
            }
            else {  // struct-type --> need to go through template structure.
                content = cJSON_GetObjectItemCaseSensitive(obj, "_content");
            }
            
            vl_close(fp);
            
            
            
            switch (jst_extract_blockid(meta)) {
                case 1: if (idmod.ubyte[0] >= fshdr.gfb.used) {
                            continue;   
                        }
                        hdr = &gfbhdr[idmod.ubyte[0]];
                        break;
                case 2: if (idmod.ubyte[0] >= fshdr.iss.used) {
                            continue;   
                        }
                        hdr = &isshdr[idmod.ubyte[0]];
                        break;
                        
               default: if (idmod.ubyte[0] >= fshdr.isf.used) {
                            continue;   
                        }
                        hdr = &isfhdr[idmod.ubyte[0]];
                        break;
            }
            
            // TIME: epoch seconds
            // MIRROR: always 0xFFFF for OTDB
            // BASE: this is derived after all tmpl files are loaded
            // IDMOD: from already extracted value
            // ALLOC
            // LENGTH: this is derived later, from file contents
            hdr->modtime= jst_extract_time(meta);
            hdr->mirror = 0xFFFF;
            //hdr->base = ... ;
            hdr->idmod  = idmod.ushort;
            hdr->alloc  = jst_extract_size(meta);
            //hdr->length  = ... ;

            obj = obj->next;
        }
    
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



