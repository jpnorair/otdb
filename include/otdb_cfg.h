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

#ifndef otdb_cfg_h
#define otdb_cfg_h

#ifndef ENABLED
#   define ENABLED  1
#endif
#ifndef DISABLED
#   define DISABLED 0
#endif

/// Default feature configurations
#define OTDB_FEATURE(VAL)           OTDB_FEATURE_##VAL
#ifndef OTDB_FEATURE_HBUILDER
#   define OTDB_FEATURE_HBUILDER    defined(__HBUILDER__)
#endif
#ifndef OTDB_FEATURE_CLIENT
#   define OTDB_FEATURE_CLIENT      DISABLED
#endif
#ifndef OTDB_FEATURE_DEBUG
#   if defined(__DEBUG__) || defined(DEBUG) || defined (_DEBUG)
#       define OTDB_FEATURE_DEBUG   ENABLED
#   else
#       define OTDB_FEATURE_DEBUG   DISABLED
#   endif
#endif

/// Parameter configurations
///@todo redefine 
#define OTDB_PARAM(VAL)             OTDB_PARAM_##VAL
#ifndef OTDB_PARAM_NAME
#   define OTDB_PARAM_NAME          "otdb"
#endif
#ifndef OTDB_PARAM_VERSION 
#   define OTDB_PARAM_VERSION       "0.1.0"
#endif
#ifndef OTDB_PARAM_DATE
#   define OTDB_PARAM_DATE          __DATE__
#endif


/// Automatic Checks


///@todo configuration for these constants
#define OTDB_PARAM_SCRATCHDIR       "~tmp"

#endif
