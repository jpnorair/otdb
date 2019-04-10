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
#include "otdb_cfg.h"
#include "debug.h"

// HB Headers/Libraries


// Standard C & POSIX Libraries

#include <stdio.h>
#include <string.h>
#include <stdarg.h>


#if 0 //OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif





int dm_printf(dterm_handle_t* dth, uint8_t* dst, size_t dstmax, const char* restrict fmt, ...) {
    int inbytes = 0;
    int rc;
    va_list vargs;
    
    va_start(vargs, fmt);
    inbytes = vsnprintf((char*)dst, dstmax, fmt, vargs);
    va_end(vargs);
    
    if (inbytes <= 0) {
        rc = inbytes;
    }
    else {
        rc = cmd_devmgr(dth, (uint8_t*)dst, &inbytes, (uint8_t*)dst, dstmax);
    }
    
    return rc;
}



int dm_xnprintf(dterm_handle_t* dth, uint8_t* dst, size_t dstmax, AUTH_level auth, uint64_t uid, const char* restrict fmt, ...) {
    static const char* auth_guest = "guest";
    static const char* auth_user = "user";
    static const char* auth_root = "root";
    const char* auth_str;
    char* pcurs;
    int psize;
    int plimit;
    va_list vargs;
    
    switch (auth) {
        case AUTH_root: auth_str = auth_root;   break;
        case AUTH_user: auth_str = auth_user;   break;
        default:        auth_str = auth_guest;  break;
    }
    
    pcurs   = (char*)dst;
    plimit  = (int)dstmax;
    psize   = 0;
    
    psize   = snprintf(pcurs, plimit, "xnode %s [", auth_str);
    plimit -= psize;
    pcurs  += psize;
    if (plimit < 0) {
        return -4;      ///@todo codify an error for buffer overflow
    }
    
    psize   = cmd_hexnwrite(pcurs, (uint8_t*)&uid, 8, plimit);
    plimit -= psize;
    pcurs  += psize;
    if (plimit < 0) {
        return -4;      ///@todo codify an error for buffer overflow
    }
    
    psize   = snprintf(pcurs, plimit, "] \"");
    plimit -= psize;
    pcurs  += psize;
    if (plimit < 0) {
        return -4;      ///@todo codify an error for buffer overflow
    }
    
    va_start(vargs, fmt);
    psize = vsnprintf(pcurs, plimit, fmt, vargs);
    va_end(vargs);
    
    plimit -= psize;
    pcurs  += psize;
    if (plimit < 0) {
        return -4;      ///@todo codify an error for buffer overflow
    }
    
    ///@todo compiler screams at this, but it works
    psize   = snprintf(pcurs, plimit, "\"\0");
    plimit -= psize;
    pcurs  += psize;
    //*pcurs++    = '"';
    //*pcurs++    = 0;
    //plimit     -= 2;
    
    if (plimit < 0) {
        return -4;      ///@todo codify an error for buffer overflow
    }
    
    psize = (int)((uint8_t*)pcurs - dst);
    
    return cmd_devmgr(dth, (uint8_t*)dst, &psize, (uint8_t*)dst, dstmax);
}

