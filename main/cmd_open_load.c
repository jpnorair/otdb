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
#include "test.h"
#include "debug.h"

// HB Headers/Libraries
#include <bintex.h>
#include <cmdtab.h>
#include <argtable3.h>
#include <cJSON.h>
#include <cJSON_blockdup.h>
#include <otfs.h>
#include <hbdp/hb_cmdtools.h>       ///@note is this needed?

// Standard C & POSIX Libraries
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>




typedef struct {
    content_type_enum   ctype;
    bool                stock;
    uint8_t             block;
    uint8_t             fileid;
    uint8_t             mod;
    uint16_t            pos;
    uint16_t            size;
    unsigned long       modtime;
} filemeta_t;



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



static void sub_tfree(void* ctx) {
    talloc_free(ctx);
}


static bool uid_in_list(const char** uidlist, size_t listsize, uint64_t cmp) {
    uint64_t devid;
    bool test = false;
    
    for (int i=0; i<listsize; i++) {
        char* endptr;
        endptr  = NULL;
        devid   = strtoull(uidlist[i], &endptr, 16);

        if ((*endptr == '\0') && (uidlist[i][0] != '\0') && (devid == cmp)) {
            test = true;
            break;
        }
    }
    
    return true;
}


///@note From https://womble.decadent.org.uk/readdir_r-advisory.html
/* Calculate the required buffer size (in bytes) for directory       *
 * entries read from the given directory handle.  Return -1 if this  *
 * this cannot be done.                                              *
 *                                                                   *
 * This code does not trust values of NAME_MAX that are less than    *
 * 255, since some systems (including at least HP-UX) incorrectly    *
 * define it to be a smaller value.                                  *
 *                                                                   *
 * If you use autoconf, include fpathconf and dirfd in your          *
 * AC_CHECK_FUNCS list.  Otherwise use some other method to detect   *
 * and use them where available.                                     */

size_t dirent_buf_size(DIR * dirp) {
    long name_max;
    size_t name_end;
#   if defined(HAVE_FPATHCONF) && defined(HAVE_DIRFD) && defined(_PC_NAME_MAX)
        name_max = fpathconf(dirfd(dirp), _PC_NAME_MAX);
        if (name_max == -1)
#           if defined(NAME_MAX)
                name_max = (NAME_MAX > 255) ? NAME_MAX : 255;
#           else
#               warning "readdir_r() requires excessive allocation"
                return (size_t)(-1);
#           endif
#   else
#       if defined(NAME_MAX)
            name_max = (NAME_MAX > 255) ? NAME_MAX : 255;
#       else
#           error "buffer size for readdir_r cannot be determined"
#       endif
#   endif

    name_end = (size_t)offsetof(struct dirent, d_name) + name_max + 1;
    return (name_end > sizeof(struct dirent)
            ? name_end : sizeof(struct dirent));
}




int cmdsub_datafile(dterm_handle_t* dth, uint8_t* dst, size_t dstmax,
                        cJSON* fstmpl, DIR* devdir, const char* path, uint64_t uid,
                        bool export_tmp) {
    int rc                  = 0;
    struct dirent *devent   = NULL;
    struct dirent *entbuf   = NULL;
    cJSON* data             = NULL;
    cJSON* dataobj;
    vlFILE* fp;
    TALLOC_CTX* cmdsub_datafile_heap;
    
    cmdsub_datafile_heap = talloc_new(dth->tctx);
    if (cmdsub_datafile_heap == NULL) {
    ///@todo better error code for out of memory
        return -1;
    }
    
    // Allocate directory traversal buffer -- fast exit if fails
    entbuf = talloc_size(cmdsub_datafile_heap, dirent_buf_size(devdir));
    if (entbuf == NULL) {
        return -1;
    }
    
    // Loop through files in the device directory, and aggregate them.
    while (1) {
        readdir_r(devdir, entbuf, &devent);
        if (devent == NULL) {
            break;
        }
        if (devent->d_type == DT_REG) {
            DEBUGPRINT("%s %d :: json=%s/%s\n", __FUNCTION__, __LINE__, path, devent->d_name);
            rc = jst_aggregate_json(cmdsub_datafile_heap, &data, path, devent->d_name);
            if (rc != 0) {
                rc = -9;
                goto cmdsub_datafile_CLOSE;
            }
        }
    }
    
    ///@note Prior to this function being called, otfs_new() or otfs_setfs()
    /// must be used in the calling function to select the device fs.
    
    // Write the default device ID to the standardized locations.
    // This may be overwritten later, by supplied JSON data.
    // - UID64 to ISF1 0:8
    // - VID16 to ISF0 0:2.  Derived from lower 16 bits of UID64.
    fp = ISF_open_su(0);
    if (fp != NULL) {
        uint16_t* cursor = (uint16_t*)vl_memptr(fp);
        if (cursor != NULL) {
            DEBUGPRINT("%s %d :: write VID [%04X] to Device [%016"PRIx64"]\n", __FUNCTION__, __LINE__, (uint16_t)(uid & 65535), uid);
            cursor[0] = (uint16_t)(uid & 65535);
        }
        vl_close(fp);
    }

    fp = ISF_open_su(1);
    if (fp != NULL) {
        uint8_t* cursor = vl_memptr(fp);
        if (cursor != NULL) {
            ///@todo make sure endian gets sorted
            DEBUGPRINT("%s %d :: write UID [%016"PRIx64"]\n", __FUNCTION__, __LINE__, uid);
            memcpy(cursor, (uint8_t*)&uid, 8);
        }
        vl_close(fp);
    }
    
    // If there's no custom data to write, move on
    if (data == NULL) {
        rc = 0;
        goto cmdsub_datafile_CLOSE;
    }

    // Correlate elements from data files with their metadata from the
    // template.  For each data element, we need the following
    // attributes: (1) Block ID, (2) File ID, (3) Data range, (4) Data
    // value.  With this information it is possible to write all the
    // per-device data.
    for (dataobj=data->child; dataobj!=NULL; dataobj=dataobj->next) {
        cJSON* obj;
        cJSON* fileobj;
        cJSON* datacontent;
        vlFILE* fp;
        filemeta_t dmeta;
        uint8_t* fdat;
        int derived_length = 0;
        
        // If there's no "_meta" field, skip this file.
        // If it exists, copy the meta information locally.
        // if modtime doesn't exist, or is zero, use time=now
        obj = cJSON_GetObjectItemCaseSensitive(dataobj, "_meta");
        DEBUGPRINT("%s %d :: Object \"_meta\" found in DataObject = %d\n", __FUNCTION__, __LINE__, (obj!=NULL));
        if (cJSON_IsObject(obj) == false) {
            continue;
        }
        dmeta.block     = jst_extract_blockid(obj);
        dmeta.fileid    = jst_extract_id(obj);
        dmeta.ctype     = jst_extract_type(obj);
        dmeta.size      = jst_extract_size(obj);
        dmeta.modtime   = jst_extract_time(obj);
        if (dmeta.modtime == 0) {
            dmeta.modtime = time(NULL);
        }
        
        // If there's no data "_content" field, skip this file.
        // We will use the datacontent JSON object later
        datacontent = cJSON_GetObjectItemCaseSensitive(dataobj, "_content");
        if (cJSON_IsObject(datacontent) == false) {
            continue;
        }
        
        // Get the template meta defaults for this file, matched on the file name
        // Make sure that template metadata is aligned with file metadata
        fileobj = cJSON_GetObjectItemCaseSensitive(fstmpl, dataobj->string);
        DEBUGPRINT("%s %d :: Object \"%s\" found in TMPL = %d\n", __FUNCTION__, __LINE__, dataobj->string, (fileobj!=NULL));
        if (cJSON_IsObject(fileobj) == false) {
            continue;
        }
        obj = cJSON_GetObjectItemCaseSensitive(fileobj, "_meta");
        DEBUGPRINT("%s %d :: Object \"_meta\" found in FileObject = %d\n", __FUNCTION__, __LINE__, (obj!=NULL));
        if (cJSON_IsObject(obj) == false) {
            continue;
        }

        // block, fileid, and size must match in template and file
        if ((dmeta.block != jst_extract_blockid(obj))
        ||  (dmeta.fileid != jst_extract_id(obj))
        ||  (dmeta.size != jst_extract_size(obj))) {
            DEBUGPRINT("%s %d :: Metadata mismatch on %s\n", __FUNCTION__, __LINE__, dataobj->string);
            continue;
        }
    
        // "stock" parameter must be taken from template only
        dmeta.stock = jst_extract_stock(obj);
    
        // "ctype" parameter is retained from file
        // "modtime" parameter is retained from file
        
        // Get the template content, and open
        obj = cJSON_GetObjectItemCaseSensitive(fileobj, "_content");
        DEBUGPRINT("%s %d :: Object \"_content\" found in FileObject = %d\n", __FUNCTION__, __LINE__, (obj!=NULL));
        if (cJSON_IsObject(obj) == false) {
            continue;
        }
        fp = vl_open( (vlBLOCK)dmeta.block, dmeta.fileid, VL_ACCESS_RW, NULL);
        if (fp == NULL) {
            continue;
        }
        
        fdat = vl_memptr(fp);
        DEBUGPRINT("%s %d :: File Data at: %016"PRIx64"\n", __FUNCTION__, __LINE__, (uint64_t)fdat);
        DEBUGPRINT("%s %d :: block=%i, file=%i, ctype=%i, size=%i, stock=%i\n", __FUNCTION__, __LINE__, dmeta.block, dmeta.fileid, dmeta.ctype, dmeta.size, dmeta.stock);
        DEBUGPRINT("%s %d :: fp->alloc=%i, fp->length=%i\n", __FUNCTION__, __LINE__, fp->alloc, fp->length);
        if (fdat == NULL) {
            vl_close(fp);
            continue;
        }
        if (fp->alloc < dmeta.size) {
            dmeta.size = fp->alloc;
        }
    
        if (dmeta.ctype == CONTENT_hex) {
            DEBUGPRINT("%s %d :: Working on Hex\n", __FUNCTION__, __LINE__);
            fp->length = cmd_hexnread(fdat, datacontent->valuestring, (size_t)dmeta.size);
        }
        else if (dmeta.ctype == CONTENT_array) {
            int items;
            DEBUGPRINT("%s %d :: Working on Array\n", __FUNCTION__, __LINE__);
            items = cJSON_GetArraySize(datacontent);
            if (items > dmeta.size) {
                items = dmeta.size;
            }
            derived_length = items;
            for (int i=0; i<items; i++) {
                cJSON* array_i  = cJSON_GetArrayItem(datacontent, i);
                fdat[i]         = (uint8_t)(255 & array_i->valueint);
            }
        }
        ///@todo might benefit from recursive treatment
        else {  // CONTENT_struct
            for (obj=obj->child; obj!=NULL; obj=obj->next) {
                cJSON*  d_elem;
                cJSON*  t_meta;
                cJSON*  t_content;
                int     bytepos;
                int     bytesout;
                
                DEBUGPRINT("%s %d :: Working on Struct element=%s\n", __FUNCTION__, __LINE__, obj->string);
                
                // Make sure object is in both data and tmpl.
                // If object yields another object, we need to drill
                // into the hierarchy
                d_elem  = cJSON_GetObjectItemCaseSensitive(datacontent, obj->string);
                if (d_elem != NULL) {
                    t_meta      = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
                    t_content   = cJSON_GetObjectItemCaseSensitive(obj, "_content");
                    if (cJSON_IsObject(t_meta) && cJSON_IsObject(t_content) && cJSON_IsObject(datacontent)) {
                        bytepos     = (int)jst_extract_pos(t_meta);
                        bytesout    = (int)jst_extract_size(t_meta);

                        for (t_content=t_content->child; t_content!=NULL; t_content=t_content->next) {
                            cJSON* d_subelem;
                            d_subelem  = cJSON_GetObjectItemCaseSensitive(d_elem, t_content->string);
                            if (d_subelem != NULL) {
                                jst_load_element( &fdat[bytepos],
                                    bytesout,
                                    (unsigned int)jst_extract_bitpos(t_content),
                                    jst_extract_string(t_content, "type"),
                                    d_subelem);
                            }
                        }
                    }
                    else {
                        bytepos = (int)jst_extract_pos(obj);
                        bytesout = jst_load_element( &fdat[bytepos],
                                        (int)dmeta.size - bytepos,
                                        (unsigned int)jst_extract_bitpos(obj),
                                        jst_extract_string(obj, "type"),
                                        d_elem);
                    }
                    
                    derived_length = bytepos + bytesout;
                }
            }
        }
        
        // end of CONTENT_struct
        if (derived_length > fp->length) {
            if (derived_length > fp->alloc) {
                derived_length = fp->alloc;
            }
            fp->length = (uint16_t)derived_length;
        }
        
        vl_setmodtime(fp, (ot_u32)dmeta.modtime);
        vl_close(fp);
    }
    // end of data file interator
    
    ///@note tmp only gets stored on OPEN or save operations
    // Save copy of JSON to local stash
    if (export_tmp) {
        char local_path[64];
        snprintf(local_path, sizeof(local_path)-10, OTDB_PARAM_SCRATCHDIR"/%016"PRIx64, uid);

        ///@todo check if dir already exists, if not, make it
        if (mkdir(local_path, 0700) == 0) {
            strcat(local_path, "/data.json");
            jst_writeout(data, local_path);
        }
    }

    cmdsub_datafile_CLOSE:
    cJSON_Delete(data);
    talloc_free(cmdsub_datafile_heap);

    return rc;
}












int cmd_open(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    
    // POSIX Filesystem and JSON handles
    char pathbuf[256];
    char* rtpath;
    DIR* dir                = NULL;
    DIR* devdir             = NULL;
    struct dirent *ent      = NULL;
    struct dirent *entbuf   = NULL;
    cJSON* tmpl             = NULL;
    cJSON* tmpl_export      = NULL;
    cJSON* data             = NULL;
    cJSON* obj              = NULL;
    void* db                = NULL;
    
    // Function Heap
    TALLOC_CTX* cmd_open_heap;
    
    // OTFS data tables and handles
    otfs_t* tmpl_fs;
    otfs_t data_fs;
    vlFSHEADER fshdr;
    vl_header_t *gfbhdr, *isshdr, *isfhdr;
    
    // Argument handling
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, jsonout_opt, archive_man, end_man};
    
    ///@todo do more input checks!!!!!!

    tmpl_fs = talloc_zero_size(dth->pctx, sizeof(otfs_t));
    if (tmpl_fs == NULL) {
        ///@todo better error code for out of memory
        return -1;
    }

    cmd_open_heap = talloc_new(dth->tctx);
    if (cmd_open_heap == NULL) {
        ///@todo better error code for out of memory
        return -1;
    }
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "open", (const char*)src, inbytes);
    if (rc != 0) {
        goto cmd_open_CLOSE;
    }
 
    /// On successful extraction, create a new device in the database
    DEBUGPRINT("cmd_open():\n  json=%d, archive=%s\n", arglist.jsonout_flag, arglist.archive_path);
    
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
    ///    elements are "block", "type", "stock," "mod", and "modtime".  If
    ///    these elements are missing, add defaults.  Default block is "isf".
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
                    (sizeof(pathbuf) - sizeof("/_TMPL") - (16+32+1)) );
    strcpy(rtpath, "/_TMPL");
    dir     = opendir(pathbuf);
    if (dir == NULL) {
        rc = -1;
        goto cmd_open_CLOSE;
    }
   
    // Allocate directory traversal buffer
    entbuf = talloc_size(cmd_open_heap, dirent_buf_size(dir));
    if (entbuf == NULL) {
        rc = -1;
        goto cmd_open_CLOSE;
    }
   
    // 2. Load all the JSON data (might span multiple files) into tmpl
    while (1) {
        readdir_r(dir, entbuf, &ent);
        if (ent == NULL) {
            break;
        }
        if (ent->d_type == DT_REG) {
            rc = jst_aggregate_json(cmd_open_heap, &tmpl, pathbuf, ent->d_name);
            if (rc != 0) { 
                rc = -256 + rc;
                goto cmd_open_CLOSE;
            }
        }
    }
    closedir(dir);
    talloc_free(entbuf);
    entbuf = NULL;
    dir = NULL;

//{ char* fbuf = cJSON_Print(tmpl);  fputs(fbuf, stderr);  free(fbuf); }

    // 3. Big Process of creating the default OTFS data structure based on JSON
    // input files

    // Template FS for defaults
    tmpl_fs->alloc   = 0;
    tmpl_fs->base    = NULL;
    tmpl_fs->uid.u64 = 0;
    memset(&fshdr, 0, sizeof(vlFSHEADER));
    if (tmpl == NULL) {
        // No template found
        rc = -2;
        goto cmd_open_CLOSE;
    }
  
    // 3a. Check template metadata.  Make sure it is valid.  Optional 
    // elements get filled with defaults if not present.
    obj = tmpl->child;
    while (obj != NULL) {
        cJSON* meta;
        cJSON* elem;
        int filesize = 0;
        int is_stock;

        meta = cJSON_GetObjectItemCaseSensitive(obj, "_meta");
        if (meta != NULL) {
            // ID: mandatory
            elem = cJSON_GetObjectItemCaseSensitive(meta, "id");
            if (cJSON_IsNumber(elem) == false) {
                rc = -3;
                goto cmd_open_CLOSE;
            }
            
            // Size: mandatory
            elem = cJSON_GetObjectItemCaseSensitive(meta, "size");
            if (cJSON_IsNumber(elem) == false) {
                rc = -4;
                goto cmd_open_CLOSE;
            }
            filesize = elem->valueint;
            
            // Stock: optional, default=true
            elem = cJSON_GetObjectItemCaseSensitive(meta, "stock");
            if (cJSON_IsBool(elem) == false) {
                elem = cJSON_CreateBool(true);
                cJSON_AddItemToObject(meta, "stock", elem);
                is_stock = 1;
            }
            else {
                is_stock = (elem->valueint != 0);
            }
            
            // Block: optional, default="isf"
            ///@todo resolve string vs. integer format
            elem = cJSON_GetObjectItemCaseSensitive(meta, "block");
            if (cJSON_IsString(elem) == false) {
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
            if (cJSON_IsString(elem) == false) {
                elem = cJSON_CreateString("array");
                cJSON_AddItemToObject(meta, "type", elem);
            }
            
            // Mod: optional, default=52
            elem = cJSON_GetObjectItemCaseSensitive(meta, "mod");
            if (cJSON_IsNumber(elem) == false) {
                elem = cJSON_CreateNumber(52.);
                cJSON_AddItemToObject(meta, "mod", elem);
            }
            
            // Time: optional, default=now() 
            elem = cJSON_GetObjectItemCaseSensitive(meta, "modtime");
            if (cJSON_IsNumber(elem) == false) {
                unsigned long time_val = time(NULL);
                elem = cJSON_CreateNumber((double)time_val);
                cJSON_AddItemToObject(meta, "modtime", elem);
            }
        }
        obj = obj->next;
    }
 
    // 3b. Template is valid.
    rc = otfs_init(&db);
    if (rc != 0) {
        rc = ERRCODE(otfs, otfs_init, rc);
        goto cmd_open_CLOSE;
    }
    
    // Remove scratchpad directory and all contents
    cmd_rmdir(OTDB_PARAM_SCRATCHDIR);
    if (mkdir(OTDB_PARAM_SCRATCHDIR, 0700) == 0) {
        if (mkdir(OTDB_PARAM_SCRATCHDIR"/_TMPL", 0700) == 0) {
            jst_writeout(tmpl, OTDB_PARAM_SCRATCHDIR"/_TMPL/tmpl.json");
        }
    }
    
    // 3b. Add overhead section to the allocation, Malloc the fs data
    ///@todo check if res_time0 is used correctly -- should it be pulled from
    /// json input or used like it is here?
    fshdr.ftab_alloc    = sizeof(vlFSHEADER) + sizeof(vl_header_t)*(fshdr.isf.files+fshdr.iss.files+fshdr.gfb.files);
    fshdr.res_time0     = (uint32_t)time(NULL);
    tmpl_fs->alloc      = vl_get_fsalloc(&fshdr);
    tmpl_fs->base       = talloc_zero_size(tmpl_fs, tmpl_fs->alloc);
    if (tmpl_fs->base == NULL) {
        rc = -5;
        goto cmd_open_CLOSE;
    }
    memcpy(tmpl_fs->base, &fshdr, sizeof(vlFSHEADER));
   
    gfbhdr  = (fshdr.gfb.files != 0) ? tmpl_fs->base+sizeof(vlFSHEADER) : NULL;
    isshdr  = (fshdr.iss.files != 0) ? tmpl_fs->base+sizeof(vlFSHEADER)+(fshdr.gfb.files*sizeof(vl_header_t)) : NULL;
    isfhdr  = (fshdr.isf.files != 0) ? tmpl_fs->base+sizeof(vlFSHEADER)+((fshdr.gfb.files+fshdr.iss.files)*sizeof(vl_header_t)) : NULL;
    
    // 4. Prepare the file table, except for base and length fields, which
    //    are done in 4b and 4c respectively.
    obj = tmpl->child;
    while (obj != NULL) {
        cJSON*  meta;
        vl_header_t* hdr;
        ot_uni16    idmod;

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
        ///@todo this time field is taken from the template, not the data file.
        /// That's probably ok as long as time is adjusted later.
        hdr->modtime= jst_extract_time(meta);
        hdr->mirror = 0xFFFF;
        //hdr->base = ... ;
        hdr->idmod  = idmod.ushort;
        hdr->alloc  = jst_extract_size(meta);
        //hdr->length  = ... ;

        DEBUGPRINT("File ID:%d, MOD:0%02o, Alloc:%d, Time=%d\n", idmod.ubyte[0], idmod.ubyte[1], hdr->alloc, hdr->modtime);
        obj = obj->next;
    }
  
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
              
        // Implicit params stock & type
        is_stock    = jst_extract_stock(meta);
        ctype       = jst_extract_type(meta);
        
        content     = cJSON_GetObjectItemCaseSensitive(obj, "_content");
        hdr->length = 0;
        filedata    = (uint8_t*)tmpl_fs->base + hdr->base;
        if (content != NULL) {
            // Struct type, most involved
            if (ctype == CONTENT_struct) {   
                for (content=content->child; content!=NULL; content=content->next) {
                    cJSON* submeta;
                    int offset;
                    int e_sz;
                  
                    // Nested data element has _meta field
                    ///@todo recursive treatment of nested elements
                    submeta = cJSON_GetObjectItemCaseSensitive(content, "_meta");
                    if (submeta != NULL) {
                        offset  = (int)jst_extract_pos(submeta);
                        e_sz    = jst_extract_size(submeta);
                        submeta = cJSON_GetObjectItemCaseSensitive(content, "_content");
                        if (submeta != NULL) {
                            submeta = submeta->child;
                            while (submeta != NULL) {
                                jst_load_element( &filedata[offset], 
                                        (int)e_sz, 
                                        (unsigned int)jst_extract_bitpos(submeta), 
                                        jst_extract_string(submeta, "type"), 
                                        cJSON_GetObjectItemCaseSensitive(submeta, "def"));
                                submeta = submeta->next;
                            }
                        }
                    }
                    
                    // Flat data element
                    else {
                        offset = (int)jst_extract_pos(content);
                        e_sz = jst_load_element( &filedata[offset], 
                                    (int)hdr->alloc - offset, 
                                    0, 
                                    jst_extract_string(content, "type"), 
                                    cJSON_GetObjectItemCaseSensitive(content, "def"));
                    }
                    
                    // Adjust header length to extent of maximum element
                    if ((offset+e_sz) > hdr->length) {
                        hdr->length = (offset+e_sz);
                    }
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

    // TEST PRINT THE HEADER
    DEBUG_RUN( test_dumpbytes(tmpl_fs->base, sizeof(vl_header_t), fshdr.ftab_alloc, "FS TABLE"); );

    // 5. By this point, the default FS is created based on the input 
    // template.  For each device in the imported JSON, we make a copy of
    // this template and apply changes that are present.
    
    // Go into each directory that isn't "_TMPL"
    // The pathbuf already contains the root directory, from step 2, but we 
    // need to clip the _TMPL part.
    *rtpath = 0;
    dir     = opendir(pathbuf);
    if (dir == NULL) {
        rc = -6;
        goto cmd_open_CLOSE;
    }
    entbuf = talloc_size(cmd_open_heap, dirent_buf_size(dir));
    if (entbuf == NULL) {
        rc = -6;
        goto cmd_open_CLOSE;
    }
    
    while (rc >= 0) {
        char* endptr;
  
        readdir_r(dir, entbuf, &ent);
        if (ent == NULL) {
            break;
        }
        
        // If entity is not a Directory, go to next entity in the dir
        DEBUGPRINT("%s %d :: d_name=%s\n", __FUNCTION__, __LINE__, ent->d_name);           
        if (ent->d_type != DT_DIR) {
            continue;
        }
        
        // Name of directory should be a pure hex number: skip others
        endptr = NULL;
        data_fs.uid.u64 = strtoull(ent->d_name, &endptr, 16);
        if (((*ent->d_name != '\0') && (*endptr == '\0')) == 0) {
            continue;
        }
        
        // Create new FS using defaults from template
        ///@note data_fs goes on the permanent memory context
        data_fs.alloc   = tmpl_fs->alloc;
        data_fs.base    = talloc_size(dth->pctx, tmpl_fs->alloc);
        if (data_fs.base == NULL) {
            rc = -7;
        }
        else {
            memcpy(data_fs.base, tmpl_fs->base, tmpl_fs->alloc);
        
            // Create new FS based on device id and template FS
            DEBUGPRINT("%s %d :: ID=%"PRIx64"\n", __FUNCTION__, __LINE__, data_fs.uid.u64);
            rc = otfs_new(db, &data_fs);
            if (rc != 0) {
                rc = ERRCODE(otfs, otfs_new, rc);
            }
            else {
                // Enter Device Directory: max is 16 hex chars long (8 bytes)
                snprintf(rtpath, 16, "/%s", ent->d_name);
                DEBUGPRINT("%s %d :: devdir=%s\n", __FUNCTION__, __LINE__, pathbuf);
                devdir = opendir(pathbuf);
                if (devdir == NULL) {
                    rc = -8;
                }
                else {
                    ///@todo verify that final argument is indeed what we want to do
                    rc = cmdsub_datafile(dth, dst, dstmax, tmpl, devdir, pathbuf, data_fs.uid.u64, true);
                    closedir(devdir);
                    devdir = NULL;
                }
            }
            
            // free data_fs.base allocation if there's an error
            if (rc != 0) {
                DEBUGPRINT("%s %d :: rc=%i\n", __FUNCTION__, __LINE__, rc);
                otfs_del(db, &data_fs, &sub_tfree);
                //talloc_free(data_fs.base);    // done by otfs_del() above
                DEBUGPRINT("%s %d :: otfs_del() passed\n", __FUNCTION__, __LINE__);
            }
        }
    }
    closedir(dir);
    dir = NULL;
    
//if (gfbhdr != NULL) {
//fprintf(stderr, "GFB BASE = %u\n", gfbhdr[0].base);
//test_dumpbytes(data_fs.base+gfbhdr[0].base, 16, fshdr.gfb.alloc, "GFB DATA");
//}
//if (isshdr != NULL) {
//fprintf(stderr, "ISS BASE = %u\n", isshdr[0].base);
//test_dumpbytes(data_fs.base+isshdr[0].base, 16, fshdr.iss.alloc, "ISS DATA");
//}
//if (isfhdr != NULL) {
//fprintf(stderr, "ISF BASE = %u\n", isfhdr[0].base);
//test_dumpbytes(data_fs.base+isfhdr[0].base, 16, fshdr.isf.alloc, "ISF DATA");
//}
    
    cmd_open_CLOSE:
    DEBUGPRINT("%s %d :: cmd_open_CLOSE\n", __FUNCTION__, __LINE__);
    // ------------------------------------------------------------------------
    // 8. Close and Free all dangling memory elements
    if (rc >= 0) {
        /// The JSON template is allocated in a local context.  This is usually
        /// a memory pool that gets overwritten frequently.  We need to copy
        /// this local template to the permanent context.
        if (tmpl != NULL) {
            size_t tmpl_size;
            void*  tmpl_block;
            
            DEBUGPRINT("%s %d :: cJSON_GetObjectSize\n", __FUNCTION__, __LINE__);
            tmpl_size = cJSON_GetObjectSize(tmpl);
            DEBUGPRINT("%s %d :: tmpl_size=%zu\n", __FUNCTION__, __LINE__, tmpl_size);
            if (tmpl_size != 0) {
                tmpl_block = talloc_size(dth->pctx, tmpl_size);
                if (tmpl_block == NULL) {
                    ///@todo error code
                    rc = -9;
                    goto cmd_open_ENDGAME;
                }
                
                DEBUGPRINT("%s %d :: cJSON_BlockDup\n", __FUNCTION__, __LINE__);
                tmpl_export = cJSON_BlockDup(tmpl_block, tmpl_size, tmpl);
                DEBUGPRINT("%s %d :: tmpl_export=%016llx\n", __FUNCTION__, __LINE__, (uint64_t)tmpl_export);
                if (tmpl_export == NULL) {
                    ///@todo error code
                    rc = -10;
                    goto cmd_open_ENDGAME;
                }
            }
            
            DEBUGPRINT("%s %d :: cJSON_Delete\n", __FUNCTION__, __LINE__);
            cJSON_Delete(tmpl);
        }
        
        /// Delete the existing database if one exists.  Then link the newly
        /// opened database to the global context.
        if (dth->ext->db != NULL) {
            DEBUGPRINT("%s %d :: otfs_deinit\n", __FUNCTION__, __LINE__);
            rc = otfs_deinit(dth->ext->db, &sub_tfree);
            if (rc != 0) {
                rc = ERRCODE(otfs, otfs_deinit, rc);
            }
        }
    }
    
    cmd_open_ENDGAME:
    DEBUGPRINT("%s %d :: cmd_open_ENDGAME\n", __FUNCTION__, __LINE__);
    
    /// Database open operation either succeeded or failed.  Open operation is
    /// ATOMIC, so the template (JSON) and database (BINARY) are only attached
    /// if open was 100% successful.  Otherwise, the intermediate changes are
    /// discarded -- old data remains.
    ///
    /// On successful open, the data could be pushed (using "push" command) to
    /// the device manager.  Instead of this, we now use an init file that will
    /// run open, push, etc, on startup.
    ///
    /// @note Exported template is allocated on "pctx" (permanent context)
    ///
    /// @todo Have Judy Table (db) also get contextually allocated.  This will
    ///       require modifications to Judy library.  The linked FS data is
    ///       already contextually allocated.
    ///
    if (rc >= 0) {
        dth->ext->db = db;
        talloc_free(dth->ext->tmpl);
        dth->ext->tmpl = tmpl_export;
        
        talloc_free(dth->ext->tmpl_fs);
        dth->ext->tmpl_fs = tmpl_fs;
        
//        if (dth->ext->devmgr != NULL) {
//            uint8_t pushargs[] = "";
//            int argslen = sizeof("");
//            cmd_push(dth, dst, &argslen, pushargs, dstmax);
//        }
    }
    else {
        talloc_free(tmpl_fs);
        cJSON_Delete(tmpl);
        otfs_deinit(db, &sub_tfree);
    }
    
    cJSON_Delete(data);
    if (devdir != NULL) closedir(devdir);
    if (dir != NULL)    closedir(dir);
    
    talloc_free(cmd_open_heap);
    
    return cmd_jsonout_err((char*)dst, dstmax, (bool)arglist.jsonout_flag, rc, "open");
}







int cmd_load(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    
    // POSIX Filesystem and JSON handles
    DIR* dir                = NULL;
    DIR* devdir             = NULL;
    struct dirent *ent      = NULL;
    struct dirent *entbuf   = NULL;
    char* endptr;
    TALLOC_CTX* cmd_load_heap;
    
    otfs_id_union active_id;
    
    cmd_arglist_t arglist = {
        .fields = ARGFIELD_JSONOUT | ARGFIELD_DEVICEIDLIST | ARGFIELD_ARCHIVE,
    };
    void* args[] = {help_man, jsonout_opt, archive_man, devidlist_opt, end_man};
    
    ///@todo do input checks!!!!!!
    
    /// Make sure there is something to load
    if ((dth->ext->tmpl == NULL) || (dth->ext->db == NULL)) {
        return -1;
    }
    
    cmd_load_heap = talloc_new(dth->tctx);
    if (cmd_load_heap == NULL) {
    ///@todo better error code for out of memory
        return -1;
    }
    
    /// Extract arguments into arglist struct
    rc = cmd_extract_args(&arglist, args, "load", (const char*)src, inbytes);
    if (rc != 0) {
        rc = -2;
        goto cmd_load_END;
    }
    DEBUGPRINT("cmd_load():\n  devid_list=(%d items)\n  archive=%s\n", arglist.devid_strlist_size, arglist.archive_path);
    
    /// 1. Determine how to use the command line input to update the database
    /// 2. ...
    
    // 1. Determine how to use the command line input to update the database
    // There are several possibilities
    // A. The input folder can specify a device.
    // B. The input folder can contain subfolders, each specifying a device
    // C. The input device list can be empty
    // D. The input device list can specify devices
    //
    // If the device list is empty, OTDB will implicitly use all the devices
    // specified by archive input folders
    
    dir = opendir(arglist.archive_path);
    if (dir == NULL) {
        rc = -3;
        goto cmd_load_CLOSE;
    }
    entbuf = talloc_size(cmd_load_heap, dirent_buf_size(dir));
    if (entbuf == NULL) {
        rc = -3;
        goto cmd_load_CLOSE;
    }
    
    endptr = NULL;
    active_id.u64 = strtoull(arglist.archive_path, &endptr, 16);
    if ((*endptr == '\0') && (*arglist.archive_path != '\0')) {
        // the input archive folder is a hex number
        if (arglist.devid_strlist_size > 0) {
            if (uid_in_list(arglist.devid_strlist, arglist.devid_strlist_size, active_id.u64) == false) {
                rc = -4;
                goto cmd_load_CLOSE;
            }
        }
        
        // Activate the chosen ID.  If it is not in the database, skip it.
        if (otfs_setfs(dth->ext->db, NULL, &active_id.u8[0]) == 0) {
            rc = cmdsub_datafile(dth, dst, dstmax, dth->ext->tmpl, devdir, arglist.archive_path, active_id.u64, false);
        }
    }
    else {
        char pathbuf[256];
        char* rtpath;
    
        // the input archive folder is not a hex number: must contain subfolders
        
        // Go into each directory that isn't "_TMPL"
        // The pathbuf already contains the root directory, from step 2, but we
        // need to clip the _TMPL part.
        rtpath  = stpncpy(pathbuf, arglist.archive_path, (sizeof(pathbuf) - (32+1)) );
        *rtpath = 0;
        
        while (rc == 0) {
            readdir_r(dir, entbuf, &ent);
            if (ent == NULL) {
                break;
            }
            
            // If entity is not a Directory, go to next entity in the dir
            DEBUGPRINT("%s %d :: d_name=%s\n", __FUNCTION__, __LINE__, ent->d_name);
            if (ent->d_type != DT_DIR) {
                continue;
            }
            
            // Name of directory must be a pure hex number.
            endptr          = NULL;
            active_id.u64   = strtoull(ent->d_name, &endptr, 16);
            if (((*ent->d_name != '\0') && (*endptr == '\0')) == 0) {
                continue;
            }
            
            // If there is a device ID list, we need to make sure the UID of the
            // input folder is in the list.  If there is no device ID list, we
            // use all input folders.
            if (arglist.devid_strlist_size > 0) {
                if (uid_in_list(arglist.devid_strlist, arglist.devid_strlist_size, active_id.u64) == false) {
                    continue;
                }
            }
            
            // - Enter Device Directory: max is 16 hex chars long (8 bytes)
            // - Activate the chosen ID.  If it is not in the database, skip it.
            snprintf(rtpath, 16, "/%s", ent->d_name);
            DEBUGPRINT("%s %d :: devdir=%s\n", __FUNCTION__, __LINE__, pathbuf);
            devdir = opendir(pathbuf);
            if (devdir == NULL) {
                rc = -8;
            }
            else {
                if (otfs_setfs(dth->ext->db, NULL, &active_id.u8[0]) == 0) {
                    rc = cmdsub_datafile(dth, dst, dstmax, dth->ext->tmpl, devdir, pathbuf, active_id.u64, false);
                }
                closedir(devdir);
                devdir = NULL;
            }
        }
    }
    
    // 7. Close and Free all dangling memory elements
    cmd_load_CLOSE:
    if (devdir != NULL) closedir(devdir);
    if (dir != NULL)    closedir(dir);
    
    cmd_load_END:
    talloc_free(cmd_load_heap);
    return cmd_jsonout_err((char*)dst, dstmax, arglist.jsonout_flag, rc, "load");
}



