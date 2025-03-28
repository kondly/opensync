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
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "memutil.h"
#include "log.h"
#include "json_util.h"
#include "os.h"
#include "ovsdb.h"
#include "target.h"
#include "network_metadata.h"
#include "cellm_mgr.h"
#include "hw_acc.h"

uint32_t cellm_netmask_to_cidr(char *mask)
{
    uint32_t netmask;
    uint8_t ip_addr[4];
    uint32_t cidr = 0;

    sscanf(mask, "%d.%d.%d.%d", (int *)&ip_addr[0], (int *)&ip_addr[1], (int *)&ip_addr[2], (int *)&ip_addr[3]);
    memcpy(&netmask, ip_addr, sizeof(netmask));

    while (netmask)
    {
        cidr += (netmask & 0x01);
        netmask = netmask >> 1;
    }
    return cidr;
}

void cellm_make_route(char *subnet, char *netmask, char *route)
{
    uint32_t cidr;

    memset(route, 0, C_IPV6ADDR_LEN);
    cidr = cellm_netmask_to_cidr(netmask);
    snprintf(route, C_IPV6ADDR_LEN, "%s/%d", subnet, cidr);
}

static int cellm_route_exec_cmd(char *cmd)
{
    int res;
    res = system(cmd);

    LOGD("%s: cmd=%s, res=%d, errno=%s", __func__, cmd, res, strerror(errno));
    return res;
}

/*
 * This is for Phase II when we route individual clients
 */
int cellm_create_route_table(cellm_mgr_t *mgr)
{
    char cmd[1024];
    /* ip route add 0.0.0.0/0 dev wwan0 table 76 */
    snprintf(cmd, sizeof(cmd), "ip route add 0.0.0.0/0 dev wwan0 table 76");
    return (cellm_route_exec_cmd(cmd));
}

static int client_cmp(const void *a, const void *b)
{
    const char *name_a = a;
    const char *name_b = b;
    return (strcmp(name_a, name_b));
}
void cellm_client_table_update(cellm_mgr_t *mgr, struct schema_DHCP_leased_IP *dhcp_lease)
{
    struct client_entry *new_entry;
    struct client_entry *entry;

    if (!dhcp_lease->hostname_present)
    {
        LOGI("%s: hostname is absent", __func__);
        return;
    }

    if (!dhcp_lease->inet_addr_present)
    {
        LOGI("%s: inet_addr is absent", __func__);
        return;
    }

    new_entry = CALLOC(1, sizeof(struct client_entry));
    STRSCPY(new_entry->client_name, dhcp_lease->hostname);
    STRSCPY(new_entry->client_addr, dhcp_lease->inet_addr);
    LOGI("%s: New client entry %s:%s", __func__, dhcp_lease->hostname, dhcp_lease->inet_addr);

    entry = ds_tree_find(&mgr->client_table, new_entry);
    if (entry)
    {
        LOGI("%s: Existing client entry %s:%s", __func__, entry->client_name, entry->client_addr);
        FREE(new_entry);
        return;
    }
    ds_tree_insert(&mgr->client_table, new_entry, new_entry);
}

void cellm_client_table_delete(cellm_mgr_t *mgr, struct schema_DHCP_leased_IP *dhcp_lease)
{
    struct client_entry to_del;
    struct client_entry *entry;

    if (dhcp_lease->hostname_present && dhcp_lease->inet_addr_present)
    {
        STRSCPY(to_del.client_name, dhcp_lease->hostname);
        STRSCPY(to_del.client_addr, dhcp_lease->inet_addr);
        entry = ds_tree_find(&mgr->client_table, &to_del);
        if (!entry) return;

        LOGD("%s: Delete client entry %s:%s", __func__, dhcp_lease->hostname, dhcp_lease->inet_addr);
        ds_tree_remove(&mgr->client_table, entry);
        FREE(entry);
        return;
    }
}

void cellm_create_client_table(cellm_mgr_t *mgr)
{
    ds_tree_init(&mgr->client_table, client_cmp, struct client_entry, entry_node);
}

void cellm_update_route(cellm_mgr_t *mgr, char *if_name, char *lte_subnet, char *cell_gw, char *lte_netmask)
{
    STRSCPY(mgr->cellm_route->cellm_if_name, if_name);
    STRSCPY(mgr->cellm_route->cellm_subnet, lte_subnet);
    STRSCPY(mgr->cellm_route->cellm_gw, cell_gw);
    STRSCPY(mgr->cellm_route->cellm_netmask, lte_netmask);
    mgr->cellm_route->cellm_metric = LTE_DEFAULT_METRIC;
    LOGI("%s: lte_if_name[%s], lte_subnet[%s], cell_gw[%s] lte_netmask[%s], lte_metric[%d]",
         __func__,
         if_name,
         lte_subnet,
         cell_gw,
         lte_netmask,
         mgr->cellm_route->cellm_metric);
}

void cellm_update_wan_route(cellm_mgr_t *mgr, struct schema_Wifi_Route_Config *rc)
{
    cellm_route_info_t *cellm_route = mgr->cellm_route;

    LOGI("%s: wan_if_name[%s], wan_subnet[%s], wan_gw[%s], wan_metric[%d], lte_metric[%d]",
         __func__,
         rc->if_name,
         rc->dest_addr,
         rc->gateway,
         rc->metric,
         cellm_route->cellm_metric);
    STRSCPY(cellm_route->wan_if_name, rc->if_name);
    STRSCPY(cellm_route->wan_subnet, rc->dest_addr);
    STRSCPY(cellm_route->wan_gw, rc->gateway);
    STRSCPY(cellm_route->wan_netmask, rc->dest_mask);
    cellm_route->wan_metric = rc->metric;
    if (cellm_route->wan_metric > cellm_route->cellm_metric)
    {
        cellm_set_wan_state(CELLM_WAN_STATE_DOWN);
    }
    else
    {
        cellm_set_wan_state(CELLM_WAN_STATE_UP);
    }
}

/*
 * This is for Phase II when we selectively route clients over LTE
 */
int cellm_add_lte_client_routes(cellm_mgr_t *mgr)
{
    struct client_entry *entry;
    int res = 0;
    char cmd[1024];

    LOGI("%s: failover:%d", __func__, mgr->cellm_state_info->cellm_failover_active);
    entry = ds_tree_head(&mgr->client_table);
    while (entry)
    {
        if (!is_input_shell_safe(entry->client_addr)) return -1;

        /* ip rule add from `client_addr` lookup 76 */
        snprintf(cmd, sizeof(cmd), "ip rule add from %s lookup 76", entry->client_addr);
        res = cellm_route_exec_cmd(cmd);
        entry = ds_tree_next(&mgr->client_table, entry);
    }
    return res;
}

/*
 * This will be used in Phase II when we selectively route clients over LTE
 * during failover.
 */
int cellm_restore_default_client_routes(cellm_mgr_t *mgr)
{
    struct client_entry *entry;
    int res = 0;
    char cmd[1024];

    LOGI("%s: failover:%d", __func__, mgr->cellm_state_info->cellm_failover_active);

    entry = ds_tree_head(&mgr->client_table);
    while (entry)
    {
        if (!is_input_shell_safe(entry->client_addr)) return -1;

        /* ip rule del from `client_addr` lookup table 76 */
        snprintf(cmd, sizeof(cmd), "ip rule del from %s lookup 76", entry->client_addr);
        res = cellm_route_exec_cmd(cmd);
        entry = ds_tree_next(&mgr->client_table, entry);
    }

    return res;
}

int cellm_set_route_metric(cellm_mgr_t *mgr)
{
    cellm_route_info_t *route;

    route = mgr->cellm_route;
    if (!route) return -1;

    if (route->cellm_gw[0])
    {
        /*
         * Route updates are now handled by NM via the Wifi_Route_Config table.
         */
        return cellm_ovsdb_update_wifi_route_config_metric(mgr, route->cellm_if_name, route->cellm_metric);
    }
    else
    {
        LOGI("%s: cell_gw[%s] not set", __func__, route->cellm_gw);
    }

    return 0;
}

int celln_force_lte(cellm_mgr_t *mgr)
{
    cellm_route_info_t *route;
    uint32_t new_lte_priority;
    char cmd[256];
    int res;

    route = mgr->cellm_route;
    LOGI("%s: cellm_route[%s][%s]", __func__, route->cellm_if_name, route->cellm_ip_addr);
    if (!route) return -1;

    if (route->wan_gw[0])
    {
        LOGI("%s: if_name[%s]", __func__, route->wan_if_name);

        /*
         * If we just set the WAN route metric to a value higher than the value
         * for LTE, CM will eventually detect that the WAN interface is
         * healthy and switch the metric back so the WAN interface is the
         * preferred route. To get around this, we have to bring the WAN
         * interface down in the 'force' case.
         */
        snprintf(cmd, sizeof(cmd), "ifconfig %s down", route->wan_if_name);
        res = cmd_log_check_safe(cmd);
        if (res)
        {
            LOGI("%s: cmd[%s] failed", __func__, cmd);
            return res;
        }

        route->wan_priority = cellm_ovsdb_cmu_get_wan_priority(mgr);
        route->cellm_priority = cellm_ovsdb_cmu_get_lte_priority(mgr);
        new_lte_priority = route->wan_priority + LTE_CMU_DEFAULT_PRIORITY;
        LOGI("%s: wan_priority[%d], LTE_CMU_DEFAULT_PRIORITY[%d], new_lte_priority[%d]",
             __func__,
             route->wan_priority,
             LTE_CMU_DEFAULT_PRIORITY,
             new_lte_priority);
        return cellm_ovsdb_cmu_update_lte_priority(mgr, new_lte_priority);
    }

    return 0;
}

int cellm_restore_wan(cellm_mgr_t *mgr)
{
    cellm_route_info_t *route;
    char cmd[256];
    int res;

    route = mgr->cellm_route;
    if (!route) return -1;

    snprintf(cmd, sizeof(cmd), "ifconfig %s up", route->wan_if_name);
    res = cmd_log_check_safe(cmd);
    if (res)
    {
        LOGI("%s: cmd[%s] failed", __func__, cmd);
        return res;
    }

    return cellm_ovsdb_cmu_update_lte_priority(mgr, route->cellm_priority);
}

int cellm_set_route_preferred(cellm_mgr_t *mgr)
{
    cellm_route_info_t *route;
    int res;

    route = mgr->cellm_route;
    LOGI("%s: cellm_route[%s][%s]", __func__, route->cellm_if_name, route->cellm_ip_addr);
    if (!route) return -1;

    /* Make sure we have a valid LTE route before changing the WAN metric */
    if (route->cellm_gw[0])
    {
        LOGI("%s: if_name[%s]", __func__, route->wan_if_name);

        route->wan_metric = WAN_L3_FAIL_METRIC;
        /*
         * Route updates are now handled by NM via the Wifi_Route_Config table.
         */
        res = cellm_ovsdb_update_wifi_route_config_metric(mgr, route->wan_if_name, route->wan_metric);
        if (res)
        {
            LOGI("%s: ltem_ovsdb_update_wifi_route_config_metric() failed for [%s]", __func__, route->wan_if_name);
            return res;
        }
    }

    return 0;
}

int cellm_set_wan_route_preferred(cellm_mgr_t *mgr)
{
    cellm_route_info_t *route;
    int res;

    route = mgr->cellm_route;
    LOGI("%s: wan_route[%s][%s]", __func__, route->wan_if_name, route->wan_gw);
    if (!route) return -1;

    /* Make sure we have a valid WAN route before changing the metric */
    if (route->wan_gw[0])
    {
        LOGI("%s: if_name[%s]", __func__, route->wan_if_name);

        route->wan_metric = WAN_DEFAULT_METRIC;
        /*
         * Route updates are now handled by NM via the Wifi_Route_Config table.
         */
        res = cellm_ovsdb_update_wifi_route_config_metric(mgr, route->wan_if_name, route->wan_metric);
        if (res)
        {
            LOGI("%s: ltem_ovsdb_update_wifi_route_config_metric() failed for [%s]", __func__, route->wan_if_name);
            return res;
        }
    }

    return 0;
}

void cellm_flush_flows(cellm_mgr_t *mgr)
{
    char cmd[256];
    int res;

    snprintf(cmd, sizeof(cmd), "conntrack -F");
    res = cmd_log(cmd);
    if (res)
    {
        LOGI("%s: cmd[%s] failed", __func__, cmd);
        return;
    }

    hw_acc_flush_all_flows();

    return;
}
