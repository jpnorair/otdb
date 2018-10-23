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

#ifndef debug_h
#define debug_h

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "cliopt.h"

/// Set __DEBUG__ during compilation to enable debug features (mainly printing)

#define _HEX_(HEX, SIZE, ...)  do { \
    fprintf(stderr, __VA_ARGS__); \
    for (int i=0; i<(SIZE); i++) {   \
        fprintf(stderr, "%02X ", (HEX)[i]);   \
    } \
    fprintf(stderr, "\n"); \
} while (0)



#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

/*
Black        0;30     Dark Gray     1;30
Red          0;31     Light Red     1;31
Green        0;32     Light Green   1;32
Brown/Orange 0;33     Yellow        1;33
Blue         0;34     Light Blue    1;34
Purple       0;35     Light Purple  1;35
Cyan         0;36     Light Cyan    1;36
Light Gray   0;37     White         1;37
*/

#if defined(__DEBUG__)
#   define DEBUG_PRINTF(...)    do { if (cliopt_isdebug()) fprintf(stderr, KYEL "DEBUG: " KNRM __VA_ARGS__); } while(0)
#   define TTY_PRINTF(...)      do { if (cliopt_isdebug()) fprintf(stderr, "TTY: " __VA_ARGS__); } while(0)
#   define TTY_TX_PRINTF(...)   do { if (cliopt_isdebug()) fprintf(stderr, "TTY_TX: " __VA_ARGS__); } while(0)
#   define TTY_RX_PRINTF(...)   do { if (cliopt_isdebug()) fprintf(stderr, "TTY_RX: " __VA_ARGS__); } while(0)
#   define HEX_DUMP(LABEL, HEX, ...) do { if (cliopt_isdebug()) _HEX_(HEX, SIZE, ...) } while(0)

#else
#   define DEBUG_PRINTF(...)    do { } while(0)
#   define TTY_PRINTF(...)      do { } while(0)
#   define TTY_TX_PRINTF(...)   do { } while(0)
#   define TTY_RX_PRINTF(...)   do { } while(0)
#   define HEX_DUMP(LABEL, HEX, ...) do { } while(0)

#endif




#endif /* cliopt_h */
