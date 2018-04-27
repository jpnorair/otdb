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

#ifndef ppipe_h
#define ppipe_h

#include <stdio.h>


typedef struct {
    char*   fpath;
    FILE*   file;
    int     fd;
    int     fmode;
} ppipe_fifo_t;

typedef struct {
    char            basepath[256];
    ppipe_fifo_t*   fifo;
    size_t          num;
} ppipe_t;



int ppipe_init(ppipe_t* pipes, const char* basepath);

void ppipe_deinit(ppipe_t* pipes);

int ppipe_new(ppipe_t* pipes, const char* prefix, const char* name, const char* fmode);

int ppipe_del(ppipe_t* pipes, int ppd);

const char* ppipe_getpath(ppipe_t* pipes, int ppd);




/// Debugging
void ppipe_print(ppipe_t* pipes);




/// Possibly Deprecated
FILE* ppipe_getfile(ppipe_t* pipes, int ppd);





/// Deprecated
ppipe_t* ppipe_ref(ppipe_t* pipes);





#endif /* ppipe_h */
