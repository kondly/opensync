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

#include <stdlib.h>
#include <string.h>

#include "ovsdb_table.h"
#include "schema.h"
#include "log.h"
#include "osp_unit.h"

#include "timevt_server.h"

#include "memutil.h"
#include "qm.h"

// Max number of events collected for single report
#ifndef TESRV_MAX_REPORT_EVENTS
#define TESRV_MAX_REPORT_EVENTS 512
#endif

static ovsdb_table_t table_AWLAN_Node;
static ovsdb_table_t table_TELOG_Config;

static struct s_mqtt_telogger
{
    te_server_handle sh; // server handle
    char *nodeId;
    char *locationId;
    char *topic; // mqtt topic
    int qos; // mqtt qos or -1 when undefined
    int interval; // aggr interval

    struct ev_loop *loop;
    char swver[256];

} g_tesrv;

static char *alloc_string(char *oldstr, const char *newstr)
{
    char *str = NULL;

    if (newstr != NULL) {
        int str_size = strlen(newstr) + 1;
        str = (char *)REALLOC(oldstr, str_size);
        strscpy(str, newstr, str_size);
    } else {
        FREE(oldstr);
        str = NULL;
    }
    return str;
}

static void update_identity(const struct schema_AWLAN_Node *new)
{
    if (new != NULL)
    {
        const char *pval;
        pval = SCHEMA_KEY_VAL(new->mqtt_headers, "locationId");
        if (pval && *pval != '\0') g_tesrv.locationId = alloc_string(g_tesrv.locationId, pval);

        pval = SCHEMA_KEY_VAL(new->mqtt_headers, "nodeId");
        if (pval && *pval != '\0') g_tesrv.nodeId = alloc_string(g_tesrv.nodeId, pval);
    }

    tesrv_set_identity(g_tesrv.sh, g_tesrv.locationId, g_tesrv.nodeId);
}

static void callback_AWLAN_Node(
        ovsdb_update_monitor_t *mon,
        struct schema_AWLAN_Node *old,
        struct schema_AWLAN_Node *new)
{
    switch(mon->mon_type)
    {
    default:
        break;

    case OVSDB_UPDATE_NEW:
    case OVSDB_UPDATE_MODIFY:
        update_identity(new);
        break;
    }
}

static bool eh_on_new_report_prepared(
        void *subscriber,
        te_server_handle srv,
        const uint8_t *report,
        size_t length)
{
    struct s_mqtt_telogger *obj = (struct s_mqtt_telogger *)subscriber;
    // no remote config received? drop report
    if (obj->topic == NULL || *obj->topic == '\0')
    {
        LOG(INFO, "te-server: report of %zu bytes dropped due to missing mqtt topic", length);
        return false;
    }

    qm_item_t item;
    qm_response_t resp;

    memset(&item, 0, sizeof(item));
    item.buf = (void *)report;
    item.size = length;
    item.topic = obj->topic;
    item.req.compress = QM_REQ_COMPRESS_IF_CFG;
    if (obj->qos >= 0)
    {
        item.req.set_qos = 1;
        item.req.qos_val = (uint8_t)obj->qos;
    }
    bool rv = qm_mqtt_send_message(&item, &resp);
    LOG(INFO, "te-server: sending report of %zu bytes via mqtt: %s", length, rv ? "ack" : "dropped");
    return rv;
}

static bool open_server(int report_interval)
{
    g_tesrv.sh = tesrv_open(g_tesrv.loop, NULL, g_tesrv.swver, report_interval, TESRV_MAX_REPORT_EVENTS);
    if (g_tesrv.sh == NULL)
    {
        LOG(ERR, "te-server: server creation failure");
        return false;
    }
    tesrv_subscribe_new_report(g_tesrv.sh, &g_tesrv, &eh_on_new_report_prepared);
    LOG(INFO, "te-server: started");
    return true;
}

static bool set_log_severity(char *new_sev)
{
    if (new_sev == NULL || *new_sev == '\0') return false;
    return tesrv_set_log_severity(g_tesrv.sh, log_severity_fromstr(new_sev));
}

static void callback_TELOG_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_TELOG_Config *old,
        struct schema_TELOG_Config *new)
{
    switch(mon->mon_type)
    {
    case OVSDB_UPDATE_DEL:
        // stop generating mqtt reports
        FREE(g_tesrv.topic);
        g_tesrv.topic = NULL;
        break;

    case OVSDB_UPDATE_NEW:
        if (new->report_interval_exists)
        {
            tesrv_set_aggregation_period(g_tesrv.sh, new->report_interval);
        }
        g_tesrv.topic = alloc_string(g_tesrv.topic, new->mqtt_topic_exists ? new->mqtt_topic : NULL);
        g_tesrv.qos = new->mqtt_qos_exists ? new->mqtt_qos : -1;
        if (new->log_severity_exists) (void)set_log_severity(new->log_severity);
        update_identity(NULL);
        break;

    case OVSDB_UPDATE_MODIFY:
        if (new->mqtt_topic_changed)
        {
            g_tesrv.topic = alloc_string(g_tesrv.topic, new->mqtt_topic);
        }

        if (new->report_interval_changed)
        {
            tesrv_set_aggregation_period(g_tesrv.sh, new->report_interval);
        }

        if (new->log_severity_changed)
        {
            (void)set_log_severity(new->log_severity);
        }

        if (new->mqtt_qos_exists)
        {
            g_tesrv.qos = new->mqtt_qos;
        }
        break;

    case OVSDB_UPDATE_ERROR:
        break;
    }
}

void mqtt_telog_fini(void)
{
    if (g_tesrv.sh == NULL) return;

    tesrv_close(g_tesrv.sh);
    g_tesrv.sh = NULL;
    FREE(g_tesrv.topic);
    FREE(g_tesrv.nodeId);
    FREE(g_tesrv.locationId);
    LOG(INFO, "te-server: stopped");

    // missing d-tor for monitoring
    table_AWLAN_Node.table_callback = NULL;
    table_TELOG_Config.table_callback = NULL;
}

void mqtt_telog_init(struct ev_loop *ev)
{
    memset(&g_tesrv, 0, sizeof(g_tesrv));

    g_tesrv.qos = -1;
    (void)osp_unit_sw_version_get(g_tesrv.swver, sizeof(g_tesrv.swver));
    g_tesrv.loop = ev ? ev : EV_DEFAULT;

    /*
     * At the beginning allow collecting events for 10 minutes, which is assumed
     * the longest startup time before TELOG config will arrive from the cloud.
     * After this time just try to send the report via mqtt and forget it
     */
    if (!open_server(10*60 /*=10 minutes*/)) return;

    OVSDB_TABLE_INIT_NO_KEY(AWLAN_Node);
    OVSDB_TABLE_MONITOR_F(AWLAN_Node, ((char*[]){"mqtt_headers", NULL}));

    // Initialize OVSDB TELOG_Config table
    OVSDB_TABLE_INIT_NO_KEY(TELOG_Config);
    OVSDB_TABLE_MONITOR(TELOG_Config, false);
}
