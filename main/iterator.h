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

// HB Library
#include <otfs.h>

// Standard C & POSIX Libraries
#include <stdio.h>
#include <string.h>


typedef int (*iteraction_t)(dterm_handle_t*, uint8_t*, int*, uint8_t**, size_t, int, cmd_arglist_t*, otfs_t*);





int iterator_uids(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t** src, size_t dstmax,
                cmd_arglist_t* arglist, iteraction_t action);
