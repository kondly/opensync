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

#include "osa_assert.h"
#include "log.h"
#include "util.h"
#include "memutil.h"
#include "build_version.h"
#include "inet.h"
#include "evx.h"
#include "schema.h"
#include "osp_unit.h"
#include "target.h"

#include "nm2.h"
#include "nm2_iface.h"
#include "nm2_nb_port.h"
#include "kconfig.h"

/*
 * ===========================================================================
 *  Globals and forward declarations
 * ===========================================================================
 */
static ds_key_cmp_t nm2_iface_cmp;

ds_tree_t nm2_iface_list = DS_TREE_INIT(nm2_iface_cmp, struct nm2_iface, if_tnode);
/* Ordered list of pending commits */
ds_dlist_t nm2_iface_commit_list = DS_DLIST_INIT(struct nm2_iface, if_commit_dnode);

static void nm2_iface_dhcpc_notify(inet_t *self, enum osn_dhcp_option opt, const char *value);
static inet_t *nm2_iface_new_inet(const char *ifname, enum nm2_iftype type);
static os_fdbuf_pcap_t *nm2_iface_new_fdbuf_pcap(const char *ifname, enum nm2_iftype type);

static const uint8_t dhcp_default_req_options[] = {
    DHCP_OPTION_SUBNET_MASK,
    DHCP_OPTION_ROUTER,
    DHCP_OPTION_DNS_SERVERS,
    DHCP_OPTION_DOMAIN_NAME,
    DHCP_OPTION_BCAST_ADDR,
    DHCP_OPTION_VENDOR_SPECIFIC
 };

/*
 * ===========================================================================
 *  Type handling
 * ===========================================================================
 */

/*
 * Convert an NM2_IFTYPE_* enum to a human readable string
 */
const char *nm2_iftype_tostr(enum nm2_iftype type)
{
    const char *iftype_str[NM2_IFTYPE_MAX + 1] =
    {
        #define _STR(sym, str) [sym] = str,
        NM2_IFTYPE(_STR)
        #undef _STR
    };

    ASSERT(type <= NM2_IFTYPE_MAX, "Unknown nm2_iftype value");

    return iftype_str[type];
}

/*
 * Convert iftype as string to an enum
 */
bool nm2_iftype_fromstr(enum nm2_iftype *type, const char *str)
{
    const char *iftype_str_map[NM2_IFTYPE_MAX + 1] =
    {
        #define _STR(sym, str) [sym] = str,
        NM2_IFTYPE(_STR)
        #undef _STR
    };

    int ii;

    for (ii = 0; ii < NM2_IFTYPE_MAX; ii++)
    {
        if (iftype_str_map[ii] == NULL) break;

        if (strcmp(iftype_str_map[ii], str) == 0)
        {
            *type = ii;
            return true;
        }
    }

    *type = NM2_IFTYPE_NONE;

    return false;
}

/*
 * ===========================================================================
 *  Interface handling
 * ===========================================================================
 */

/*
 * General initializer
 */
bool nm2_iface_init(void)
{
    return true;
}

/*
 * Find and return the interface @p ifname.
 * If the interface is not found  NULL will
 * be returned.
 */
struct nm2_iface *nm2_iface_get_by_name(char *ifname)
{
    return ds_tree_find(&nm2_iface_list, ifname);
}

static void nm2_dhcp_opt_update(struct ev_loop *loop, ev_debounce *w, int revent)
{
    struct nm2_iface *piface = (struct nm2_iface *)w->data;

    /* Force a status update */
    nm2_inet_state_update(piface);
}

/*
* Creates a new interface of the specified type.
* Also initializes dhcp_options and inet interface.
*/
struct nm2_iface *nm2_iface_new(const char *_ifname, enum nm2_iftype if_type)
{
    TRACE();

    const char *ifname = _ifname;

    struct nm2_iface *piface;

    piface = CALLOC(1, sizeof(struct nm2_iface));
    piface->if_type = if_type;

    if (strscpy(piface->if_name, ifname, sizeof(piface->if_name)) < 0)
    {
        LOG(ERR, "nm2_iface_new: %s (%s): Error creating interface, name too long.",
                ifname,
                nm2_iftype_tostr(if_type));
        FREE(piface);
        return NULL;
    }

    /* Dynamically initialize the DHCP client options */
    dhcp_options_t *opts = CALLOC(1, sizeof(*opts) + sizeof(dhcp_default_req_options));
    memcpy(opts->option_id, dhcp_default_req_options, sizeof(dhcp_default_req_options));
    opts->length = ARRAY_SIZE(dhcp_default_req_options);
    piface->if_dhcp_req_options = opts;

    ev_debounce_init(&piface->if_opt_debounce, nm2_dhcp_opt_update, 0.5);
    piface->if_opt_debounce.data = piface;

    piface->if_inet = nm2_iface_new_inet(ifname, if_type);
    if (piface->if_inet == NULL)
    {
        LOG(ERR, "nm2_iface_new: %s (%s): Error creating interface, constructor failed.",
                ifname,
                nm2_iftype_tostr(if_type));

        FREE(piface);
        return NULL;
    }

    piface->if_fdbuf_pcap = nm2_iface_new_fdbuf_pcap(ifname, if_type);

    /* Setup private data pointer */
    piface->if_inet->in_data = piface;

    ds_tree_insert(&nm2_iface_list, piface, piface->if_name);

    nm2_iface_status_register(piface);

    /*
     * Wifi_Route_Config can be populated before Wifi_Inet_Config, therefore the
     * route may exists before the interface is created. To deal with this
     * scenario, re-apply the routes when the interface is created
     */
    nm2_route_cfg_reapply(piface);
    nm2_route6_cfg_reapply(piface);

    LOG(INFO, "nm2_iface_new: %s: Created new interface (type %s).", ifname, nm2_iftype_tostr(if_type));

    return piface;
}

/*
 * Delete interface and associated structures
 */
bool nm2_iface_del(struct nm2_iface *piface)
{
    bool retval = true;
    struct nm2_ip_interface *if_ipi;

    if_ipi = piface->if_ipi;

    /* Remove the interface from the "pending commits" list */
    if (piface->if_commit)
    {
        ds_dlist_remove(&nm2_iface_commit_list, piface);
    }

    os_fdbuf_pcap_drop_safe(&piface->if_fdbuf_pcap);

    /* Destroy the inet object */
    if (!inet_del(piface->if_inet))
    {
        LOG(WARN, "nm2_iface: %s (%s): Error deleting interface.",
                piface->if_name,
                nm2_iftype_tostr(piface->if_type));
        retval = false;
    }

    /* Destroy DHCP options list */
    ev_debounce_stop(EV_DEFAULT, &piface->if_opt_debounce);
    FREE(piface->if_dhcp_req_options);

    if (if_ipi != NULL && if_ipi->ipi_iface == piface)
    {
        if_ipi->ipi_iface = NULL;
    }

    /* Remove interface from global interface list */
    ds_tree_remove(&nm2_iface_list, piface);

    FREE(piface);
    return retval;
}

/*
 * Retrieve a nm2_iface structure by looking it up using the ip address
 */
struct nm2_iface *nm2_iface_find_by_ipv4(osn_ip_addr_t addr)
{
    TRACE();

    osn_ip_addr_t if_addr;
    osn_ip_addr_t if_subnet;
    osn_ip_addr_t addr_subnet;

    struct nm2_iface *piface;

    ds_tree_foreach(&nm2_iface_list, piface)
    {
        int prefix;

        if_addr = piface->if_inet_state.in_ipaddr;

        /*
         * Derive the prefix from the "netmask" address in the if_state structure  and assign it
         * to both if_addr and addr
         */
        prefix = osn_ip_addr_to_prefix(&piface->if_inet_state.in_netmask);
        if (prefix == 0) continue;

        if_addr.ia_prefix = prefix;
        addr.ia_prefix = prefix;

        /* Calculate the subnets of both IPs */
        if_subnet = osn_ip_addr_subnet(&if_addr);
        addr_subnet = osn_ip_addr_subnet(&addr);

        /* If both subnets match, it means that addr is part of the IP range in piface */
        if (osn_ip_addr_cmp(&if_subnet, &addr_subnet) == 0)
        {
            return piface;
        }
    }

    return NULL;
}

/*
 * Apply the pending configuration to the runtime system.
 *
 * This function is split into two parts, nm2_iface_apply() which just schedules
 * a callback to __nm2_iface_apply().
 *
 * Note: The actual configuration will be applied after the return point of
 * this function.
 */

void __nm2_iface_apply(EV_P_ ev_debounce *w, int revent)
{
    (void)loop;
    (void)w;
    (void)revent;

    TRACE();

    struct nm2_iface *piface;
    ds_dlist_iter_t iter;

    ds_dlist_foreach_iter(&nm2_iface_commit_list, piface, iter)
    {
        piface->if_commit = false;

        /* Apply the configuration */
        if (!inet_commit(piface->if_inet))
        {
            LOG(NOTICE, "nm2_iface: %s (%s): Error committing new configuration.",
                    piface->if_name,
                    nm2_iftype_tostr(piface->if_type));
        }

        nm2_inet_bridge_config_reapply(piface);

        ds_dlist_iremove(&iter);
    }
}

/* Schedule __nm2_iface_apply() via debouncing */
bool nm2_iface_apply(struct nm2_iface *piface)
{
    TRACE();

    static ev_debounce apply_timer;

    static bool apply_init = false;

    /* If already flagged, do not add to the list */
    if (piface->if_commit) return true;

    /* Flag the interface as pending for commit */
    piface->if_commit = true;
    ds_dlist_insert_tail(&nm2_iface_commit_list, piface);

    /* If it's not already initialized, initialize the commit timer */
    if (!apply_init)
    {
        ev_debounce_init(&apply_timer, __nm2_iface_apply, NM2_IFACE_APPLY_TIMER);
        apply_init = true;
    }

    /* Schedule the configuration apply */
    ev_debounce_start(EV_DEFAULT, &apply_timer);

    return true;
}

/*
 * Create a new interface of type @p type. This is primarily used as a dispatcher
 * for various inet_new_*() constructors.
 *
 * Additionally, it initializes some of the DHCP client options -- this is
 * true also for interfaces that do not actually use DHCP.
 *
 * TODO: Get rid of the target dependency (osp_unit_sku_get(), osp_unit_model_get() etc).
 */
inet_t *nm2_iface_new_inet(const char *ifname, enum nm2_iftype type)
{
    TRACE();

    char serial_num[100] = { 0 };
    char hostname[C_HOSTNAME_LEN] = { 0 };
    char vendor_class[OSN_DHCP_VENDORCLASS_MAX] = { 0 };

    inet_t *nif = NULL;

    switch (type)
    {
        case NM2_IFTYPE_ETH:
        case NM2_IFTYPE_BRIDGE:
        case NM2_IFTYPE_TUNNEL:
            nif = inet_eth_new(ifname);
            break;
        case NM2_IFTYPE_TAP:
            nif = inet_tap_new(ifname);
            break;

        case NM2_IFTYPE_VIF:
            nif = inet_vif_new(ifname);
            break;

        case NM2_IFTYPE_GRE:
        case NM2_IFTYPE_GRE6:
            nif = inet_gre_new(ifname);
            break;

        case NM2_IFTYPE_VLAN:
            nif = inet_vlan_new(ifname);
            break;

        case NM2_IFTYPE_PPPOE:
            nif = inet_pppoe_new(ifname);
            break;

        case NM2_IFTYPE_LTE:
            nif = inet_lte_new(ifname);
            break;

        case NM2_IFTYPE_UNMANAGED:
            nif = inet_unmanaged_new(ifname);
            break;

        default:
            /* Unsupported types */
            LOG(ERR, "nm2_iface: %s: Unsupported interface type: %d", ifname, type);
            return NULL;
    }

    if (nif == NULL)
    {
        LOG(ERR, "nm2_iface: %s: Error initializing interface type: %d", ifname, type);
        return NULL;
    }

    /*
     * Common initialization for all interfaces
     */

    /* Retrieve vendor class, sku, hostname ... we need these values to populate DHCP options */
    if (vendor_class[0] == '\0' && osp_unit_model_get(vendor_class, sizeof(vendor_class)) == false)
    {
        STRSCPY(vendor_class, TARGET_NAME);
    }

    if (serial_num[0] == '\0')
    {
        osp_unit_serial_get(serial_num, sizeof(serial_num));
    }

    if (hostname[0] == '\0')
    {
        if (osp_unit_dhcpc_hostname_get(hostname, sizeof(hostname)) != true)
        {
            /* In case hostname failed then use model name for hostname */
            tsnprintf(hostname, sizeof(hostname), "%s", vendor_class);
        }
    }

    /* Set DHCP options */
    inet_dhcpc_option_set(nif, DHCP_OPTION_VENDOR_CLASS, vendor_class);
    inet_dhcpc_option_set(nif, DHCP_OPTION_HOSTNAME, hostname);
    inet_dhcpc_option_set(nif, DHCP_OPTION_OSYNC_SWVER, app_build_number_get());
    inet_dhcpc_option_set(nif, DHCP_OPTION_OSYNC_PROFILE, app_build_profile_get());
    inet_dhcpc_option_set(nif, DHCP_OPTION_OSYNC_SERIAL_OPT, serial_num);

    return nif;
}

os_fdbuf_pcap_t *nm2_iface_new_fdbuf_pcap(const char *ifname, enum nm2_iftype type)
{
    switch (type) {
        case NM2_IFTYPE_BRIDGE:
            /* Only bridges are subject to Forward Database
             * Update Frames.
             */
            return os_fdbuf_pcap_new(ifname);
        case NM2_IFTYPE_NONE:
        case NM2_IFTYPE_ETH:
        case NM2_IFTYPE_VIF:
        case NM2_IFTYPE_VLAN:
        case NM2_IFTYPE_GRE:
        case NM2_IFTYPE_GRE6:
        case NM2_IFTYPE_TAP:
        case NM2_IFTYPE_PPPOE:
        case NM2_IFTYPE_LTE:
        case NM2_IFTYPE_TUNNEL:
        case NM2_IFTYPE_UNMANAGED:
        case NM2_IFTYPE_MAX:
            break;
    }
    return NULL;
}

/**
 * Re-registering to callbacks will cause the status to be synchronized.
 */
void nm2_iface_status_register(struct nm2_iface *piface)
{
    /* Register to inet_state */
    inet_state_notify(piface->if_inet, nm2_inet_state_fn);

    /* Register to IPv6 address updates registrations */
    inet_ip6_addr_status_notify(piface->if_inet, nm2_ip6_addr_status_fn);

    /*
     * Register to IPv6 status updates registrations, except for TAP interfaces
     * as those typically generate duplicate entries (the same entries are present
     * on physical interfaces)
     */
    if (piface->if_type != NM2_IFTYPE_TAP)
    {
        inet_ip6_neigh_status_notify(piface->if_inet, nm2_ip6_neigh_status_fn);
    }

    /* Register DHCP client option registrations */
    inet_dhcpc_option_notify(piface->if_inet, nm2_iface_dhcpc_notify);

    /* Register the route state function */
    inet_route_notify(piface->if_inet, nm2_route_notify);
    inet_route6_notify(piface->if_inet, nm2_route6_notify);
}

/*
 * ===========================================================================
 * DHCP client options notifications
 * ===========================================================================
 */
void nm2_iface_dhcpc_notify(inet_t *inet, enum osn_dhcp_option opt, const char *value)
{
    struct nm2_iface *piface = inet->in_data;

    (void)opt;
    (void)value;

    ev_debounce_start(EV_DEFAULT, &piface->if_opt_debounce);
}

/*
 * Interface comparator
 */
int nm2_iface_cmp(const void *_a, const void  *_b)
{
    const struct nm2_iface *a = _a;
    const struct nm2_iface *b = _b;

    return strcmp(a->if_name, b->if_name);
}
