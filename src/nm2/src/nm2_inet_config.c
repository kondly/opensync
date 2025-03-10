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

#include <stdarg.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>

#include "os.h"
#include "util.h"
#include "memutil.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "nm2.h"
#include "nm2_iface.h"
#include "target.h"

#include "inet.h"


#define MODULE_ID LOG_MODULE_ID_OVSDB

#define SCHEMA_IF_TYPE_VIF "vif"
/*
 * ===========================================================================
 *  Global variables
 * ===========================================================================
 */

static ovsdb_table_t table_Wifi_Inet_Config;

/*
 * ===========================================================================
 *  Forward declarations
 * ===========================================================================
 */
static void callback_Wifi_Inet_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Inet_Config *old_rec,
        struct schema_Wifi_Inet_Config *iconf);

static bool nm2_inet_interface_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_ip_assign_scheme_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_upnp_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_static_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_dns_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_dhcps_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_dhcps_options_set(
        struct nm2_iface *piface,
        const char *opts);

static bool nm2_inet_ip4tunnel_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_ip6tunnel_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_dhsnif_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_vlan_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

static bool nm2_inet_vlan_egress_qos_map_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf);

static bool nm2_inet_credential_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf);

static bool nm2_inet_dhcp_renew_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf);

static void nm2_inet_copy(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *pconfig);

#define SCHEMA_FIND_KEY(x, key)    __find_key( \
        (char *)x##_keys, sizeof(*(x ## _keys)), \
        (char *)x, sizeof(*(x)), x##_len, key)

static const char * __find_key(
        char *keyv,
        size_t keysz,
        char *datav,
        size_t datasz,
        int vlen,
        const char *key);
struct nm2_iface *nm2_add_inet_conf(struct schema_Wifi_Inet_Config *iconf);
void nm2_del_inet_conf(struct schema_Wifi_Inet_Config *old_rec);
struct nm2_iface *nm2_modify_inet_conf(struct schema_Wifi_Inet_Config *iconf);


/*
 * ===========================================================================
 * Initialize Inet Config
 * ===========================================================================
 */
void nm2_inet_config_init(void)
{
    LOGI("Initializing NM Inet Config");

    // Initialize OVSDB tables
    OVSDB_TABLE_INIT(Wifi_Inet_Config, if_name);

    // Initialize OVSDB monitor callbacks
    OVSDB_TABLE_MONITOR(Wifi_Inet_Config, false);

    return;
}


/*
 * ===========================================================================
 *  Interface configuration
 * ===========================================================================
 */

/*
 * Push the configuration from the Wifi_Inet_Config (iconf) to the interface (piface)
 */
bool nm2_inet_config_set(struct nm2_iface *piface, struct schema_Wifi_Inet_Config *iconf)
{
    bool retval = true;

    nm2_inet_copy(piface, iconf);

    retval &= nm2_inet_interface_set(piface, iconf);
    retval &= nm2_inet_ip_assign_scheme_set(piface, iconf);
    retval &= nm2_inet_upnp_set(piface, iconf);
    retval &= nm2_inet_static_set(piface, iconf);
    retval &= nm2_inet_dns_set(piface, iconf);
    retval &= nm2_inet_dhcps_set(piface, iconf);
    retval &= nm2_inet_ip4tunnel_set(piface, iconf);
    retval &= nm2_inet_ip6tunnel_set(piface, iconf);
    retval &= nm2_inet_dhsnif_set(piface, iconf);
    retval &= nm2_inet_vlan_set(piface, iconf);
    retval &= nm2_inet_vlan_egress_qos_map_set(piface, iconf);
    retval &= nm2_inet_credential_set(piface, iconf);
    retval &= nm2_inet_dhcp_renew_set(piface, iconf);

    return retval;
}

/* Apply general interface settings from schema */
bool nm2_inet_interface_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    bool retval = true;

    if (!inet_interface_enable(piface->if_inet, iconf->enabled))
    {
        LOG(WARN, "inet_config: %s (%s): Error enabling interface (%d).",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->enabled);

        retval = false;
    }

    if (!inet_network_enable(piface->if_inet, iconf->network))
    {
        LOG(WARN, "inet_config: %s (%s): Error enabling network (%d).",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->network);

        retval = false;
    }

    if (iconf->mtu_exists && !inet_mtu_set(piface->if_inet, iconf->mtu))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting MTU %d.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->mtu);

        retval = false;
    }

    if (iconf->no_flood_exists && !inet_noflood_set(piface->if_inet, iconf->no_flood))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting no-flood to %s.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->no_flood ? "true" : "false");
    }

    if (!inet_parent_ifname_set(piface->if_inet, iconf->parent_ifname_exists ? iconf->parent_ifname : NULL))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting parent interface name.",
                piface->if_name, nm2_iftype_tostr(piface->if_type));

        retval = false;
    }

    if (!inet_nat_enable(piface->if_inet, iconf->NAT_exists && iconf->NAT))
    {
        LOG(WARN, "inet_config: %s (%s): Error enabling NAT (%d).",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->NAT_exists && iconf->NAT);

        retval = false;
    }

    /* Enable or disable MAC reporting on the interface */
    bool mac_reporting = iconf->mac_reporting_exists ? iconf->mac_reporting : false;
    nm2_mac_reporting_set(piface->if_name, mac_reporting);

    return retval;
}

static void nm2_inet_dhcp_req_options_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    size_t n;
    int ii;
    dhcp_options_t *opts = piface->if_dhcp_req_options;

    /*
     * Compare current and previous options
     */
    for (n = 0; n < opts->length; n++)
    {
        for (ii = 0; ii < iconf->dhcp_req_len; ii++)
        {
            if (iconf->dhcp_req[ii] == opts->option_id[n])
                break;
        }

        /* Option not found */
        if (ii >= iconf->dhcp_req_len) break;
    }

    /*
     * Deleting all old options always triggers unwanted reconfiguration events.
     *
     * The logic here is that if all old options are present in the new set,
     * there's no need to clear all old options. This skips a full reconfiguration
     * event in the case that the two sets are the same.
     *
     * If the input array is empty, either we're using the previous or the
     * default options. In any case, there's nothing to delete.
     */
    if (n < opts->length && iconf->dhcp_req_len > 0)
    {
        // clear old requested options before reconfiguration
        for (n = 0; n < opts->length; n++)
        {
            (void)inet_dhcpc_option_request(piface->if_inet, (enum osn_dhcp_option)opts->option_id[n], false);
        }
    }

    if (iconf->dhcp_req_len > 0)
    {
        FREE(piface->if_dhcp_req_options);
        piface->if_dhcp_req_options = NULL;

        opts = CALLOC(1, sizeof(*opts) +
            iconf->dhcp_req_len * sizeof(opts->option_id[0]));

        opts->length = (size_t)iconf->dhcp_req_len;

        for (n = 0; n < opts->length; n++)
        {
            opts->option_id[n] = (uint8_t)iconf->dhcp_req[n];
        }

        piface->if_dhcp_req_options = opts;
    }
    else // get default options when not provided by interface
    {
        opts = piface->if_dhcp_req_options;
    }

    for (n = 0; n < opts->length; n++)
    {
        (void)inet_dhcpc_option_request(piface->if_inet, (enum osn_dhcp_option)opts->option_id[n], true);
    }
}

/* Apply IP assignment scheme from OVSDB schema */
bool nm2_inet_ip_assign_scheme_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    enum inet_assign_scheme assign_scheme = INET_ASSIGN_NONE;

    if (iconf->ip_assign_scheme_exists)
    {
        if (strcmp(iconf->ip_assign_scheme, "static") == 0)
        {
            assign_scheme = INET_ASSIGN_STATIC;
        }
        else if (strcmp(iconf->ip_assign_scheme, "dhcp") == 0)
        {
            nm2_inet_dhcp_req_options_set(piface, iconf);
            assign_scheme = INET_ASSIGN_DHCP;
        }
    }

    if (!inet_assign_scheme_set(piface->if_inet, assign_scheme))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting the IP assignment scheme: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->ip_assign_scheme);

        return false;
    }

    return true;
}

typedef struct upnp_map_item
{
    const char* name;
    enum osn_upnp_mode mode;    
} t_upnp_map_item;

static const t_upnp_map_item upnp_map[] =
{
    { .name = "disabled", .mode = UPNP_MODE_NONE },
    { .name = "internal", .mode = UPNP_MODE_INTERNAL },
    { .name = "external", .mode = UPNP_MODE_EXTERNAL },
    { .name = "internal_iptv", .mode = UPNP_MODE_INTERNAL_IPTV },
    { .name = "external_iptv", .mode = UPNP_MODE_EXTERNAL_IPTV }
};

enum osn_upnp_mode nm2_str2upnp(const char *str)
{
    size_t n;
    for(n = 0; n < ARRAY_SIZE(upnp_map); ++n)
    {
        if(0 == strcmp(str, upnp_map[n].name))
        {
            return upnp_map[n].mode;
        }
    }
    LOG(WARN, "inet_config: Unknown UPnP mode %s. Assuming \"disabled\".", str);
    return UPNP_MODE_NONE;
}

const char *nm2_upnp2str(enum osn_upnp_mode mode)
{
    size_t n;
    for(n = 0; n < ARRAY_SIZE(upnp_map); ++n)
    {
        if(mode == upnp_map[n].mode)
        {
            return upnp_map[n].name;
        }
    }
    LOG(WARN, "inet_config: Unknown UPnP mode = %d.", mode);
    return NULL;
}

/* Apply UPnP settings from schema */
bool nm2_inet_upnp_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    enum osn_upnp_mode upnp = UPNP_MODE_NONE;

    if (iconf->upnp_mode_exists)
    {
        upnp = nm2_str2upnp(iconf->upnp_mode);
    }

    if (!inet_upnp_mode_set(piface->if_inet, upnp))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting UPnP mode: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->upnp_mode);

        return false;
    }

    return true;
}

/* Apply static IP configuration from schema */
bool nm2_inet_static_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    bool retval = true;

    osn_ip_addr_t ipaddr = OSN_IP_ADDR_INIT;
    osn_ip_addr_t netmask = OSN_IP_ADDR_INIT;
    osn_ip_addr_t bcaddr = OSN_IP_ADDR_INIT;

    if (iconf->inet_addr_exists && !osn_ip_addr_from_str(&ipaddr, iconf->inet_addr))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid IP address: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->inet_addr);
    }

    if (iconf->netmask_exists && !osn_ip_addr_from_str(&netmask, iconf->netmask))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid netmask: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->netmask);
    }

    if (iconf->broadcast_exists && !osn_ip_addr_from_str(&bcaddr, iconf->broadcast))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid broadcast address: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->netmask);
    }

    if (!inet_ipaddr_static_set(piface->if_inet, ipaddr, netmask, bcaddr))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting the static IP configuration,"
                " ipaddr = "PRI_osn_ip_addr
                " netmask = "PRI_osn_ip_addr
                " bcaddr = "PRI_osn_ip_addr".",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                FMT_osn_ip_addr(ipaddr),
                FMT_osn_ip_addr(netmask),
                FMT_osn_ip_addr(bcaddr));

        retval = false;
    }

    osn_ip_addr_t gateway = OSN_IP_ADDR_INIT;
    if (iconf->gateway_exists && !osn_ip_addr_from_str(&gateway, iconf->gateway))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid broadcast address: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                iconf->netmask);
    }

    if (!inet_gateway_set(piface->if_inet, gateway))
    {
        LOG(WARN, "inet_config: %s (%s): Error setting the default gateway "PRI_osn_ip_addr".",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                FMT_osn_ip_addr(gateway));

        retval = false;
    }

    return retval;
}

/* Apply DNS settings from schema */
bool nm2_inet_dns_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    osn_ip_addr_t primary_dns = OSN_IP_ADDR_INIT;
    osn_ip_addr_t secondary_dns = OSN_IP_ADDR_INIT;

    const char *sprimary = SCHEMA_FIND_KEY(iconf->dns, "primary");
    const char *ssecondary = SCHEMA_FIND_KEY(iconf->dns, "secondary");

    if (sprimary != NULL &&
            !osn_ip_addr_from_str(&primary_dns, sprimary))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid primary DNS: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                sprimary);
    }

    if (ssecondary != NULL &&
            !osn_ip_addr_from_str(&secondary_dns, ssecondary))
    {
        LOG(WARN, "inet_config: %s (%s): Invalid secondary DNS: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                ssecondary);
    }

    if (!inet_dns_set(piface->if_inet, primary_dns, secondary_dns))
    {
        LOG(WARN, "inet_config: %s (%s): Error applying DNS settings.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));

        return false;
    }

    return true;
}

/* Apply DHCP server configuration from schema */
bool nm2_inet_dhcps_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    osn_ip_addr_t lease_start = OSN_IP_ADDR_INIT;
    osn_ip_addr_t lease_stop = OSN_IP_ADDR_INIT;
    int lease_time = -1;
    bool retval = true;

    const char *slease_time = SCHEMA_FIND_KEY(iconf->dhcpd, "lease_time");
    const char *slease_start = SCHEMA_FIND_KEY(iconf->dhcpd, "start");
    const char *slease_stop = SCHEMA_FIND_KEY(iconf->dhcpd, "stop");
    const char *sdhcp_opts = SCHEMA_FIND_KEY(iconf->dhcpd, "dhcp_option");

    /*
     * Parse the lease range, start/stop fields are just IPs
     */
    if (slease_start != NULL)
    {
        if (!osn_ip_addr_from_str(&lease_start, slease_start))
        {
            LOG(WARN, "inet_config: %s (%s): Error parsing IP address lease_start: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    slease_start);
            retval = false;
        }
    }

    /* Lease stop options */
    if (slease_stop != NULL)
    {
        if (!osn_ip_addr_from_str(&lease_stop, slease_stop))
        {
            LOG(WARN, "inet_config: %s (%s): Error parsing IP address lease_stop: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    slease_stop);
            retval = false;
        }
    }

    /*
     * Parse the lease time, the lease time may have a suffix denoting the units of time -- h for hours, m for minutes, s for seconds
     */
    if (slease_time != NULL)
    {
        char *suffix;
        /* The lease time is usually expressed with a suffix, for example 12h for hours. */
        lease_time = strtoul(slease_time, &suffix, 10);

        /* If the amount of characters converted is 0, then it's an error */
        if (suffix == slease_time)
        {
            LOG(WARN, "inet_config: %s (%s): Error parsing lease time: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    slease_time);
        }
        else
        {
            switch (*suffix)
            {
                case 'h':
                case 'H':
                    lease_time *= 60;
                    /* FALLTHROUGH */

                case 'm':
                case 'M':
                    lease_time *= 60;
                    /* FALLTHROUGH */

                case 's':
                case 'S':
                case '\0':
                    break;

                default:
                    LOG(WARN, "inet_config: %s (%s): Error parsing lease time -- invalid time suffix: %s",
                            piface->if_name,
                            nm2_iftype_tostr(piface->if_type),
                            slease_time);
                    lease_time = -1;
            }
        }
    }

    /*
     * Parse options -- options are a ';' separated list of comma separated key,value pairs
     */
    if (!nm2_inet_dhcps_options_set(piface, sdhcp_opts))
    {
        LOG(ERR, "inet_config: %s (%s): Error parsing DHCP server options: %s",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                sdhcp_opts);
        retval = false;
    }

    if (!inet_dhcps_range_set(piface->if_inet, lease_start, lease_stop))
    {
        LOG(ERR, "inet_config: %s (%s): Error setting DHCP range.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));
        retval = false;
    }

    if (!inet_dhcps_lease_set(piface->if_inet, lease_time))
    {
        LOG(ERR, "inet_config: %s (%s): Error setting DHCP lease time: %d",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                lease_time);
        retval = false;
    }

    bool enable = slease_start != NULL && slease_stop != NULL;

    /* Enable DHCP lease notifications */
    if (!inet_dhcps_lease_notify(piface->if_inet, nm2_dhcp_lease_notify))
    {
        LOG(ERR, "inet_config: %s (%s): Error registering DHCP lease event handler.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));
        retval = false;
    }

    /* Start the DHCP server */
    if (!inet_dhcps_enable(piface->if_inet, enable))
    {
        LOG(ERR, "inet_config: %s (%s): Error %s DHCP server.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                enable ? "enabling" : "disabling");
        retval = false;
    }

    return retval;
}

/* Apply DHCP server options from schema MAP */
bool nm2_inet_dhcps_options_set(struct nm2_iface *piface, const char *opts)
{
    int ii;

    char *topts = NULL;
    if (opts != NULL) topts = strdup(opts);

    /* opt_track keeps track of options that were set -- we must unset all other options at the end */
    uint8_t opt_track[C_SET_LEN(DHCP_OPTION_MAX, uint8_t)];
    memset(opt_track, 0, sizeof(opt_track));

    /* First, split by semi colons */
    char *psemi = topts;
    char *csemi;

    while ((csemi = strsep(&psemi, ";")) != NULL)
    {
        char *pcomma = csemi;

        /* Split by comma */
        char *sopt = strsep(&pcomma, ",");
        char *sval = pcomma;

        int opt = strtoul(sopt, NULL, 0);
        if (opt <= 0 || opt >= DHCP_OPTION_MAX)
        {
            LOG(WARN, "inet_config: %s (%s): Invalid DHCP option specified: %s. Ignoring.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    sopt);
            continue;
        }

        C_SET_ADD(opt_track, opt);

        if (!inet_dhcps_option_set(piface->if_inet, opt, sval))
        {
            LOG(WARN, "inet_config: %s (%s): Error setting DHCP option %d.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    opt);
        }
    }

    /* Clear all other options */
    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        if (C_IS_SET(opt_track, ii)) continue;

        if (!inet_dhcps_option_set(piface->if_inet, ii, NULL))
        {
            LOG(WARN, "inet_config: %s (%s): Error clearing DHCP option %d.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    ii);
        }
    }

    if (topts != NULL) FREE(topts);

    return true;
}

/* Apply IPv4 tunnel settings */
bool nm2_inet_ip4tunnel_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    bool retval = true;
    const char *ifparent = NULL;
    osn_ip_addr_t local_addr = OSN_IP_ADDR_INIT;
    osn_ip_addr_t remote_addr = OSN_IP_ADDR_INIT;
    osn_mac_addr_t remote_mac = OSN_MAC_ADDR_INIT;

    // support only GRE tunnel types
    if (piface->if_type != NM2_IFTYPE_GRE)
    {
        return true;
    }

    if (iconf->gre_ifname_exists)
    {
        if (iconf->gre_ifname[0] == '\0')
        {
            LOG(WARN, "inet_config: %s (%s): Parent interface exists, but is empty.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type));
            retval = false;
        }

        ifparent = iconf->gre_ifname;
    }

    if (iconf->gre_local_inet_addr_exists)
    {
        if (!osn_ip_addr_from_str(&local_addr, iconf->gre_local_inet_addr))
        {
            LOG(WARN, "inet_config: %s (%s): Local IPv4 address is invalid: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->gre_local_inet_addr);

            retval = false;
        }
    }

    if (iconf->gre_remote_inet_addr_exists)
    {
        if (!osn_ip_addr_from_str(&remote_addr, iconf->gre_remote_inet_addr))
        {
            LOG(WARN, "inet_config: %s (%s): Remote IPv4 address is invalid: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->gre_remote_inet_addr);

            retval = false;
        }
    }

    if (iconf->gre_remote_mac_addr_exists)
    {
        if (!osn_mac_addr_from_str(&remote_mac, iconf->gre_remote_mac_addr))
        {
            LOG(WARN, "inet_config: %s (%s): Remote MAC address is invalid: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->gre_remote_mac_addr);

            retval = false;
        }
    }

    if (!inet_ip4tunnel_set(piface->if_inet, ifparent, local_addr, remote_addr, remote_mac))
    {
        LOG(ERR, "inet_config: %s (%s): Error setting IPv4 tunnel settings: parent=%s local="PRI_osn_ip_addr" remote="PRI_osn_ip_addr" remote_mac="PRI_osn_mac_addr,
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                ifparent,
                FMT_osn_ip_addr(local_addr),
                FMT_osn_ip_addr(remote_addr),
                FMT_osn_mac_addr(remote_mac));

        retval = false;
    }

    return retval;
}

/* Apply IPv6 tunnel settings */
bool nm2_inet_ip6tunnel_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    bool retval = true;
    const char *ifparent = NULL;
    osn_ip6_addr_t local_addr = OSN_IP6_ADDR_INIT;
    osn_ip6_addr_t remote_addr = OSN_IP6_ADDR_INIT;
    osn_mac_addr_t remote_mac = OSN_MAC_ADDR_INIT;

    // support only GRE tunnel types
    if (piface->if_type != NM2_IFTYPE_GRE6)
    {
        return true;
    }

    if (iconf->gre_ifname_exists)
    {
        if (iconf->gre_ifname[0] == '\0')
        {
            LOG(WARN, "inet_config: %s (%s): Parent interface exists, but is empty.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type));
            retval = false;
        }

        ifparent = iconf->gre_ifname;
    }

    if (iconf->gre_local_inet_addr_exists)
    {
        if (!osn_ip6_addr_from_str(&local_addr, iconf->gre_local_inet_addr))
        {
            LOG(WARN, "inet_config: %s (%s): Local IPv6 address is invalid: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->gre_local_inet_addr);

            retval = false;
        }
    }

    if (iconf->gre_remote_inet_addr_exists)
    {
        if (!osn_ip6_addr_from_str(&remote_addr, iconf->gre_remote_inet_addr))
        {
            LOG(WARN, "inet_config: %s (%s): Remote IPv6 address is invalid: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->gre_remote_inet_addr);

            retval = false;
        }
    }

    if (iconf->gre_remote_mac_addr_exists)
    {
        if (!osn_mac_addr_from_str(&remote_mac, iconf->gre_remote_mac_addr))
        {
            LOG(WARN, "inet_config: %s (%s): Remote MAC address is invalid: %s",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type),
                    iconf->gre_remote_mac_addr);

            retval = false;
        }
    }

    if (!inet_ip6tunnel_set(piface->if_inet, ifparent, local_addr, remote_addr, remote_mac))
    {
        LOG(ERR, "inet_config: %s (%s): Error setting IPv6 tunnel settings: parent=%s local="PRI_osn_ip6_addr" remote="PRI_osn_ip6_addr" remote_mac="PRI_osn_mac_addr,
                piface->if_name,
                nm2_iftype_tostr(piface->if_type),
                ifparent,
                FMT_osn_ip6_addr(local_addr),
                FMT_osn_ip6_addr(remote_addr),
                FMT_osn_mac_addr(remote_mac));

        retval = false;
    }

    return retval;
}

/* Apply DHCP sniffing configuration from schema */
bool nm2_inet_dhsnif_set(struct nm2_iface *piface, const struct schema_Wifi_Inet_Config *iconf)
{
    /*
     * Enable or disable the DHCP sniffing callback according to schema values
     */
    if (iconf->dhcp_sniff_exists && iconf->dhcp_sniff)
    {
        return inet_dhsnif_lease_notify(piface->if_inet, nm2_dhcp_lease_notify);
    }

    return inet_dhsnif_lease_notify(piface->if_inet, NULL);
}

/* Apply VLAN configuration from schema */
bool nm2_inet_vlan_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    int vlanid = 0;

    /* Not supported for non-VLAN types */
    if (piface->if_type != NM2_IFTYPE_VLAN) return true;

    if (iconf->vlan_id_exists)
    {
        vlanid = iconf->vlan_id;
    }

    return inet_vlanid_set(piface->if_inet, vlanid);
}

static bool nm2_inet_vlan_egress_qos_map_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    /* Not supported for non-VLAN types */
    if (piface->if_type != NM2_IFTYPE_VLAN) return true;

    return inet_vlan_egress_qos_map_set(piface->if_inet, 
        iconf->vlan_egress_qos_map_exists ? iconf->vlan_egress_qos_map : ""/*=safe,default*/);
}


/* Set interface credentials */
bool nm2_inet_credential_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    /*
     * Currently only PPPoE is supported
     */
    if (piface->if_type != NM2_IFTYPE_PPPOE) return true;

    const char *username = SCHEMA_FIND_KEY(iconf->ppp_options, "username");
    const char *password = SCHEMA_FIND_KEY(iconf->ppp_options, "password");

    return inet_credential_set(piface->if_inet, username, password);
}

bool nm2_inet_dhcp_renew_set(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    uint32_t dhcp_renew;

    if (iconf->dhcp_renew_exists)
    {
        dhcp_renew = iconf->dhcp_renew;
    }
    else
    {
        LOG(WARN, "%s: dhcp_renew is not existing", __func__);
        return true;
    }

    return inet_dhcp_renew_set(piface->if_inet, dhcp_renew);
}

/*
 * Some fields must be cached for later retrieval; these are typically used
 * for populating the Wifi_Inet_State and Wifi_Master_State tables.
 *
 * This function is responsible for copying these fields from the schema
 * structure to nm2_iface.
 */
void nm2_inet_copy(
        struct nm2_iface *piface,
        const struct schema_Wifi_Inet_Config *iconf)
{
    /* Copy fields that should be simply copied to Wifi_Inet_State */
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache._uuid, iconf->_uuid);
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.if_uuid, iconf->if_uuid);
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.dhcpd, iconf->dhcpd);
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.dhcpd_keys, iconf->dhcpd_keys);
    piface->if_cache.dhcpd_len = iconf->dhcpd_len;
    piface->if_cache.gre_ifname_exists = iconf->gre_ifname_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.gre_ifname, iconf->gre_ifname);
    piface->if_cache.gre_ifname_exists = iconf->gre_ifname_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.gre_ifname, iconf->gre_ifname);
    piface->if_cache.gre_remote_inet_addr_exists = iconf->gre_remote_inet_addr_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.gre_remote_inet_addr, iconf->gre_remote_inet_addr);
    piface->if_cache.gre_local_inet_addr_exists = iconf->gre_local_inet_addr_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.gre_local_inet_addr, iconf->gre_local_inet_addr);
    piface->if_cache.vlan_id_exists = iconf->vlan_id_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.vlan_id, iconf->vlan_id);
    piface->if_cache.vlan_egress_qos_map_exists = iconf->vlan_egress_qos_map_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.vlan_egress_qos_map, iconf->vlan_egress_qos_map);
    piface->if_cache.parent_ifname_exists = iconf->parent_ifname_exists;
    NM2_IFACE_INET_CONFIG_COPY(piface->if_cache.parent_ifname, iconf->parent_ifname);
}


struct nm2_iface *nm2_add_inet_conf(struct schema_Wifi_Inet_Config *iconf)
{
    enum nm2_iftype iftype;
    struct nm2_iface *piface = NULL;

    LOG(INFO, "nm2_add_inet_conf: INSERT interface %s (type %s).",
            iconf->if_name,
            iconf->if_type);

    if (!nm2_iftype_fromstr(&iftype, iconf->if_type))
    {
        LOG(ERR, "nm2_add_inet_conf: %s: Unknown interface type %s. Unable to create interface.",
                iconf->if_name,
                iconf->if_type);
        return NULL;
    }

    piface = nm2_iface_new(iconf->if_name, iftype);
    if (piface == NULL)
    {
        LOG(ERR, "nm2_add_inet_conf: %s (%s): Unable to create interface.",
                iconf->if_name,
                iconf->if_type);
        return NULL;
    }

    return piface;
}

void nm2_del_inet_conf(struct schema_Wifi_Inet_Config *old_rec)
{
    struct nm2_iface *piface = NULL;

    LOG(INFO, "nm2_del_inet_conf: interface %s (type %s).", old_rec->if_name, old_rec->if_type);

    piface = nm2_iface_get_by_name(old_rec->if_name);
    if (piface == NULL)
    {
        LOG(ERR, "nm2_del_inet_conf: Unable to delete non-existent interface %s.", old_rec->if_name);
    }

    if (piface != NULL && !nm2_iface_del(piface))
    {
        LOG(ERR, "nm2_del_inet_conf: Error during destruction of interface %s.", old_rec->if_name);
    }

    nm2_inet_state_del(old_rec->if_name);

    return;
}

struct nm2_iface *nm2_modify_inet_conf(struct schema_Wifi_Inet_Config *iconf)
{
    enum nm2_iftype iftype;
    struct nm2_iface *piface = NULL;

    LOG(INFO, "nm2_modify_inet_conf: UPDATE interface %s (type %s).",
            iconf->if_name,
            iconf->if_type);


    if (!nm2_iftype_fromstr(&iftype, iconf->if_type))
    {
        LOG(ERR, "nm2_modify_inet_conf: %s: Unknown interface type %s. Unable to create interface.",
                iconf->if_name,
                iconf->if_type);
        return NULL;
    }

    piface = nm2_iface_get_by_name(iconf->if_name);
    if (piface == NULL)
    {
        LOG(ERR, "nm2_modify_inet_conf: %s (%s): Unable to modify interface, didn't find it.",
        iconf->if_name,
        iconf->if_type);
    }

    return piface;
}


/*
 * OVSDB Wifi_Inet_Config table update handler.
 */
void callback_Wifi_Inet_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Inet_Config *old_rec,
        struct schema_Wifi_Inet_Config *iconf)
{
    struct nm2_iface *piface = NULL;

    TRACE();

    switch(mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            piface = nm2_add_inet_conf(iconf);
            if (piface == NULL) break;
            nm2_mcast_init_ifc(piface);
            break;
        case OVSDB_UPDATE_DEL:
            nm2_del_inet_conf(old_rec);
            break;
        case OVSDB_UPDATE_MODIFY:
            piface = nm2_modify_inet_conf(iconf);
            break;
        default:
            LOG(ERR, "Invalid Wifi_Inet_Config mon_type(%d)", mon->mon_type);
    }

    if (mon->mon_type == OVSDB_UPDATE_NEW ||
        mon->mon_type == OVSDB_UPDATE_MODIFY)
    {

        if(!piface)
        {
            LOG(ERR, "callback_Wifi_Inet_Config: Couldn't get the interface(%s)",
            iconf->if_name);
            return;
        }

        /* Push the configuration to the interface */
        if (!nm2_inet_config_set(piface, iconf))
        {
            LOG(ERR, "callback_Wifi_Inet_Config: %s (%s): Unable to set configuration",
                    iconf->if_name,
                    iconf->if_type);
        }

        /* Apply the configuration */
        if (!nm2_iface_apply(piface))
        {
            LOG(ERR, "callback_Wifi_Inet_Config: %s (%s): Unable to apply configuration.",
                    iconf->if_name,
                    iconf->if_type);
        }

        if (!nm2_inet_state_update(piface))
        {
            LOG(ERR, "callback_Wifi_Inet_Config: %s: Error updating state.",
                    iconf->if_name);
        }
    }

    return;
}

/*
 * ===========================================================================
 *  MISC
 * ===========================================================================
 */

/**
 * Lookup a value by key in a schema map structure.
 *
 * A schema map structures is defined as follows:
 *
 * char name[NAME_SZ][LEN]      - value array
 * char name_keys[KEY_SZ][LEN]  - key array
 * char name_len;               - length of name and name_keys arrays
 */
const char * __find_key(char *keyv, size_t keysz, char *datav, size_t datasz, int vlen, const char *key)
{
    int ii;

    for (ii = 0; ii < vlen; ii++)
    {
        if (strcmp(keyv, key) == 0) return datav;

        keyv += keysz;
        datav += datasz;
    }

    return NULL;
}

