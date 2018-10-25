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

#ifndef popen2_h
#define popen2_h

#include <stdio.h>

// For pid_t
#include <unistd.h>
#include <sys/types.h>

typedef struct {
    pid_t   pid;
    int     fd_writeto;
    int     fd_readfrom;
} childproc_t;


int popen2_s(childproc_t* childproc, const char* cmdline);

void popen2_kill_s(childproc_t* childproc);

pid_t popen2(const char* cmdline, int* fd_tochild, int* fd_fromchild);

void popen2_kill(pid_t pid, int fd_tochild, int fd_fromchild);



#endif /* popen2_h */
