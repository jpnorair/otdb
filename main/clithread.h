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

#ifndef clithread_h
#define clithread_h

// Configuration Header
#include "otdb_cfg.h"
#include "cliopt.h"

#include <pthread.h>

typedef struct ptlist {
    pthread_t client;
    struct ptlist* prev;
    struct ptlist* next;
} clithread_item_t;

typedef struct {
    int fd_in;
    int fd_out;
    void* ext;
} clithread_args_t;

typedef clithread_item_t** clithread_handle_t;


clithread_handle_t clithread_init(void);

clithread_item_t* clithread_add(clithread_handle_t handle, const pthread_attr_t* attr, void* (*start_routine)(void*), void* arg);

void clithread_del(clithread_item_t* item);

void clithread_deinit(clithread_handle_t handle);





#endif
