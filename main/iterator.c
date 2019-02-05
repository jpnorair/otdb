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
#include "iterator.h"
#include "cliopt.h"
#include "otdb_cfg.h"
#include "debug.h"

// HB Headers/Libraries


// Standard C & POSIX Libraries
#include <stdio.h>
#include <string.h>

#if 0 //OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif



static int sub_nextdevice(void* handle, otfs_t** outfs, int* devid_i, const char** strlist, size_t listsz) {
    int devtest = 1;
    uint64_t uid;

    for (; (devtest!=0) && (*devid_i<listsz); (*devid_i)++) {
        DEBUGPRINT("%s %d :: devid[%i] = %s\n", __FUNCTION__, __LINE__, *devid_i, strlist[*devid_i]);
        uid = strtoull(strlist[*devid_i], NULL, 16);
        devtest = otfs_setfs(handle, outfs, (uint8_t*)&uid);
    }
    
    return devtest;
}



int iterator_uids(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t** src, size_t dstmax,
                cmd_arglist_t* arglist, iteraction_t action) {
    int devtest;
    int devid_i = 0;
    int count;
    int outbytes = 0;
    otfs_t* devfs;
    int dstlimit;

    if (arglist->devid_strlist_size > 0) {
        devtest = sub_nextdevice(dth->ext, &devfs, &devid_i, arglist->devid_strlist, arglist->devid_strlist_size);
    }
    else {
        uint64_t uid = 0;
        devtest = otfs_iterator_start(dth->ext, &devfs, (uint8_t*)&uid);
    }
    
    dstlimit = (int)dstmax;
    count = 0;
    while (devtest == 0) {
        int newbytes;
        count++;
        
        newbytes    = action(dth, dst, inbytes, src, (size_t)dstlimit, count, arglist, devfs);
        dstlimit   -= newbytes;
        dst        += newbytes;
        
        if (newbytes < 0) {
            outbytes = -3;
            goto iterator_EXIT;
        }
        if (dstlimit < 0 ) {
            outbytes = -4;
            goto iterator_EXIT;
        }
        
        if (arglist->devid_strlist_size > 0) {
            devtest = sub_nextdevice(dth->ext, &devfs, &devid_i, arglist->devid_strlist, arglist->devid_strlist_size);
        }
        else {
            uint64_t uid = 0;
            devtest = otfs_iterator_next(dth->ext, &devfs, (uint8_t*)&uid);
        }
    }

    iterator_EXIT:
    return outbytes;
}
