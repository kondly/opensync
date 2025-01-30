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

#include <string.h>
#include <jansson.h>

#include "log.h"
#include "memutil.h"
#include "module.h"
#include "osp_ps.h"
#include "ovsdb_table.h"
#include "ovsdb_sync.h"
#include "json_util.h"
#include "schema.h"
#include "schema_consts.h"
#include "ff_lib.h"

#define OVSDB_TS_KEY   "topology_snapshot"
#define PM_MODULE_NAME "topology_snapshot"
#define LOG_PREFIX     "[PM:TS] "

#define PS_LAST_PARENT_STORE "pm_last_parent_store"
#define PS_LAST_PARENT_KEY   "last_parent"

#define LAST_PARENT_PRIORITY 10

static ev_timer timeout_network_reenable;
static ovsdb_table_t table_Node_Config;
static bool multi_ap_enabled;

static void pm_ts_erase_last_parent_store(void)
{
    LOGI(LOG_PREFIX "Erasing last parent store: %s", PS_LAST_PARENT_STORE);
    osp_ps_erase_store_name(PS_LAST_PARENT_STORE, 0);
}

static void pm_ts_store_last_parent_credentials(void)
{
    json_t *json_res;
    char *config_str = NULL;
    osp_ps_t *ps = NULL;
    size_t index;
    json_t *row;
    size_t str_size;
    ssize_t set_result;
    char *filter_columns[] = {
        "+",
        SCHEMA_COLUMN(Wifi_VIF_Config, if_name),
        SCHEMA_COLUMN(Wifi_VIF_Config, parent),
        SCHEMA_COLUMN(Wifi_VIF_Config, ssid),
        SCHEMA_COLUMN(Wifi_VIF_Config, wpa_key_mgmt),
        SCHEMA_COLUMN(Wifi_VIF_Config, wpa_psks),
        SCHEMA_COLUMN(Wifi_VIF_Config, multi_ap),
        NULL};

    json_res = ovsdb_sync_select_where2(
            SCHEMA_TABLE(Wifi_VIF_Config),
            ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_Config, mode), SCHEMA_CONSTS_VIF_MODE_STA));
    if (json_res == NULL)
    {
        LOGE(LOG_PREFIX "Error select operation ovsdb");
        goto exit;
    }
    json_array_foreach(json_res, index, row) ovsdb_table_filter_row(row, filter_columns);

    ps = osp_ps_open(PS_LAST_PARENT_STORE, OSP_PS_RDWR | OSP_PS_ENCRYPTION);
    if (ps == NULL)
    {
        LOGE(LOG_PREFIX "Error opening %s persistent store.", PS_LAST_PARENT_STORE);
        goto exit;
    }

    config_str = json_dumps(json_res, JSON_COMPACT);
    if (config_str == NULL)
    {
        LOGE(LOG_PREFIX "Error converting %s JSON to string.", PS_LAST_PARENT_KEY);
        goto exit;
    }

    str_size = strlen(config_str) + 1;
    set_result = osp_ps_set(ps, PS_LAST_PARENT_KEY, config_str, str_size);

    const bool is_error = (set_result < 0);
    const bool is_deleted = (set_result == 0);
    const bool is_write_too_small = (is_error == false) && (is_deleted == false) && ((size_t)set_result < str_size);

    if (is_error || is_write_too_small)
    {
        LOGE(LOG_PREFIX "Error storing %s to persistent storage.", PS_LAST_PARENT_KEY);
        goto exit;
    }

exit:
    if (json_res != NULL) json_decref(json_res);
    if (config_str != NULL) json_free(config_str);
    if (ps != NULL) osp_ps_close(ps);
}

static void callback_Node_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_Node_Config *old_rec,
        struct schema_Node_Config *conf)
{
    switch (mon->mon_type)
    {
        case OVSDB_UPDATE_ERROR:
            return;
        case OVSDB_UPDATE_NEW:
        case OVSDB_UPDATE_MODIFY:
            if (strcmp(OVSDB_TS_KEY, conf->key) == 0) pm_ts_store_last_parent_credentials();
            break;
        case OVSDB_UPDATE_DEL:
            if (strcmp(OVSDB_TS_KEY, conf->key) == 0) pm_ts_erase_last_parent_store();
            break;
    }
}

static json_t *pm_ts_sta_network_ps_load(void)
{
    osp_ps_t *ps = NULL;
    char *config_str = NULL;
    json_t *config_json = NULL;
    ssize_t str_size;

    ps = osp_ps_open(PS_LAST_PARENT_STORE, OSP_PS_RDWR | OSP_PS_ENCRYPTION);
    if (ps == NULL)
    {
        LOGE(LOG_PREFIX "Error opening %s persistent store.", PS_LAST_PARENT_STORE);
        goto exit;
    }

    str_size = osp_ps_get(ps, PS_LAST_PARENT_KEY, NULL, 0);
    if (str_size < 0)
    {
        LOGE(LOG_PREFIX " Error fetching %s key size.", PS_LAST_PARENT_KEY);
        goto exit;
    }
    else if (str_size == 0)
    {
        LOGD(LOG_PREFIX " Read 0 bytes for %s from persistent storage. The record does not exist yet.",
             PS_LAST_PARENT_KEY);
        goto exit;
    }

    config_str = MALLOC((size_t)str_size);
    if (osp_ps_get(ps, PS_LAST_PARENT_KEY, config_str, (size_t)str_size) != str_size)
    {
        LOGE(LOG_PREFIX "Error retrieving persistent %s key.", PS_LAST_PARENT_KEY);
        goto exit;
    }
    config_json = json_loads(config_str, 0, NULL);
    if (config_json == NULL)
    {
        LOGE(LOG_PREFIX "Error parsing JSON: %s", config_str);
        goto exit;
    }

exit:
    if (config_str != NULL) FREE(config_str);
    if (ps != NULL) osp_ps_close(ps);

    return config_json;
}

static bool pm_ts_enable_sta_networks_where(bool enable, json_t *where)
{
    int rc;
    json_t *row;

    row = json_pack("{ s : b }", SCHEMA_COLUMN(Wifi_Credential_Config, enabled), enable);
    if (row == NULL)
    {
        LOGE(LOG_PREFIX "%s: Error packing json", __func__);
        return false;
    }

    rc = ovsdb_sync_update_where("Wifi_Credential_Config", where, row);
    if (rc == -1)
    {
        LOGE(LOG_PREFIX "%s: Error updating Wifi_Credential_Config", __func__);
        return false;
    }

    return true;
}
static bool pm_ts_enable_sta_networks_type(bool enable, char *type)
{
    return pm_ts_enable_sta_networks_where(
            enable,
            ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Credential_Config, onboard_type), type));
}

static bool pm_ts_enable_all_sta_networks(bool enable)
{
    return pm_ts_enable_sta_networks_where(enable, NULL);
}

/* This function checks if network from Wifi_Credential_Config
 * is currently used and works but controller didn't filled yet
 * Wifi_VIF_Config */
static bool pm_ts_check_stored_network_used(void)
{
    size_t index;
    json_t *row;
    bool rv = false;

    /* Manager::is_connected == true */
    json_t *is_connected = ovsdb_sync_select_where(
            SCHEMA_TABLE(Manager),
            ovsdb_where_simple_typed(SCHEMA_COLUMN(Manager, is_connected), "true", OCLM_BOOL));
    if (is_connected == NULL) return false; /* node is not connected */
    json_decref(is_connected);

    /* Wifi_VIF_Config::mode == sta */
    json_t *vifs = ovsdb_sync_select_where(
            SCHEMA_TABLE(Wifi_VIF_Config),
            ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_Config, mode), SCHEMA_CONSTS_VIF_MODE_STA));

    if (vifs == NULL) return false; /* no sta interface discovered */

    json_array_foreach(vifs, index, row)
    {
        const char *ssid = json_string_value(json_object_get(row, SCHEMA_COLUMN(Wifi_VIF_Config, ssid)));
        /* If ssid is set in Wifi_VIF_Config then we can assume
         * that whole network config is set via Wifi_VIF_Config
         * In that case Wifi_Credential_Config networks
         * for that interface are not used anymore */
        if (strlen(ssid) > 0) continue;

        const char *if_name = json_string_value(json_object_get(row, SCHEMA_COLUMN(Wifi_VIF_Config, if_name)));
        json_t *where = ovsdb_where_multi(
                ovsdb_where_simple(SCHEMA_COLUMN(Connection_Manager_Uplink, if_name), if_name),
                ovsdb_where_simple_typed(SCHEMA_COLUMN(Connection_Manager_Uplink, is_used), "true", OCLM_BOOL),
                NULL);

        /* Connection_Manager_Uplink::is_used == true for if_name */
        json_t *uplink = ovsdb_sync_select_where(SCHEMA_TABLE(Connection_Manager_Uplink), where);

        if (uplink != NULL)
        {
            rv = true; /* found interface that has no VIF_Config and is used */
            json_decref(uplink);
            break;
        }
    }
    json_decref(vifs);
    return rv;
}

static void pm_ts_sta_network_reenable_timeout_cb(EV_P_ ev_timer *arg, int events)
{
    LOGN(LOG_PREFIX "Timeout reached! Reenabling all networks");
    pm_ts_enable_all_sta_networks(true);

    /* FIXME: Check below assumes that default onboard network are GRE only
     * when default onboard type will contain multi_ap networks
     * refactor of this logic will be required */
    if (multi_ap_enabled && !pm_ts_check_stored_network_used())
    {
        LOGI(LOG_PREFIX "multi-ap network WA was applied, reverting WA. Disabling all multi-ap type networks");
        pm_ts_enable_sta_networks_type(false, SCHEMA_CONSTS_ONBOARD_TYPE_MULTI_AP);
    }
}

static bool pm_ts_cfg_is_multi_ap(json_t *row)
{
    const char *multi_ap;
    multi_ap = json_string_value(json_object_get(row, SCHEMA_COLUMN(Wifi_VIF_Config, multi_ap)));
    return (multi_ap != NULL && strcmp(multi_ap, SCHEMA_CONSTS_MULTI_AP_BACKHAUL_STA) == 0);
}

static void pm_ts_start_timer(void)
{
    const float reenable_network_timeout = CONFIG_PM_TS_RESTORE_TIMEOUT;
    /* start timer to reenable all networks after timeout */
    ev_timer_init(&timeout_network_reenable, pm_ts_sta_network_reenable_timeout_cb, reenable_network_timeout, 0);
    ev_timer_start(EV_DEFAULT_ & timeout_network_reenable);
}

static void pm_ts_append_sta_network(json_t *config)
{
    size_t index;
    ovs_uuid_t uuid;
    json_t *row;
    struct schema_Wifi_Credential_Config cred;
    pjs_errmsg_t err;

    json_array_foreach(config, index, row)
    {
        memset(&cred, 0, sizeof(struct schema_Wifi_Credential_Config));
        SCHEMA_SET_BOOL(cred.enabled, true);
        SCHEMA_SET_STR(cred.ssid, json_string_value(json_object_get(row, SCHEMA_COLUMN(Wifi_VIF_Config, ssid))));
        SCHEMA_SET_STR(cred.bssid, json_string_value(json_object_get(row, SCHEMA_COLUMN(Wifi_VIF_Config, parent))));
        SCHEMA_SET_INT(cred.priority, LAST_PARENT_PRIORITY);
        SCHEMA_SET_STR(
                cred.onboard_type,
                pm_ts_cfg_is_multi_ap(row) ? SCHEMA_CONSTS_ONBOARD_TYPE_MULTI_AP : SCHEMA_CONSTS_ONBOARD_TYPE_GRE);

        if (pm_ts_cfg_is_multi_ap(row)) multi_ap_enabled = 1;

        /* FIXME: This assumes only WPA-PSK. This is fine for
         * now because ow_ovsdb_cconf selects any encyption
         */
        SCHEMA_KEY_VAL_APPEND(cred.security, SCHEMA_CONSTS_SECURITY_ENCRYPT, SCHEMA_CONSTS_SECURITY_ENCRYPT_WPA);

        /* We can provide only one password for sta network
         * Example entry "["map",[["key--1","psk_password"]]]"
         * to get psk from that structure we need to properly unwrap
         * Explained unwraping step-by-step:
         * r = json_object_get(row, "wpa_psks") => "["map",[["key--1","psk_password"]]]"
         * r = json_array_get(r, 1) => "[["key--1","psk_password"]]"
         * r = json_array_get(r, 0) => "["key--1","psk_password"]"
         * r = json_array_get(r, 1) => "psk_password"  */
        const char *psk = json_string_value(json_array_get(
                json_array_get(json_array_get(json_object_get(row, SCHEMA_COLUMN(Wifi_VIF_Config, wpa_psks)), 1), 0),
                1));
        SCHEMA_KEY_VAL_APPEND(cred.security, SCHEMA_CONSTS_SECURITY_KEY, psk);

        json_t *where = ovsdb_where_simple(
                SCHEMA_COLUMN(Wifi_VIF_Config, if_name),
                json_string_value(json_object_get(row, SCHEMA_COLUMN(Wifi_VIF_Config, if_name))));

        if (where == NULL)
        {
            LOGW(LOG_PREFIX " Error creating ovsdb where simple");
            continue;
        }

        bool rc = ovsdb_sync_insert_with_parent(
                SCHEMA_TABLE(Wifi_Credential_Config),
                schema_Wifi_Credential_Config_to_json(&cred, err),
                &uuid,
                SCHEMA_TABLE(Wifi_VIF_Config),
                where,
                SCHEMA_COLUMN(Wifi_VIF_Config, credential_configs));
        if (rc == false) LOGW(LOG_PREFIX " Error inserting entry in Wifi_Credential_Config ");
    }

    /* Errata: devices may not supported network list mixed types
     * if multi_ap is available we disable gre based networks
     * Most of platforms configure interface upfront,
     * because we cannot add bridge interface to wpa_supplicant at
     * network block configuration level
     * and some platforms configure interface type upfront */
    if (multi_ap_enabled)
    {
        LOGI(LOG_PREFIX "Discovered multi-ap network, applying WA. Disabling all gre type networks");
        pm_ts_enable_sta_networks_type(false, SCHEMA_CONSTS_ONBOARD_TYPE_GRE);
        pm_ts_start_timer();
    }
}

static void pm_ts_after_start_cleanup(void)
{
    json_t *rows = ovsdb_sync_select(SCHEMA_TABLE(Node_Config), SCHEMA_COLUMN(Node_Config, module), PM_MODULE_NAME);
    if (rows == NULL)
    {
        LOGI(LOG_PREFIX "Node_Config entry does not exist, clearing persistent storage");
        pm_ts_erase_last_parent_store();
        return;
    }
    json_decref(rows);
}

static void mod_pm_ts_init(void *data)
{
    (void)data;
    json_t *config = NULL;

    LOGI(LOG_PREFIX "Starting module");
    OVSDB_TABLE_INIT_NO_KEY(Node_Config);
    OVSDB_TABLE_MONITOR(Node_Config, false);

    /* check if presistent storage is available */
    config = pm_ts_sta_network_ps_load();
    if (config != NULL)
    {
        LOGI(LOG_PREFIX "Found last parent configuration, restoring data");

        if (ff_is_flag_enabled("use_only_preferred_network"))
        {
            LOGN(LOG_PREFIX "Feature flag use_only_preferred_network is set. Disabling all default networks");
            pm_ts_enable_all_sta_networks(false);
            pm_ts_start_timer();
        }
        /* Append stored network to list */
        pm_ts_append_sta_network(config);

        json_decref(config);
    }
    pm_ts_after_start_cleanup();
}

static void mod_pm_ts_fini(void *data)
{
    (void)data;
}

MODULE(ps_topology_snapshot, mod_pm_ts_init, mod_pm_ts_fini)
