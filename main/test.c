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

#include "test.h"

#include <stdio.h>
#include <stdint.h>

void test_dumpbytes(const uint8_t* data, size_t cols, size_t datalen, const char* label) {
    
    fprintf(stderr, "%s\n", label);
    fprintf(stderr, "data-length = %zu\n", datalen);
    
    for (int16_t i=0; i<datalen; ) {
        if ((i % cols) == 0) {
            fprintf(stderr, "%04d: ", i);
        }
        
        fprintf(stderr, "%02X ", data[i]);
        
        i++;
        if ((i % cols) == 0) {
            fprintf(stderr, "\n");
        }
    }
    fprintf(stderr, "\n\n");
}


