//
//  pktlist.h
//  otter
//
//  Created by John Peter Norair on 18/7/17.
//  Copyright © 2017 JP Norair (Indigresso). All rights reserved.
//

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
