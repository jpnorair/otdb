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

// HB Headers/Libraries


// Standard C & POSIX Libraries
#include <stdio.h>
#include <stdarg.h>




int dm_printf(dterm_handle_t* dth, uint8_t* dst, size_t dstmax, const char* restrict fmt, ...);


int dm_xnprintf(dterm_handle_t* dth, uint8_t* dst, size_t dstmax, AUTH_level auth, uint64_t uid, const char* restrict fmt, ...);


