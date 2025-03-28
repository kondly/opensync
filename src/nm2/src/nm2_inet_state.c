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

/*
 * ===========================================================================
 *  NM2 Wifi_Inet_State and Wifi_Master_State
 * ===========================================================================
 */
#include "log.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "nm2_iface.h"

/*
 * ===========================================================================
 *  Global Variables
 * ===========================================================================
 */
static ovsdb_table_t table_Wifi_Inet_State;
static ovsdb_table_t table_Wifi_Master_State;

/*
 * ===========================================================================
 *  Forward declarations
 * ===========================================================================
 */
static void nm2_inet_state_to_schema(
        struct schema_Wifi_Inet_State *pstate,
        struct nm2_iface *piface);

static void nm2_inet_state_master_to_schema(
        struct schema_Wifi_Master_State *pstate,
        struct nm2_iface *piface);

/**
 * UPnP mode to string conversion function */
extern const char * nm2_upnp2str(enum osn_upnp_mode mode);

/**
 * DHCP option id to name conversion
 * The implementation is Plume specific, since there is no standard
 * with definitions of exact DHCP option names.
 * @param opt_id option id in 1..255 range
 * @return found option name or empty string when not found
 */
extern const char* dhcp_option_name(int opt_id);

/*
 * Initialize NM Inet State OVSDB tables
 */
void nm2_inet_state_init(void)
{
    OVSDB_TABLE_INIT(Wifi_Inet_State, if_name);
    OVSDB_TABLE_INIT(Wifi_Master_State, if_name);
}

/*
 * Update interface state and master state
 */
bool nm2_inet_state_update(struct nm2_iface *piface)
{
    struct schema_Wifi_Inet_State istate;
    struct schema_Wifi_Master_State mstate;

    /* Update inet state */
    nm2_inet_state_to_schema(&istate, piface);
    if (!ovsdb_table_upsert(&table_Wifi_Inet_State, &istate, false))
    {
        LOG(ERR, "nm2_inet_state: %s (%s): Unable to update Wifi_Inet_State.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));
    }

    /* Update master state */
    nm2_inet_state_master_to_schema(&mstate, piface);
    if (!ovsdb_table_upsert(&table_Wifi_Master_State, &mstate, false))
    {
        LOG(ERR, "nm2_inet_state: %s (%s): Unable to update Wifi_Master_State.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));
    }

    return true;
}

/**
 * Convert the interface status in @piface to a schema inet state structure
 */
void nm2_inet_state_to_schema(
        struct schema_Wifi_Inet_State *pstate,
        struct nm2_iface *piface)
{
    const char *if_type;

    /**
     * Fill out the schema_Wifi_Inet_State structure
     */

    /* Clear the current state */
    memset(pstate, 0, sizeof(*pstate));

    strscpy(pstate->if_name, piface->if_name, sizeof(pstate->if_name));

    pstate->enabled = piface->if_inet_state.in_interface_enabled;
    pstate->network = piface->if_inet_state.in_network_enabled;

    pstate->mtu_exists = true;
    pstate->mtu = piface->if_inet_state.in_mtu;

    pstate->NAT_exists = true;
    pstate->NAT = piface->if_inet_state.in_nat_enabled;

    pstate->inet_addr_exists = true;
    snprintf(pstate->inet_addr, sizeof(pstate->inet_addr), PRI_osn_ip_addr,
            FMT_osn_ip_addr( piface->if_inet_state.in_ipaddr));

    pstate->netmask_exists = true;
    snprintf(pstate->netmask, sizeof(pstate->netmask), PRI_osn_ip_addr,
            FMT_osn_ip_addr( piface->if_inet_state.in_netmask));

    pstate->broadcast_exists = true;
    snprintf(pstate->broadcast, sizeof(pstate->broadcast), PRI_osn_ip_addr,
            FMT_osn_ip_addr( piface->if_inet_state.in_bcaddr));

    pstate->gateway_exists = true;
    snprintf(pstate->gateway, sizeof(pstate->gateway), PRI_osn_ip_addr,
            FMT_osn_ip_addr( piface->if_inet_state.in_gateway));

    snprintf(pstate->hwaddr, sizeof(pstate->hwaddr), PRI_osn_mac_addr,
            FMT_osn_mac_addr(piface->if_inet_state.in_macaddr));

    /* Retrieve speed and duplex mode (use breakthrough to add new if types)  */
    switch (piface->if_type)
    {
        /* Retrieve speed and duplex type for ethernet interfaces */
        case NM2_IFTYPE_ETH:
            SCHEMA_SET_INT(pstate->link_speed, piface->if_inet_state.in_speed);
            switch(piface->if_inet_state.in_duplex)
            {
                case OSN_DUPLEX_FULL:
                    SCHEMA_SET_STR(pstate->link_duplex, "full");
                    break;
                case OSN_DUPLEX_HALF:
                    SCHEMA_SET_STR(pstate->link_duplex, "half");
                    break;
                default:
                    SCHEMA_SET_STR(pstate->link_duplex, "unknown");
            }

        default:
            break;
    }

    /*
     * Copy the assignment scheme
     */
    char *scheme = NULL;
    switch (piface->if_inet_state.in_assign_scheme)
    {
        case INET_ASSIGN_NONE:
            scheme = "none";
            break;

        case INET_ASSIGN_STATIC:
            scheme = "static";
            break;

        case INET_ASSIGN_DHCP:
            scheme = "dhcp";

        default:
            break;
    }

    if (scheme != NULL && strscpy(pstate->ip_assign_scheme, scheme, sizeof(pstate->ip_assign_scheme)) > 0)
    {
        pstate->ip_assign_scheme_exists = true;
    }
    else
    {
        pstate->ip_assign_scheme_exists = false;
    }

    /*
     * UPnP mode
     */
    const char *upnp = nm2_upnp2str(piface->if_inet_state.in_upnp_mode);
    pstate->upnp_mode_exists = (upnp != NULL);
    if(pstate->upnp_mode_exists)
    {
        strscpy(pstate->upnp_mode, upnp, sizeof(pstate->upnp_mode));
    }

    if_type = nm2_iftype_tostr(piface->if_type);
    if (if_type != NULL)
    {
        STRSCPY(pstate->if_type, if_type);
    }

    pstate->dns_present = true;
    int di = 0;

    if (osn_ip_addr_cmp(&OSN_IP_ADDR_INIT, &piface->if_inet_state.in_dns1) != 0)
    {
        STRSCPY(pstate->dns_keys[di], "primary");
        snprintf(pstate->dns[di], sizeof(pstate->dns[di]), PRI_osn_ip_addr,
                FMT_osn_ip_addr(piface->if_inet_state.in_dns1));
        di++;
    }

    if (osn_ip_addr_cmp(&OSN_IP_ADDR_INIT, &piface->if_inet_state.in_dns2) != 0)
    {
        STRSCPY(pstate->dns_keys[di], "secondary");
        snprintf(pstate->dns[di], sizeof(pstate->dns[di]), PRI_osn_ip_addr,
                FMT_osn_ip_addr(piface->if_inet_state.in_dns2));
        di++;
    }

    pstate->dns_len = di;

    //NM2_IFACE_INET_CONFIG_COPY(pstate->if_uuid, piface->if_cache.if_uuid);
    /* XXX: if_uuid must be populated from the _uuid field of Inet_Config -- if_uuid is not an uuid type though */
    strscpy(pstate->if_uuid, (char *)piface->if_cache._uuid, sizeof(pstate->if_uuid));

    NM2_IFACE_INET_CONFIG_COPY(pstate->inet_config, piface->if_cache._uuid);
    NM2_IFACE_INET_CONFIG_COPY(pstate->dhcpd, piface->if_cache.dhcpd);
    NM2_IFACE_INET_CONFIG_COPY(pstate->dhcpd_keys, piface->if_cache.dhcpd_keys);
    pstate->dhcpd_len = piface->if_cache.dhcpd_len;

    pstate->gre_ifname_exists = piface->if_cache.gre_ifname_exists;
    NM2_IFACE_INET_CONFIG_COPY(pstate->gre_ifname, piface->if_cache.gre_ifname);
    pstate->gre_remote_inet_addr_exists = piface->if_cache.gre_remote_inet_addr_exists;
    NM2_IFACE_INET_CONFIG_COPY(pstate->gre_remote_inet_addr, piface->if_cache.gre_remote_inet_addr);
    pstate->gre_local_inet_addr_exists = piface->if_cache.gre_local_inet_addr_exists;
    NM2_IFACE_INET_CONFIG_COPY(pstate->gre_local_inet_addr, piface->if_cache.gre_local_inet_addr);
    NM2_IFACE_INET_CONFIG_COPY(pstate->vlan_id, piface->if_cache.vlan_id);
    pstate->vlan_id_exists = piface->if_cache.vlan_id_exists;
    NM2_IFACE_INET_CONFIG_COPY(pstate->vlan_egress_qos_map, piface->if_cache.vlan_egress_qos_map);
    pstate->vlan_egress_qos_map_exists = piface->if_cache.vlan_egress_qos_map_exists;
    NM2_IFACE_INET_CONFIG_COPY(pstate->parent_ifname, piface->if_cache.parent_ifname);
    pstate->parent_ifname_exists = piface->if_cache.parent_ifname_exists;


    /* Unsupported fields, for now */
    pstate->softwds_mac_addr_exists = false;
    pstate->softwds_wrap = false;

    /* Add DHCP client options */
    const dhcp_options_t *p_opts = piface->if_dhcp_req_options;
    size_t n;

    for(n = 0, pstate->dhcpc_len = 0; n < p_opts->length; n++)
    {
        enum osn_dhcp_option optid = (enum osn_dhcp_option)p_opts->option_id[n];
        const char *value; bool req;
        if (inet_dhcpc_option_get(piface->if_inet, optid, &req, &value) && value != NULL)
        {
            STRSCPY(pstate->dhcpc_keys[pstate->dhcpc_len], dhcp_option_name(optid));
            STRSCPY(pstate->dhcpc[pstate->dhcpc_len], value);
            pstate->dhcpc_len++;
        }
    }
}

/*
 * Create a Wifi_Master_State structure from the cached interface state
 */
void nm2_inet_state_master_to_schema(
        struct schema_Wifi_Master_State *pstate,
        struct nm2_iface *piface)
{
    const char *if_type;

    /**
     * Fill out the schema_Wifi_Inet_State structure
     */

    /* Clear the current state */
    memset(pstate, 0, sizeof(*pstate));

    strscpy(pstate->if_name, piface->if_name, sizeof(pstate->if_name));

    pstate->port_state_exists = true;
    snprintf(pstate->port_state, sizeof(pstate->port_state), "%s",
            piface->if_inet_state.in_port_status ? "active" : "inactive");

    pstate->network_state_exists = true;
    snprintf(pstate->network_state, sizeof(pstate->network_state), "%s",
            piface->if_inet_state.in_network_enabled ? "up" : "down");

    pstate->inet_addr_exists = true;
    snprintf(pstate->inet_addr, sizeof(pstate->inet_addr), PRI_osn_ip_addr,
            FMT_osn_ip_addr(piface->if_inet_state.in_ipaddr));

    pstate->netmask_exists = true;
    snprintf(pstate->netmask, sizeof(pstate->netmask), PRI_osn_ip_addr,
            FMT_osn_ip_addr(piface->if_inet_state.in_netmask));

    if_type = nm2_iftype_tostr(piface->if_type);
    if (if_type != NULL)
    {
        STRSCPY(pstate->if_type, if_type);
    }

    // Apparently this is a required field, but it's not really used in PML1.2
    strscpy(pstate->if_uuid.uuid, "00000000-0000-0000-0000-000000000000", sizeof(pstate->if_uuid.uuid));
    pstate->if_uuid_exists = true;

    /* XXX: Unsupported fields for now */
    pstate->onboard_type_exists = false;
    pstate->uplink_priority_exists = false;

    /* Do not update the port_status for VIF interfaces */
    if (piface->if_type == NM2_IFTYPE_VIF)
    {
        //LOG(TRACE, "inet_state: %s (%s): mark port_state not present", piface->if_name, nm2_iftype_tostr(piface->if_type));
        schema_Wifi_Master_State_mark_all_present(pstate);
        pstate->_partial_update = true;
        pstate->port_state_present = false;
    }

    /* Add DHCP client options */
    const dhcp_options_t *p_opts = piface->if_dhcp_req_options;
    size_t n;

    for(n = 0, pstate->dhcpc_len = 0; n < p_opts->length; n++)
    {
        enum osn_dhcp_option optid = (enum osn_dhcp_option)p_opts->option_id[n];
        const char *value; bool req;
        if (inet_dhcpc_option_get(piface->if_inet, optid, &req, &value) && value != NULL)
        {
            STRSCPY(pstate->dhcpc_keys[pstate->dhcpc_len], dhcp_option_name(optid));
            STRSCPY(pstate->dhcpc[pstate->dhcpc_len], value);
            pstate->dhcpc_len++;
        }
    }
}

/*
 * Delete inet and master state rows matching interface name
 */
bool nm2_inet_state_del(const char *ifname)
{
    int rc;

    bool retval = true;

    rc = ovsdb_table_delete_simple(
                &table_Wifi_Inet_State,
                SCHEMA_COLUMN(Wifi_Inet_State, if_name),
                ifname);
    if (rc <= 0)
    {
        LOG(ERR, "inet_state: Error deleting Wifi_Inet_State for interface %s.", ifname);
        retval = false;
    }

    rc = ovsdb_table_delete_simple(
                &table_Wifi_Master_State,
                SCHEMA_COLUMN(Wifi_Inet_State, if_name),
                ifname);
    if (rc <= 0)
    {
        LOG(ERR, "inet_state: Error deleting Wifi_Master_State for interface %s.", ifname);
        retval = false;
    }

    return retval;
}

/*
 * Copy interface state and update status
 */
void nm2_inet_state_fn(inet_t *self, const inet_state_t *state)
{
    struct nm2_iface *piface = self->in_data;

    piface->if_inet_state = *state;
    nm2_inet_state_update(piface);
}

