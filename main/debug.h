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



#define _E_NRM  "\033[0m"
#define _E_RED  "\033[31m"
#define _E_GRN  "\033[32m"
#define _E_YEL  "\033[33m"
#define _E_BLU  "\033[34m"
#define _E_MAG  "\033[35m"
#define _E_CYN  "\033[36m"
#define _E_WHT  "\033[37m"


#if defined(__DEBUG__)
#   define DEBUG_PRINTF(...)    do { if (cliopt_isdebug()) fprintf(stderr, _E_YEL "DEBUG: " _E_NRM __VA_ARGS__); } while(0)
#   define HEX_DUMP(LABEL, HEX, ...) do { if (cliopt_isdebug()) _HEX_(HEX, SIZE, ...) } while(0)

#else
#   define DEBUG_PRINTF(...)    do { } while(0)
#   define HEX_DUMP(LABEL, HEX, ...) do { } while(0)

#endif

#define VERBOSE_PRINTF(...)     do { if (cliopt_isverbose()) fprintf(stderr, _E_CYN "MSG: " _E_NRM __VA_ARGS__); } while(0)
#define VDATA_PRINTF(...)       do { if (cliopt_isverbose()) fprintf(stderr, _E_GRN "DATA: " _E_NRM __VA_ARGS__); } while(0)





///@todo finish error codes and make them into an HB lib

/// LIBCODE: up to 128
#define LIBCODE(VAL)                    LIBCODE_##VAL
#define LIBCODE_argtable                1
#define LIBCODE_bintex                  2
#define LIBCODE_cJSON                   3
#define LIBCODE_cmdtab                  4
#define LIBCODE_hbuilder                5
#define LIBCODE_judy                    6
#define LIBCODE_otfs                    7
#define LIBCODE_OTEAX                   8
#define LIBCODE_smut                    9

/// FNCODE: up to 256 per lib
#define FNCODE(VAL)                 FNCODE_##VAL

#define FNCODE_arg_nullcheck        1
#define FNCODE_arg_parse            2

#define FNCODE_bintex_fs            1
#define FNCODE_bintex_ss            2
#define FNCODE_bintex_iter_fq       3
#define FNCODE_bintex_iter_sq       4


#define FNCODE_cJSON_Parse          1
#define FNCODE_cJSON_ParseWithOpts  2
#define FNCODE_cJSON_Print          3
#define FNCODE_cJSON_PrintUnformatted   4
#define FNCODE_cJSON_PrintBuffered      5
#define FNCODE_cJSON_PrintPreallocated  6
#define FNCODE_cJSON_Delete         7
#define FNCODE_cJSON_GetArraySize   8
#define FNCODE_cJSON_GetArrayItem   9
#define FNCODE_cJSON_GetObjectItem  10
#define FNCODE_cJSON_GetObjectItemCaseSensitive 11
#define FNCODE_cJSON_HasObjectItem  12
#define FNCODE_cJSON_GetErrorPtr    13
#define FNCODE_cJSON_GetStringValue 14
#define FNCODE_cJSON_IsInvalid      15
#define FNCODE_cJSON_IsFalse        16
#define FNCODE_cJSON_IsTrue         17
#define FNCODE_cJSON_IsBool         18
#define FNCODE_cJSON_IsNull         19
#define FNCODE_cJSON_IsNumber       20
#define FNCODE_cJSON_IsString       21
#define FNCODE_cJSON_IsArray        22
#define FNCODE_cJSON_IsObject       23
#define FNCODE_cJSON_IsRaw          24
#define FNCODE_cJSON_CreateNull     25
#define FNCODE_cJSON_CreateTrue     26
#define FNCODE_cJSON_CreateFalse    27
#define FNCODE_cJSON_CreateBool     28
#define FNCODE_cJSON_CreateNumber   29
#define FNCODE_cJSON_CreateString   30
#define FNCODE_cJSON_CreateRaw      31
#define FNCODE_cJSON_CreateArray    32
#define FNCODE_cJSON_CreateObject   33
#define FNCODE_cJSON_CreateStringReference  34
#define FNCODE_cJSON_CreateObjectReference  35
#define FNCODE_cJSON_CreateArrayReference   36
#define FNCODE_cJSON_CreateIntArray     37
#define FNCODE_cJSON_CreateFloatArray   38
#define FNCODE_cJSON_CreateDoubleArray  39
#define FNCODE_cJSON_CreateStringArray  40
#define FNCODE_cJSON_AddItemToArray     41
#define FNCODE_cJSON_AddItemToObject    42
#define FNCODE_cJSON_AddItemToObjectCS  43
#define FNCODE_cJSON_AddItemReferenceToArray    44
#define FNCODE_cJSON_AddItemReferenceToObject   45
#define FNCODE_cJSON_DetachItemViaPointer       46
#define FNCODE_cJSON_DetachItemFromArray        47
#define FNCODE_cJSON_DeleteItemFromArray        48
#define FNCODE_cJSON_DetachItemFromObject       49
#define FNCODE_cJSON_DetachItemFromObjectCaseSensitive  50
#define FNCODE_cJSON_DeleteItemFromObject       51
#define FNCODE_cJSON_DeleteItemFromObjectCaseSensitive  52
#define FNCODE_cJSON_InsertItemInArray          53
#define FNCODE_cJSON_ReplaceItemViaPointer      54
#define FNCODE_cJSON_ReplaceItemInArray         55
#define FNCODE_cJSON_ReplaceItemInObject        56
#define FNCODE_cJSON_ReplaceItemInObjectCaseSensitive   57
#define FNCODE_cJSON_Duplicate          58
#define FNCODE_cJSON_Compare            59
#define FNCODE_cJSON_Minify             60
#define FNCODE_cJSON_AddNullToObject    61
#define FNCODE_cJSON_AddTrueToObject    62
#define FNCODE_cJSON_AddFalseToObject   63
#define FNCODE_cJSON_AddBoolToObject    64
#define FNCODE_cJSON_AddNumberToObject  65
#define FNCODE_cJSON_AddStringToObject  66
#define FNCODE_cJSON_AddRawToObject     67
#define FNCODE_cJSON_AddObjectToObject  68
#define FNCODE_cJSON_AddArrayToObject   69
#define FNCODE_cJSON_SetNumberHelper    70
#define FNCODE_cJSON_malloc             71
#define FNCODE_cJSON_free               72

#define FNCODE_cmdtab_init          1
#define FNCODE_cmdtab_add           2
#define FNCODE_cmdtab_free          3
#define FNCODE_cmdtab_search        4
#define FNCODE_cmdtab_subsearch     5
#define FNCODE_cmdtab_list          6

#define FNCODE_hbuilder_init        1
#define FNCODE_hbuilder_free        2
#define FNCODE_hbuilder_runcmd      3

#define FNCODE_judy_open            1
#define FNCODE_judy_close           2
#define FNCODE_judy_clone           3
#define FNCODE_judy_data            4
#define FNCODE_judy_cell            5
#define FNCODE_judy_strt            6
#define FNCODE_judy_slot            7
#define FNCODE_judy_key             8
#define FNCODE_judy_end             9
#define FNCODE_judy_nxt             10
#define FNCODE_judy_prv             11
#define FNCODE_judy_del             12

#define FNCODE_otfs_init            1
#define FNCODE_otfs_deinit          2
#define FNCODE_otfs_load_defaults   3
#define FNCODE_otfs_new             4
#define FNCODE_otfs_del             5
#define FNCODE_otfs_setfs           6
#define FNCODE_otfs_iterator_start  7
#define FNCODE_otfs_iterator_next   8

#define FNCODE_otdb_init            1
#define FNCODE_otdb_deinit          2
#define FNCODE_otdb_disconnect      3
#define FNCODE_otdb_newdevice       4
#define FNCODE_otdb_deldevice       5
#define FNCODE_otdb_setdevice       6
#define FNCODE_otdb_opendb          7
#define FNCODE_otdb_savedb          8
#define FNCODE_otdb_delfile         9
#define FNCODE_otdb_newfile         10
#define FNCODE_otdb_read            11
#define FNCODE_otdb_readall         12
#define FNCODE_otdb_restore         13
#define FNCODE_otdb_readhdr         14
#define FNCODE_otdb_readperms       15
#define FNCODE_otdb_writedata       16
#define FNCODE_otdb_writeperms      17

#define FNCODE_eax_init_and_key     18
#define FNCODE_eax_end              19
#define FNCODE_eax_encrypt_message  20
#define FNCODE_eax_decrypt_message  21
#define FNCODE_eax_init_message     22
#define FNCODE_eax_encrypt          23
#define FNCODE_eax_decrypt          24
#define FNCODE_eax_compute_tag      25
#define FNCODE_eax_auth_data        26
#define FNCODE_eax_crypt_data       27

#define FNCODE_smut_init            1
#define FNCODE_smut_free            2
#define FNCODE_smut_resp_proc       3
#define FNCODE_smut_req_proc        4
#define FNCODE_smut_extract_payload 5





#define MULTCODE(VAL)               MULTCODE_##VAL

#define MULTCODE_arg_nullcheck        1
#define MULTCODE_arg_parse            1

#define MULTCODE_bintex_fs            -1
#define MULTCODE_bintex_ss            -1
#define MULTCODE_bintex_iter_fq       -1
#define MULTCODE_bintex_iter_sq       -1


#define MULTCODE_cJSON_Parse          1
#define MULTCODE_cJSON_ParseWithOpts  1
#define MULTCODE_cJSON_Print          1
#define MULTCODE_cJSON_PrintUnformatted   1
#define MULTCODE_cJSON_PrintBuffered      1
#define MULTCODE_cJSON_PrintPreallocated  1
#define MULTCODE_cJSON_Delete         1
#define MULTCODE_cJSON_GetArraySize   1
#define MULTCODE_cJSON_GetArrayItem   1
#define MULTCODE_cJSON_GetObjectItem  1
#define MULTCODE_cJSON_GetObjectItemCaseSensitive 1
#define MULTCODE_cJSON_HasObjectItem  1
#define MULTCODE_cJSON_GetErrorPtr    1
#define MULTCODE_cJSON_GetStringValue 1
#define MULTCODE_cJSON_IsInvalid      1
#define MULTCODE_cJSON_IsFalse        1
#define MULTCODE_cJSON_IsTrue         1
#define MULTCODE_cJSON_IsBool         1
#define MULTCODE_cJSON_IsNull         1
#define MULTCODE_cJSON_IsNumber       1
#define MULTCODE_cJSON_IsString       1
#define MULTCODE_cJSON_IsArray        1
#define MULTCODE_cJSON_IsObject       1
#define MULTCODE_cJSON_IsRaw          1
#define MULTCODE_cJSON_CreateNull     1
#define MULTCODE_cJSON_CreateTrue     1
#define MULTCODE_cJSON_CreateFalse    1
#define MULTCODE_cJSON_CreateBool     1
#define MULTCODE_cJSON_CreateNumber   1
#define MULTCODE_cJSON_CreateString   1
#define MULTCODE_cJSON_CreateRaw      1
#define MULTCODE_cJSON_CreateArray    1
#define MULTCODE_cJSON_CreateObject   1
#define MULTCODE_cJSON_CreateStringReference  1
#define MULTCODE_cJSON_CreateObjectReference  1
#define MULTCODE_cJSON_CreateArrayReference   1
#define MULTCODE_cJSON_CreateIntArray     1
#define MULTCODE_cJSON_CreateFloatArray   1
#define MULTCODE_cJSON_CreateDoubleArray  1
#define MULTCODE_cJSON_CreateStringArray  1
#define MULTCODE_cJSON_AddItemToArray     1
#define MULTCODE_cJSON_AddItemToObject    1
#define MULTCODE_cJSON_AddItemToObjectCS  1
#define MULTCODE_cJSON_AddItemReferenceToArray    1
#define MULTCODE_cJSON_AddItemReferenceToObject   1
#define MULTCODE_cJSON_DetachItemViaPointer       1
#define MULTCODE_cJSON_DetachItemFromArray        1
#define MULTCODE_cJSON_DeleteItemFromArray        1
#define MULTCODE_cJSON_DetachItemFromObject       1
#define MULTCODE_cJSON_DetachItemFromObjectCaseSensitive  1
#define MULTCODE_cJSON_DeleteItemFromObject       1
#define MULTCODE_cJSON_DeleteItemFromObjectCaseSensitive  1
#define MULTCODE_cJSON_InsertItemInArray          1
#define MULTCODE_cJSON_ReplaceItemViaPointer      1
#define MULTCODE_cJSON_ReplaceItemInArray         1
#define MULTCODE_cJSON_ReplaceItemInObject        1
#define MULTCODE_cJSON_ReplaceItemInObjectCaseSensitive   1
#define MULTCODE_cJSON_Duplicate          1
#define MULTCODE_cJSON_Compare            1
#define MULTCODE_cJSON_Minify             1
#define MULTCODE_cJSON_AddNullToObject    1
#define MULTCODE_cJSON_AddTrueToObject    1
#define MULTCODE_cJSON_AddFalseToObject   1
#define MULTCODE_cJSON_AddBoolToObject    1
#define MULTCODE_cJSON_AddNumberToObject  1
#define MULTCODE_cJSON_AddStringToObject  1
#define MULTCODE_cJSON_AddRawToObject     1
#define MULTCODE_cJSON_AddObjectToObject  1
#define MULTCODE_cJSON_AddArrayToObject   1
#define MULTCODE_cJSON_SetNumberHelper    1
#define MULTCODE_cJSON_malloc             1
#define MULTCODE_cJSON_free               1

#define MULTCODE_cmdtab_init          -1
#define MULTCODE_cmdtab_add           -1
#define MULTCODE_cmdtab_free          -1
#define MULTCODE_cmdtab_search        -1
#define MULTCODE_cmdtab_subsearch     -1
#define MULTCODE_cmdtab_list          -1

#define MULTCODE_hbuilder_init        -1
#define MULTCODE_hbuilder_free        -1
#define MULTCODE_hbuilder_runcmd      -1

#define MULTCODE_judy_open            1
#define MULTCODE_judy_close           1
#define MULTCODE_judy_clone           1
#define MULTCODE_judy_data            1
#define MULTCODE_judy_cell            1
#define MULTCODE_judy_strt            1
#define MULTCODE_judy_slot            1
#define MULTCODE_judy_key             1
#define MULTCODE_judy_end             1
#define MULTCODE_judy_nxt             1
#define MULTCODE_judy_prv             1
#define MULTCODE_judy_del             1

#define MULTCODE_otfs_init            -1
#define MULTCODE_otfs_deinit          -1
#define MULTCODE_otfs_load_defaults   -1
#define MULTCODE_otfs_new             -1
#define MULTCODE_otfs_del             -1
#define MULTCODE_otfs_setfs           -1
#define MULTCODE_otfs_iterator_start  -1
#define MULTCODE_otfs_iterator_next   -1

#define MULTCODE_otdb_init            -1
#define MULTCODE_otdb_deinit          -1
#define MULTCODE_otdb_disconnect      -1
#define MULTCODE_otdb_newdevice       -1
#define MULTCODE_otdb_deldevice       -1
#define MULTCODE_otdb_setdevice       -1
#define MULTCODE_otdb_opendb          -1
#define MULTCODE_otdb_savedb          -1
#define MULTCODE_otdb_delfile         -1
#define MULTCODE_otdb_newfile         -1
#define MULTCODE_otdb_read            -1
#define MULTCODE_otdb_readall         -1
#define MULTCODE_otdb_restore         -1
#define MULTCODE_otdb_readhdr         -1
#define MULTCODE_otdb_readperms       -1
#define MULTCODE_otdb_writedata       -1
#define MULTCODE_otdb_writeperms      -1

#define MULTCODE_eax_init_and_key     -1
#define MULTCODE_eax_end              -1
#define MULTCODE_eax_encrypt_message  -1
#define MULTCODE_eax_decrypt_message  -1
#define MULTCODE_eax_init_message     -1
#define MULTCODE_eax_encrypt          -1
#define MULTCODE_eax_decrypt          -1
#define MULTCODE_eax_compute_tag      -1
#define MULTCODE_eax_auth_data        -1
#define MULTCODE_eax_crypt_data       -1

#define MULTCODE_smut_init            -1
#define MULTCODE_smut_free            -1
#define MULTCODE_smut_resp_proc       -1
#define MULTCODE_smut_req_proc        -1
#define MULTCODE_smut_extract_payload -1



#define ERRCODE(LIB, FN, RC) \
    ( -(int)((LIBCODE(LIB)<<24) | (FNCODE(FN)<<16)) - (MULTCODE(FN)*(RC)) )




#endif /* cliopt_h */
