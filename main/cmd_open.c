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







int cmd_open(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    
    // POSIX Filesystem and JSON handles
    char pathbuf[256];
    char* rtpath;
    DIR* dir                = NULL;
    DIR* devdir             = NULL;
    struct dirent *ent      = NULL;
    struct dirent *devent   = NULL;
    cJSON* tmpl             = NULL;
    cJSON* data             = NULL;
    cJSON* obj              = NULL;
    bool tmpl_valid         = false;
    
    // OTFS data tables and handles
    otfs_t tmpl_fs;
    vlFSHEADER fshdr;
    vl_header_t *gfbhdr, *isshdr, *isfhdr;
    
    // Argument handling
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEID | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, archive_man, end_man};
    
    ///@todo do input checks!!!!!!
  
    /// Make sure OTDB is available
    if (dth->ext == NULL) {
        return -1;
    }
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "open", (const char*)src, inbytes);
    if (rc != 0) {
        goto cmd_open_END;
    }
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    /// On successful extraction, create a new device in the database
    DEBUGPRINT("cmd_open():\n  archive=%s\n", arglist.archive_path);
    
    ///@todo implementation
    /// When opening a database, a copy of the template gets saved 
    /// temporarily as long as the Database is open.
    
    /// 1. Check if input file is compressed.  If so, decompress it to 
    ///    the local store (.activedb)
    /// 
    /// 1b.If input file is a directory (not compressed), copy it to the
    ///    local store (.activedb)
    
    /// 2. Template is stored in "_TMPL" directory within the archive 
    ///    folder.  Go through the _TMPL folder and create a single JSON
    ///    model that contains all the object data from all the files.
    ///    If any of the JSON is invalid, send an error (and say which 
    ///    where the error was, if possible).
    
    /// 3. Do a check to make sure the "_meta" object for each object has
    ///    at least "id" and "size" elements, within template.  Optional 
    ///    elements are "block", "type", "stock," "mod", and "time".  If these 
    ///    elements are missing, add defaults.  Default block is "isf". 
    ///    Default stock is "true".  Default mod is 00110100 (octal 64, decimal 
    ///    52).  Default time is the present epoch seconds since 1.1.1970.
    /// 3b.Save the template to the terminal/user instance.
    
    /// 4. Create the OTFS instance FS-table for this template, using the 
    ///    extracted template data.
    /// 4b.Derive OTFS file boundaries based on the template data, and write
    ///    these to the new OTFS FS-table.
    /// 4c.Derive additional OTFS file metadata based on the template data, and
    ///    write these to the new OTFS FS-table.
    
    /// 5. Data is stored in directories that have a name corresponding to
    ///    the DeviceID of the Device they are on.  Go through each of 
    ///    these files and make sure that the name of the directory is a
    ///    valid DeviceID (a number).  If there is an error, don't import
    ///    that Device.  Else, add a new FS to the database using this ID
    ///    and build a JSON data from an aggregate of all the JSON files
    ///    inside the directory. Using the JSON for this file, correlate it
    ///    against the master template and write it to each file in the new FS.
    
    /// 6. Wrap-up
    
    
    // String length is limited to the pathbuf minus the maximum OTDB filename
    rtpath  = stpncpy(pathbuf, arglist.archive_path, 
                    (sizeof(pathbuf) - sizeof("/_TMPL/") - (16+32+1)) );
    strcpy(rtpath, "/_TMPL/");
    dir     = opendir(pathbuf);
    if (dir == NULL) {
        rc = -1;
        goto cmd_open_CLOSE;
    }
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    // 2. Load all the JSON data (might span multiple files) into tmpl
    while (1) {
        ent = readdir(dir);
        if (ent == NULL) {
            break;
        }
        if (ent->d_type == DT_REG) {
            rc = jst_aggregate_json(&tmpl, ent->d_name);
            if (rc != 0) {
                rc = -256 + rc;
                goto cmd_open_CLOSE;
            }
        }
    }
    closedir(dir);
    dir = NULL;
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    // 3. Big Process of creating the default OTFS data structure based on JSON
    // input files
    tmpl_fs.alloc   = 0;
    tmpl_fs.base    = NULL;
    tmpl_fs.uid.u64 = 0;
    memset(&fshdr, 0, sizeof(vlFSHEADER));
    
    if (tmpl == NULL) {
        // No template found
        rc = -2;
        goto cmd_open_CLOSE;
    }
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    // 3a. Check template metadata.  Make sure it is valid.  Optional 
    // elements get filled with defaults if not present.
    obj = tmpl->child;
    while (obj != NULL) {
        cJSON* meta;
        cJSON* elem;
        int filesize = 0;
        int is_stock;
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
        meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
        if (meta != NULL) {
            // ID: mandatory
            elem = cJSON_GetObjectItemCaseSensitive(meta, "id");
            if (elem == NULL) {
                rc = -512 - 1;
                goto cmd_open_CLOSE;
            }
            
            // Size: mandatory
            elem = cJSON_GetObjectItemCaseSensitive(meta, "size");
            if (elem == NULL) {
                rc = -512 - 2;
                goto cmd_open_CLOSE;
            }
            filesize = elem->valueint;
            
            // Stock: optional, default=true
            elem = cJSON_GetObjectItemCaseSensitive(meta, "stock");
            if (elem == NULL) {
                elem = cJSON_CreateBool(true);
                cJSON_AddItemToObject(meta, "stock", elem);
                is_stock = 1;
            }
            else {
                is_stock = (elem->valueint != 0);
            }
            
            // Block: optional, default="isf"
            elem = cJSON_GetObjectItemCaseSensitive(meta, "block");
            if (elem == NULL) {
                fshdr.isf.alloc += filesize;
                fshdr.isf.used  += is_stock;
                fshdr.isf.files++;
                elem = cJSON_CreateString("isf");
                cJSON_AddItemToObject(meta, "block", elem);
            }
            switch (jst_blockid(elem)) {
                case 1: fshdr.gfb.alloc += filesize;
                        fshdr.gfb.used  += is_stock;
                        fshdr.gfb.files++;
                        break;
                        
                case 2: fshdr.iss.alloc += filesize;
                        fshdr.iss.used  += is_stock;
                        fshdr.iss.files++;
                        break;
            
               default: fshdr.isf.alloc += filesize;
                        fshdr.isf.used  += is_stock;
                        fshdr.isf.files++;
            }
            
            // Type: optional, default="array"
            elem = cJSON_GetObjectItemCaseSensitive(meta, "type");
            if (elem == NULL) {
                elem = cJSON_CreateString("array");
                cJSON_AddItemToObject(meta, "type", elem);
            }
            
            // Mod: optional, default=52
            elem = cJSON_GetObjectItemCaseSensitive(meta, "mod");
            if (elem == NULL) {
                elem = cJSON_CreateNumber(52.);
                cJSON_AddItemToObject(meta, "mod", elem);
            }
            
            // Time: optional, default=now()
            elem = cJSON_GetObjectItemCaseSensitive(meta, "time");
            if (elem == NULL) {
                elem = cJSON_CreateNumber((double)time(NULL));
                cJSON_AddItemToObject(meta, "time", elem);
            }
        }
        obj = obj->next;
    }
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    // 3b. Template is valid.  This is the point-of-no return.  Clear any old
    // template that may extist on the terminal and assign the new one.
    if (dth->tmpl != NULL) {
        cJSON_Delete(dth->tmpl);
    }
    tmpl_valid  = true;
    dth->tmpl   = tmpl;
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    // 3b. Add overhead section to the allocation, Malloc the fs data
    fshdr.ftab_alloc    = sizeof(vlFSHEADER) + sizeof(vl_header_t)*(fshdr.isf.files+fshdr.iss.files+fshdr.gfb.files);
    fshdr.res_time0     = (uint32_t)time(NULL);
    tmpl_fs.alloc       = vworm_fsalloc(&fshdr);
    tmpl_fs.base        = calloc(tmpl_fs.alloc, sizeof(uint8_t));
    if (tmpl_fs.base == NULL) {
        rc = -3;
        goto cmd_open_CLOSE;
    }
    memcpy(tmpl_fs.base, &fshdr, sizeof(vlFSHEADER));
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    gfbhdr  = (fshdr.gfb.files != 0) ? tmpl_fs.base+sizeof(vlFSHEADER) : NULL;
    isshdr  = (fshdr.iss.files != 0) ? tmpl_fs.base+sizeof(vlFSHEADER)+(fshdr.gfb.files*sizeof(vl_header_t)) : NULL;
    isfhdr  = (fshdr.isf.files != 0) ? tmpl_fs.base+sizeof(vlFSHEADER)+((fshdr.gfb.files+fshdr.iss.files)*sizeof(vl_header_t)) : NULL;
    
    // 4. Prepare the file table, except for base and length fields, which
    //    are done in 4b and 4c respectively.
    obj = tmpl->child;
    while (obj != NULL) {
        cJSON*  meta;
        vl_header_t* hdr;
        ot_uni16    idmod;
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
        meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
        if (meta == NULL) {
            // Skip files without meta objects
            continue;
        }
        
        // IDMOD & Block
        // Find the file header position based on Block and ID
        ///@todo make this a function (used in multiple places)
        ///@todo implement way to use non-stock files
        idmod.ubyte[0] = jst_extract_id(meta);
        idmod.ubyte[1] = jst_extract_mod(meta);
        
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
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    // 4b. Derive Base values from file allocations and write them to table
    if (fshdr.gfb.used > 0) {
        gfbhdr[0].base = fshdr.ftab_alloc;
        for (int i=1; i<fshdr.gfb.used; i++) {
            gfbhdr[i].base = gfbhdr[i-1].base + gfbhdr[i-1].alloc;
        }
    }
    if (fshdr.iss.used > 0) {
        isshdr[0].base = fshdr.ftab_alloc + fshdr.gfb.alloc;
        for (int i=1; i<fshdr.iss.used; i++) {
            isshdr[i].base = isshdr[i-1].base + isshdr[i-1].alloc;
        }
    }
    if (fshdr.isf.used > 0) {
        isfhdr[0].base = fshdr.ftab_alloc + fshdr.gfb.alloc + fshdr.iss.alloc;
        for (int i=1; i<fshdr.isf.used; i++) {
            isfhdr[i].base = isfhdr[i-1].base + isfhdr[i-1].alloc;
        }
    }
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    // 4c. Derive Length values from template and write default values to
    // the filesystem.
    obj = tmpl->child;
    while (obj != NULL) {
        cJSON*  meta;
        cJSON*  content;
        vl_header_t* hdr;
        bool    is_stock;
        content_type_enum ctype;
        uint8_t* filedata;
        ot_uni16    idmod;
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
        meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
        if (meta == NULL) {
            // Skip files without meta objects
            continue;
        }
        
        // IDMOD & Block
        // Find the file header position based on Block and ID
        ///@todo make this a function (used in multiple places)
        ///@todo implement way to use non-stock files
        idmod.ubyte[0] = jst_extract_id(meta);
        idmod.ubyte[1] = jst_extract_mod(meta);
        
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
        
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);        
        // Implicit params stock & type
        is_stock    = jst_extract_stock(meta);
        ctype       = jst_extract_type(meta);
        
        content     = cJSON_GetObjectItemCaseSensitive(obj, "_content");
        hdr->length = 0;
        filedata    = (uint8_t*)tmpl_fs.base + hdr->base;
        if (content != NULL) {
            // Struct type, most involved
            if (ctype == CONTENT_struct) {       
                content = content->child;
                while (content != NULL) {
                    cJSON* submeta;
                    int offset;
                    int e_sz;
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);                    
                    // Nested data element has _meta field
                    ///@todo recursive treatment of nested elements
                    submeta = cJSON_GetObjectItemCaseSensitive(content, "_meta");
                    if (submeta != NULL) {
                        offset  = jst_extract_pos(submeta);
                        e_sz    = jst_extract_size(submeta);
                        submeta = cJSON_GetObjectItemCaseSensitive(content, "_content");
                        if (submeta != NULL) {
                            submeta = submeta->child;
                            while (submeta != NULL) {
                                jst_load_element( &filedata[offset], 
                                        (size_t)e_sz, 
                                        (unsigned int)jst_extract_bitpos(submeta), 
                                        jst_extract_string(submeta, "type"), 
                                        cJSON_GetObjectItemCaseSensitive(content, "def"));
                                submeta = submeta->next;
                            }
                        }
                    }
                    
                    // Flat data element
                    else {
                        offset = jst_extract_pos(submeta);
                        e_sz = jst_load_element( &filedata[offset], 
                                    (size_t)(hdr->alloc - offset), 
                                    0, 
                                    jst_extract_string(submeta, "type"), 
                                    cJSON_GetObjectItemCaseSensitive(content, "def"));
                    }
                    content = content->next;
                }
            }
            
            // Bytearray type, each 
            else if (ctype == CONTENT_array) {  
                if (cJSON_IsArray(content)) {
                    hdr->length = cJSON_GetArraySize(content);
                    if (hdr->length > hdr->alloc) {
                        hdr->length = hdr->alloc;
                    }
                    for (int i=0; i<hdr->length; i++) {
                        cJSON* array_i  = cJSON_GetArrayItem(content, i);
                        filedata[i]     = (uint8_t)(255 & array_i->valueint);
                    }
                }
            }
            
            // hexstring type
            else {                      
                if (cJSON_IsString(content)) {
                    hdr->length = cmd_hexnread(filedata, content->valuestring, hdr->alloc);
                }
            }
        }

        obj = obj->next;
    }
    
    // 5. By this point, the default FS is created based on the input 
    // template.  For each device in the imported JSON, we make a copy of
    // this template and apply changes that are present.
    
    // Go into each directory that isn't "_TMPL"
    // The pathbuf already contains the root directory, from step 2, but we 
    // need to clip the _TMPL part.
    *rtpath = 0;
    dir     = opendir(pathbuf);
    if (dir == NULL) {
        rc = -1;
        goto cmd_open_CLOSE;
    }
    
    while (1) {
        char* endptr;
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
        ent = readdir(dir);
        if (ent == NULL) {
            break;
        }
        
        if (ent->d_type == DT_DIR) {
            vlFILE* fp;
        
            // Name of directory should be a pure hex number: skip others
            endptr = NULL;
            tmpl_fs.uid.u64 = strtoull(ent->d_name, &endptr, 16);
            if (((*ent->d_name != '\0') && (*endptr == '\0')) == 0) {
                continue;
            }
            
            // Create new FS based on device id and template FS
            rc = otfs_new(dth->ext, &tmpl_fs);
            if (rc != 0) {
                ///@todo adjusted error code: FS partially created
                goto cmd_open_CLOSE;
            }
            
            // Enter Device Directory: max is 16 hex chars long (8 bytes)
            strncpy(rtpath, ent->d_name, 16);
            devdir = opendir(pathbuf);
            if (devdir == NULL) {
                ///@todo adjusted error code: FS partially created
                rc = -10;
                goto cmd_open_CLOSE;
            }
            
            // Loop through files in the device directory, and aggregate them.
            while ( (devent = readdir(devdir)) != NULL ) {
                if (ent->d_type == DT_REG) {
                    rc = jst_aggregate_json(&data, ent->d_name);
                    if (rc != 0) {
                        rc = -256 + rc;
                        goto cmd_open_CLOSE;
                    }
                }
            }
            closedir(devdir);
            devdir = NULL;
            
            // Set OTFS to reference the current device FS.
            otfs_setfs(dth->ext, &tmpl_fs.uid.u8[0]);
            
            // Write the default device ID to the standardized locations.
            // - UID64 to ISF1 0:8
            // - VID16 to ISF0 0:2.  Derived from lower 16 bits of UID64.
            fp = ISF_open_su(0);
            if (fp != NULL) {
                uint16_t* cursor = (uint16_t*)vl_memptr(fp);
                if (cursor != NULL) {
                    cursor[0] = (uint16_t)(tmpl_fs.uid.u64 & 65535);
                }
                vl_close(fp);
            }
            fp = ISF_open_su(1);
            if (fp != NULL) {
                uint8_t* cursor = vl_memptr(fp);
                if (cursor != NULL) {
                    memcpy(cursor, &tmpl_fs.uid.u8[0], 8);
                }
                vl_close(fp);
            }
            
            // If there's no custom data to write, move on
            if (data == NULL) {
                continue;
            }
            data = data->child;
            
            // Correlate elements from data files with their metadata from the
            // template.  For each data element, we need the following 
            // attributes: (1) Block ID, (2) File ID, (3) Data range, (4) Data 
            // value.  With this information it is possible to write all the
            // per-device data.
            while (data != NULL) {
                cJSON* fileobj;
                vlFILE* fp;
                uint8_t* fdat;
                uint8_t block, file;
                content_type_enum ctype;
                uint16_t max;
                bool stock;
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);                
                fileobj = cJSON_GetObjectItemCaseSensitive(tmpl, data->string);
                if (fileobj == NULL) {
                    continue;
                }
                
                obj = cJSON_GetObjectItemCaseSensitive(fileobj, "_meta");
                if (obj == NULL) {
                    continue;
                }
                block   = jst_extract_blockid(obj);
                file    = jst_extract_id(obj);
                ctype   = jst_extract_type(obj);
                max     = jst_extract_size(obj);
                stock   = jst_extract_stock(obj);

                obj = cJSON_GetObjectItemCaseSensitive(fileobj, "_contents");
                if (obj == NULL) {
                    continue;
                }
                
                fp = vl_open( (vlBLOCK)block, file, VL_ACCESS_RW, NULL);
                if (fp == NULL) {
                    continue;
                }
                fdat = vl_memptr(fp);
                if (fdat == NULL) {
                    vl_close(fp);
                    continue;
                }
                            
                if (fp->alloc < max) {
                    max = fp->alloc;
                }
                
                if (ctype == CONTENT_hex) {
                    fp->length = cmd_hexnread(fdat, obj->valuestring, (size_t)max);
                }
                else if (ctype == CONTENT_array) {
                    int items;
                    items = cJSON_GetArraySize(obj);
                    if (items > max) {
                        items = max;
                    }
                    fp->length = items;
                    for (int i=0; i<items; i++) {
                        cJSON* array_i  = cJSON_GetArrayItem(obj, i);
                        fdat[i]         = (uint8_t)(255 & array_i->valueint);
                    }
                }
                
                // CONTENT_struct
                ///@todo might benefit from recursive treatment
                else {  // CONTENT_struct
                    obj = obj->child;
                    while (obj != NULL) {
                        cJSON*  t_elem;
                        int     bytepos;
                        int     bytesout;
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
                        // Make sure object is in both data and tmpl.
                        // If object yields another object, we need to drill 
                        // into the hierarchy
                        t_elem  = cJSON_GetObjectItemCaseSensitive(obj, obj->string);
                        if (t_elem != NULL) {
                            bytepos     = jst_extract_pos(t_elem);
                            bytesout    = jst_load_element( &fdat[bytepos], 
                                            (max - bytepos), 
                                            (unsigned int)jst_extract_bitpos(t_elem), 
                                            jst_extract_string(t_elem, "type"), 
                                            obj);
                        
                            // jst_load_element() returns negative values for nested elements
                            // This is how recursion would be used -- nested elements
                            // recurse.
                            if (bytesout < 0) {
                                cJSON* subobj   = obj->child;
                                bytesout        = -bytesout;
                                
                                while (subobj != NULL) {
                                    t_elem  = cJSON_GetObjectItemCaseSensitive(obj, obj->string);
                                    if (t_elem != NULL) {
                                        jst_load_element( &fdat[bytepos], 
                                            bytesout, 
                                            (unsigned int)jst_extract_bitpos(t_elem), 
                                            jst_extract_string(t_elem, "type"), 
                                            subobj);
                                    }
                                    subobj = subobj->next;
                                }
                            }
                            
                            fp->length = bytepos + bytesout;
                        }
                        
                        obj = obj->next;
                    }
                } 
                // end of CONTENT_struct
                vl_close(fp);
                
            }
            // end of data file interator
            
        }
    }
    closedir(dir);
    dir = NULL;
    
    // 6. Save new JSON template to local stash.  For debugging purposes it is
    //    also written to a local file.
    dth->tmpl = tmpl;
    
    ///@todo open a file for writing.
fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);    
    
    
    cmd_open_CLOSE:
    if ((tmpl != NULL) && (tmpl_valid == true)) {
        cJSON_Delete(tmpl);
    }
    if (data != NULL)   cJSON_Delete(data);
    if (devdir != NULL) closedir(devdir);
    if (dir != NULL)    closedir(dir);
    
    cmd_open_END:
    fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__); 
    return cmd_jsonout_err((char*)dst, dstmax, (bool)arglist.jsonout_flag, rc, "open");
}

