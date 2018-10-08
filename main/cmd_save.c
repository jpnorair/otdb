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





int cmd_save(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
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
    
    /// On successful extraction, create a new device in the database
    if (rc == 0) {
        DEBUGPRINT("cmd_open():\n  compress=%d\n  archive=%s\n", 
                arglist.compress_flag, arglist.archive_path);
                
        ///@todo implementation
        ///
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
        /// Step 2: Get the optional file lists
        /// ID's for ISF files beyond the stock files are stored in ISF-7
        /// ID's for ISS files beyond the stock files are stored in ISF-8
        /// ID's for GFB files beyond the stock files are stored in ISF-9
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
    
    return rc;
}



