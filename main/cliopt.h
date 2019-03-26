/* Copyright 2017, JP Norair
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

#ifndef cliopt_h
#define cliopt_h

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    INTF_interactive  = 0,
    INTF_pipe = 1,
    INTF_socket = 2,
    INTF_max
} INTF_Type;

typedef enum {
    FORMAT_Dynamic  = 0,
    FORMAT_Bintex   = 1,
    FORMAT_Hex8     = 2,
    FORMAT_Hex16    = 3,
    FORMAT_Hex32    = 4
} FORMAT_Type;

typedef struct {
    bool        verbose_on;
    bool        debug_on;
    FORMAT_Type format;
    INTF_Type   intf;
    
    size_t      mempool_size;
    int         timeout_ms;
} cliopt_t;


cliopt_t* cliopt_init(cliopt_t* new_master);

bool cliopt_isverbose(void);
bool cliopt_isdebug(void);

FORMAT_Type cliopt_getformat(void);

INTF_Type cliopt_getintf(void);

size_t cliopt_getpoolsize(void);
void cliopt_setpoolsize(size_t poolsize);

int cliopt_gettimeout(void);
void cliopt_settimeout(int timeout_ms);






#endif /* cliopt_h */
