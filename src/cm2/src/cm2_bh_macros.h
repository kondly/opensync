/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CM2_BH_MACROS_H_INCLUDED
#define CM2_BH_MACROS_H_INCLUDED

#include <ovsdb_sync.h>
#include <ovsdb_table.h>

static inline int strcmp_null(const char *a, const char *b)
{
    if (a == NULL && b != NULL) return -1;
    if (a != NULL && b == NULL) return 1;
    if (a == NULL && b == NULL) return 0;
    return strcmp(a, b);
}

#define BOOL_CSTR(x) ((!!(x)) ? "yes" : "no")

#define CM2_OVS_COL(mon, old, new)                                         \
    (mon)->mon_type == OVSDB_UPDATE_NEW      ? (new##_exists ? new : NULL) \
    : (mon)->mon_type == OVSDB_UPDATE_MODIFY ? (new##_exists ? new : NULL) \
    : (mon)->mon_type == OVSDB_UPDATE_DEL    ? (old##_exists ? old : NULL) \
                                             : NULL

#define CM2_BH_GRE_PREFIX "g-"

#define CM2_BH_OVS_INIT(PRIV, CALLBACK, TABLE, KEY_COLUMN, KEY_VALUE)              \
    do                                                                             \
    {                                                                              \
        const char *column = SCHEMA_COLUMN(TABLE, KEY_COLUMN);                     \
        const char *value = KEY_VALUE;                                             \
        json_t *where = ovsdb_where_simple(column, value);                         \
        struct schema_##TABLE row;                                                 \
        MEMZERO(row);                                                              \
        const bool ok = ovsdb_table_select_one_where(&table_##TABLE, where, &row); \
        if (ok == false) break;                                                    \
        ovsdb_update_monitor_t mon = {                                             \
            .mon_type = OVSDB_UPDATE_NEW,                                          \
        };                                                                         \
        CALLBACK(PRIV, &mon, NULL, &row);                                          \
    } while (0)

#endif /* CM2_BH_MACROS_H_INCLUDED */
