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

#ifndef pktlist_h
#define pktlist_h

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include <pthread.h>


typedef struct pkt {
    uint8_t*    buffer;
    void*       addr;
    size_t      size;
    time_t      tstamp;
    struct pkt  *prev;
    struct pkt  *next;
} pkt_t;


typedef void (*pkt_cb)(pkt_t*);



typedef struct {
    pkt_t*  front;
    pkt_t*  last;
    pkt_t*  cursor;
    pkt_t*  marker;
    size_t  size;
    pkt_cb  on_add;
    
    pthread_mutex_t mutex;
} pktlist_t;





// Packet List Manipulation Functions
int pktlist_init(pktlist_t* plist, pkt_cb on_add);
int pktlist_add(pktlist_t* list, void* addr, uint8_t* data, size_t size);
int pktlist_del(pktlist_t* plist, pkt_t* pkt);
pkt_t* pktlist_get(pktlist_t* plist);



#endif /* pktlist_h */
