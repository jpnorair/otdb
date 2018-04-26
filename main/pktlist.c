//
//  pktlist.c
//  otter
//
//  Created by John Peter Norair on 18/7/17.
//  Copyright © 2017 JP Norair (Indigresso). All rights reserved.
//

#include "pktlist.h"
#include "cliopt.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <assert.h>


void sub_init_boundaries(pktlist_t* plist) {
    plist->front    = NULL;
    plist->last     = NULL;
    plist->cursor   = NULL;
    plist->marker   = NULL;
    plist->size     = 0;
}


int pktlist_init(pktlist_t* plist, pkt_cb on_add) {
    sub_init_boundaries(plist);
    
    plist->on_add = on_add;
    
    assert( pthread_mutex_init(&plist->mutex, NULL) == 0 );
    
    return 0;
}



int pktlist_add(pktlist_t* plist, void* addr, uint8_t* data, size_t size) {
    pkt_t* newpkt;
    int rc;
    
    if (plist == NULL) {
        return -1;
    }
    
    newpkt = malloc(sizeof(pkt_t));
    if (newpkt == NULL) {
        return -2;
    }

    pthread_mutex_lock(&plist->mutex);

    // Setup list connections for the new packet
    // Also allocate the buffer of the new packet
    ///@todo Change the hardcoded +8 to a dynamic detection of the header
    ///      length, which depends on current mode settings in the "cli".
    ///      Dynamic header isn't implemented yet, so no rush.
    newpkt->prev    = plist->last;
    newpkt->next    = NULL;
    newpkt->size    = size;
    newpkt->buffer  = malloc(size);
    if (newpkt->buffer == NULL) {
        rc = -3;
        goto pktlist_add_END;
    }
    
    // Copy input data into the packet buffer
    memcpy(newpkt->buffer, data, size);
    
    // Save timestamp: this may or may not get used, but it's saved anyway.
    // The default sequence (which is available to header generation) is
    // from the rotating nonce of the plist.
    newpkt->tstamp  = time(NULL);
    newpkt->addr    = addr;
    
    // List is empty, so start the list
    if (plist->last == NULL) {
        plist->size         = 0;
        plist->front        = newpkt;
        plist->last         = newpkt;
        plist->cursor       = newpkt;
        plist->marker       = newpkt;
    }
    // List is not empty, so simply extend the list.
    // set the cursor to the new packet if it points to NULL (end)
    else {
        newpkt->prev->next  = newpkt;
        plist->last         = newpkt;
        
        if (plist->cursor == NULL) {
            plist->cursor   = newpkt;
        }
    }

    // Increment the list size to account for new packet
    plist->size++;
    
    // If the callback is specified, call it.
    if (plist->on_add != NULL) {
        plist->on_add(newpkt);
    }
    
    rc = (int)plist->size;
    
    pktlist_add_END:
    pthread_mutex_unlock(&plist->mutex);
    return rc;
}



int pktlist_del(pktlist_t* plist, pkt_t* pkt) {
    pkt_t*  ref;
    pkt_t   copy; 
    
    
    
    /// First thing is to free the packet even if it's not in the list
    /// We make a local copy in order to stitch the list back together.
    if (plist == NULL) {
        return -1;
    }
    if (pkt == NULL) {
        return -2;
    }
    
    pthread_mutex_lock(&plist->mutex);
    
    /// Handle idiot case (trying to delete from empty list).
    if (plist->size == 0) {
        sub_init_boundaries(plist);
        goto pktlist_del_END;
    }
    
    /// Free data held by the pkt_t
    ref     = pkt;
    copy    = *pkt;
    if (pkt->buffer != NULL) {
        free(pkt->buffer);
    }
    free(pkt);

    /// Downsize the list.  Re-init list boundaries if size == 0;
    plist->size--;
    if (plist->size == 0) {
        sub_init_boundaries(plist);
        goto pktlist_del_END;
    }
    
    /// If packet was front of list, move front to next,
    if (plist->front == ref) {
        plist->front = copy.next;
    }
    
    /// If packet was last of list, move last to prev
    if (plist->last == ref) {
        plist->last = copy.prev;
    }
    
    /// Likewise, if the cursor and marker were on the packet, advance them
    if (plist->cursor == pkt) {
        plist->cursor = copy.next;
    }
    if (plist->marker == pkt) {
        plist->marker = copy.next;
    }
    
    /// Stitch the list back together
    if (copy.next != NULL) {
        copy.next->prev = copy.prev;
    }
    if (copy.prev != NULL) {
        copy.prev->next = copy.next;
    }

    pktlist_del_END:
    pthread_mutex_unlock(&plist->mutex);
    return 0;
}



pkt_t* pktlist_get(pktlist_t* plist) {

    // packet list is not allocated -- that's a serious error
    if (plist == NULL) {
        return NULL;
    }
    
    return plist->cursor;
}



