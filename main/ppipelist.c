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
/** 
  * 
  * @description ppipelist module maintains a list of ppipes.  In fact, it's
  *              just a different API for the ppipe array.  It requires ppipe.
  *              ppipelist is searchable.
  *
  * @note the present implementation utilizes only the ppipe data structure and
  *       performs linear search on the dataset, without an index.  In later
  *       implementations, an index will be created and binary search (or like)
  *       will be implemented within ppipelist.
  */


#include "ppipelist.h"
#include "ppipe.h"

#include "pktlist.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/poll.h>


static ppipe_t  pplist;




int ppipelist_init(const char* basepath) {
    return ppipe_init(&pplist, basepath);
}

void ppipelist_deinit(void) {
    return ppipe_deinit(&pplist);
}


int sub_addpipe(const char* prefix, const char* name, const char* fmode) {
    int rc;
    ppipe_t*        ppipe;
    ppipe_fifo_t*   newfifo;
    
    rc = ppipe_new(&pplist, prefix, name, fmode);
    if (rc == 0) {
        ppipe = ppipe_ref(&pplist);
        if (ppipe != NULL) {
            newfifo = &ppipe->fifo[ppipe->num-1];
            
            ///@todo add the node to index
        }
    }
    
    return rc;
}



int ppipelist_new(const char* prefix, const char* name, const char* fmode) {
    int rc;
    ppipe_t*        ppipe;
    ppipe_fifo_t*   newfifo;
    
    rc = ppipe_new(&pplist, prefix, name, fmode);
    if (rc == 0) {
        ppipe = ppipe_ref(&pplist);
        if (ppipe != NULL) {
            newfifo = &ppipe->fifo[ppipe->num-1];
            
            ///@todo add the node to index
        }
    }
    
    return rc;
}


int ppipelist_search(ppipe_fifo_t** dst, const char* prefix, const char* name) {
    return ppipe_searchname(&pplist, dst, prefix, name);
}

int ppipelist_pollfds(struct pollfd** pollfd_list) {
    int list_sz;
    ppipe_fifo_t** fifolist;
    
    if (pollfd_list == NULL) {
        return -1;
    }
    if (pplist.num == 0) {
        return 0;
    }
    
    fifolist = malloc(pplist.num * sizeof(ppipe_fifo_t));
    list_sz = ppipe_searchmode(&pplist, fifolist, pplist.num, O_RDONLY);
    if (list_sz < 0) {
        list_sz -= 10;
    }
    else if (list_sz > 0) {
        // Client must free pollfd_list!
        *pollfd_list = malloc(list_sz * sizeof(struct pollfd));
        for (int i=0; i<list_sz; i++) {
            (*poll_list)[i].events = ;
            (*poll_list)[i].revents = ;
            
        }
    }
    
    ppipelist_EXIT:
    free(fifolist);
    return list_sz;
}


int ppipelist_del(const char* prefix, const char* name) {
    ppipe_fifo_t*   delfifo;
    int             ppd;
    
    ppd = ppipelist_search(&delfifo, prefix, name);
    
    if (ppd >= 0) {
        ppipe_del(&pplist, ppd);
        
        ///@todo remove from ppipelist
    }
    
    return ppd;
}


































int sub_put(const char* prefix, const char* name, uint8_t* hdr, uint8_t* src, size_t size) {
/// Process for opening and writing to FIFO involves doing a test open in 
/// non-blocking mode to make sure there is a consumer for the write.
/// Header (hdr) is 3 bytes and ignored if NULL.
    ppipe_fifo_t*   fifo;
    int             fd;
    
    ppipelist_search(&fifo, prefix, name);

    if (fifo != NULL) {
        //errno = 0;
        fd = open(fifo->fpath, O_WRONLY|O_NONBLOCK);
        if (fd > 0) {
            close(fd);
            fd = open(fifo->fpath, O_WRONLY);
            
            if (hdr != NULL) {
                write(fd, hdr, 3);
            }
            write(fd, src, size);
            close(fd);
            return 0;
        }
    }
    
    return -1;
}



int ppipelist_putbinary(const char* prefix, const char* name, uint8_t* src, size_t size) {
    uint8_t hdr[3];
    hdr[0]  = 0;
    hdr[1]  = (size >> 8) & 0xFF;
    hdr[2]  = (size >> 0) & 0xFF;
    return sub_put(prefix, name, hdr, src, size);
}



int ppipelist_puttext(const char* prefix, const char* name, char* src, size_t size) {
    return sub_put(prefix, name, NULL, (uint8_t*)src, size);
}



uint8_t* sub_gethex(uint8_t* dst, uint8_t input) {
    static const char convert[] = "0123456789ABCDEF";
    dst[0]  = convert[input >> 4];
    dst[1]  = convert[input & 0x0f];
    return dst;
}



int ppipelist_puthex(const char* prefix, const char* name, char* src, size_t size) {
/// This is a variant of sub_put()
    ppipe_fifo_t*   fifo;
    int             fd;

    ppipelist_search(&fifo, prefix, name);

    if (fifo != NULL) {
        //errno = 0;
        fd = open(fifo->fpath, O_WRONLY|O_NONBLOCK);
        
        if (fd > 0) {
            close(fd);
            fd = open(fifo->fpath, O_WRONLY);
            while (size-- != 0) {
                uint8_t hexbuf[2];
                write(fd, sub_gethex(hexbuf, *src++), 2);
            }
            
            close(fd);
            return 0;
        }
    }
    
    return -1;
}





















static const uint8_t hexlut0[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 16, 32, 48, 64, 80, 96,112,128,144, 0, 0, 0, 0, 0, 0, 
    0,160,176,192,208,224,240,  0,  0,  0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,160,176,192,208,224,240,  0,  0,  0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const uint8_t hexlut1[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 
    0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
   


int pipe_getbinary(struct pollfd *pubfd, uint8_t* dst, size_t max) {
/// Get binary not supported yet.  Code below is legacy code from
/// the project that was inherited from
    return -1;
/*
    int pollcode;
    int hdrbytes = 2;
    uint8_t header[2];
    uint8_t* cursor;
    size_t length;
    
    cursor = header;
    while (hdrbytes > 0) { 
        int new_bytes;
        
        pollcode = poll(pubfd, 1, 10);
        if (pollcode <= 0) {
            goto pipe_getbinary_SCRAP;
        }
        else if (pubfd[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            goto pipe_getbinary_SCRAP;
        }
    
        new_bytes   = (int)read(pubfd->fd, cursor, hdrbytes);
        cursor     += new_bytes;
        hdrbytes   -= new_bytes;
    }
    
    length = (header[1] * 256) + header[0];
    
    if (length > max) {
        ///@todo send warning
        length = max;
    }
    
    hdrbytes = (int)length;
    while (hdrbytes > 0) { 
        int new_bytes;
        
        pollcode = poll(pubfd, 1, 10);
        if (pollcode <= 0) {
            goto pipe_getbinary_SCRAP;
        }
        else if (pubfd[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            goto pipe_getbinary_SCRAP;
        }
    
        new_bytes   = (int)read(pubfd->fd, dst, hdrbytes);
        dst        += new_bytes;
        hdrbytes   -= new_bytes;
    }

    return (int)length;
    
    pipe_getbinary_SCRAP:
    return pollcode;
*/
}



int ppipelist_gethex(pktlist_t* plist, uint8_t* dst, size_t max) {
/// Returns when any one of the fd's supplied yields data or error.
    int pollcode;
    uint8_t* start;
    uint8_t* end;
    uint8_t byte;
    uint8_t hexbuf[2];
    
    start = dst;
    end = dst + max;
    
    pollcode = poll(pubfd, 1, 100);
    
    // Poll yields an error, do nothing but return error
    if (pollcode <= 0) {
        goto pipe_gethex_SCRAP;
    }
    else {
    
    
        if (pubfd[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        goto pipe_gethex_SCRAP;
    }
    
    while ( (read(pubfd->fd, hexbuf, 2) > 0) && (max--) ) {
        byte    = hexlut0[(hexbuf[0]&0x7f)];
        byte   += hexlut1[(hexbuf[1]&0x7f)];
        *dst++  = byte;
    }

    return (int)(dst - start);
    
    pipe_gethex_SCRAP:
    return pollcode;
}







int pipe_gettext(struct pollfd *pubfd, uint8_t* dst, size_t max) {
    int pollcode;
    uint8_t* start;
    uint8_t* end;
    uint8_t charbuf;
    
    start = dst;
    end = dst + max;
    
    do {
        pollcode = poll(pubfd, 1, 10);
        if (pollcode <= 0) {
            goto pipe_gettext_SCRAP;
        }
        else if (pubfd[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            goto pipe_gettext_SCRAP;
        }
    
        read(pubfd->fd, &charbuf, 1);
        *dst++ = charbuf;
        
        if (charbuf == 0) {
            break;
        }
        if (dst == end) {
            *dst = 0;
            break;
        }
    } while (1);
    
    
    return (int)(dst - start);
    
    pipe_gettext_SCRAP:
    return pollcode;
}



int ppipelist_getbinary(uint8_t* dst, size_t* size, size_t max);



int ppipelist_gethex(uint8_t* dst, size_t* size, size_t max);
int ppipelist_gettext(char* dst, size_t* size, size_t max);








