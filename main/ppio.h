//
//  ppio.h
//  otdb
//
//  Created by SolPad on 27/4/18.
//  Copyright © 2018 JP Norair. All rights reserved.
//

#ifndef ppio_h
#define ppio_h

#include <stdio.h>

typedef struct {
    pthread_t   thread;
    void*       args;
} ppio_listen_t;


#endif /* ppio_h */
