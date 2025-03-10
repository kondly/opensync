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

#ifndef INET_H_INCLUDED
#define INET_H_INCLUDED

#include <stdlib.h>

#include "os_types.h"
#include "memutil.h"

#include "osn_inet.h"
#include "osn_dhcpv6.h"
#include "osn_dhcp.h"
#include "osn_upnp.h"
#include "osn_igmp.h"
#include "osn_mld.h"


/*
 * ===========================================================================
 *  inet_t main interface
 * ===========================================================================
 */

enum inet_assign_scheme
{
    INET_ASSIGN_INVALID,
    INET_ASSIGN_NONE,
    INET_ASSIGN_STATIC,
    INET_ASSIGN_DHCP
};

enum inet_proto
{
    INET_PROTO_UDP,
    INET_PROTO_TCP
};

/*
 * ===========================================================================
 *  Inet class definition
 * ===========================================================================
 */
typedef struct __inet_state inet_state_t;

struct __inet_state
{
    bool                    in_interface_enabled;
    bool                    in_network_enabled;
    enum inet_assign_scheme in_assign_scheme;
    int                     in_mtu;
    bool                    in_nat_enabled;
    enum osn_upnp_mode      in_upnp_mode;
    bool                    in_dhcps_enabled;
    bool                    in_dhcpc_enabled;
    bool                    in_interface_exists;
    bool                    in_port_status;
    osn_ip_addr_t           in_ipaddr;
    osn_ip_addr_t           in_netmask;
    osn_ip_addr_t           in_bcaddr;
    osn_ip_addr_t           in_gateway;
    osn_mac_addr_t          in_macaddr;
    int                     in_speed;
    osn_duplex_t            in_duplex;
    osn_ip_addr_t           in_dns1;
    osn_ip_addr_t           in_dns2;
};

#define INET_STATE_INIT (inet_state_t)  \
{                                       \
    .in_ipaddr = OSN_IP_ADDR_INIT,      \
    .in_netmask = OSN_IP_ADDR_INIT,     \
    .in_bcaddr = OSN_IP_ADDR_INIT,      \
    .in_gateway = OSN_IP_ADDR_INIT,     \
    .in_macaddr = OSN_MAC_ADDR_INIT,    \
    .in_speed = OSN_NETIF_SPEED_INIT,   \
    .in_duplex = OSN_NETIF_DUPLEX_INIT, \
    .in_dns1 = OSN_IP_ADDR_INIT,        \
    .in_dns2 = OSN_IP_ADDR_INIT,        \
}

typedef struct __inet inet_t;

/* Port forwarding argument structure */
struct inet_portforward
{
    uint16_t         pf_dst_port;
    uint16_t         pf_src_port;
    enum inet_proto  pf_proto;
    osn_ip_addr_t    pf_dst_ipaddr;
    bool             pf_nat_loopback;
};

#define INET_PORTFORWARD_INIT (struct inet_portforward) \
{                                                       \
    .pf_dst_ipaddr = OSN_IP_ADDR_INIT,                  \
    .pf_nat_loopback = true,                            \
}

enum inet_ip6_addr_origin
{
    INET_IP6_ORIGIN_STATIC,
    INET_IP6_ORIGIN_DHCP,
    INET_IP6_ORIGIN_AUTO_CONFIGURED,
    INET_IP6_ORIGIN_ERROR,
};

/*
 * Interface IPv6 address status
 */
struct inet_ip6_addr_status
{
    osn_ip6_addr_t              is_addr;
    enum inet_ip6_addr_origin   is_origin;
};

/*
 * Neighbor report
 */
struct inet_ip6_neigh_status
{
    osn_ip6_addr_t              in_ipaddr;
    osn_mac_addr_t              in_hwaddr;
};

typedef void inet_state_fn_t(
        inet_t *self,
        const inet_state_t *state);

typedef void inet_route_status_fn_t(
        inet_t *self,
        struct osn_route_status *status,
        bool remove);

typedef void inet_route6_status_fn_t(
        inet_t *self,
        struct osn_route6_status *status,
        bool remove);

/* Lease notify callback */
typedef bool inet_dhcp_lease_fn_t(
        void *data,
        bool released,
        struct osn_dhcp_server_lease *dl);

/* IPv4 neighbor notification */
typedef void inet_dhcpc_option_notify_fn_t(
        inet_t *self,
        enum osn_dhcp_option opt,
        const char *value);

/* IPv6 status change notification */
typedef void inet_ip6_addr_status_fn_t(
        inet_t *self,
        struct inet_ip6_addr_status *status,
        bool remove);

/* IPv6 neighbor notification */
typedef void inet_ip6_neigh_status_fn_t(
        inet_t *self,
        struct inet_ip6_neigh_status *status,
        bool remove);

/* DHCPv6 Client status change notification */
typedef void inet_dhcp6_client_notify_fn_t(
        inet_t *self,
        struct osn_dhcpv6_client_status *status);

/* DHCPv6 Server status change notification */
typedef void inet_dhcp6_server_notify_fn_t(
        inet_t *self,
        struct osn_dhcpv6_server_status *status);

/* Pretty printing for inet_portforward */
#define PRI_inet_portforward \
        "[%s]@0.0.0.0:%d->"PRI_osn_ip_addr":%d"

#define FMT_inet_portforward(x) \
        (x).pf_proto == INET_PROTO_UDP ? "udp" : "tcp", \
        (x).pf_src_port, \
        FMT_osn_ip_addr((x).pf_dst_ipaddr), \
        (x).pf_dst_port

struct __inet
{
    /* Interface name */
    char        in_ifname[C_IFNAME_LEN];

    /* Used to store private data */
    void       *in_data;

    /* Destructor function */
    bool        (*in_dtor_fn)(inet_t *self);

    /* Enable/disable interface */
    bool        (*in_interface_enable_fn)(inet_t *self, bool enable);

    /* Set interface network  */
    bool        (*in_network_enable_fn)(inet_t *self, bool enable);

    /* Set MTU */
    bool        (*in_mtu_set_fn)(inet_t *self, int mtu);

    /* Set OVS no-flood */
    bool        (*in_noflood_set_fn)(inet_t *self, bool enable);

    /* Set parent interface */
    bool        (*in_parent_ifname_set_fn)(inet_t *self, const char *parent_ifname);

    /* Set credentials for interface. PPP or VPN links need this */
    bool        (*in_credential_set_fn)(inet_t *self, const char *username, const char *password);
    /**
     * Set IP assignment scheme:
     *   - INET_ASSIGN_NONE     - Interface does not have any IPv4 configuration
     *   - INET_ASSIGN_STATIC   - Use a static IP/Netmask/Broadcast address
     *   - INET_ASSIGN_DHCP     - Use dynamic IP address assignment
     */
    bool        (*in_assign_scheme_set_fn)(inet_t *self, enum inet_assign_scheme scheme);

    /* Renew DHCP */
    bool        (*in_dhcp_renew_set_fn)(inet_t *self, uint32_t dhcp_renew);

    /*
     * Set IP Address/Netmask/Broadcast/Gateway -- only when assign_scheme == INET_ASSIGN_STATIC
     *
     * bcast and gateway can be INET_ADDR_NONE
     */
    bool        (*in_ipaddr_static_set_fn)(
                        inet_t *self,
                        osn_ip_addr_t addr,
                        osn_ip_addr_t netmask,
                        osn_ip_addr_t bcast);

    /* Set the interface default gateway, typically set when assign_scheme == INET_ASSIGN_STATIC */
    bool        (*in_gateway_set_fn)(inet_t *self, osn_ip_addr_t  gwaddr);

    /* Enable NAT */
    bool        (*in_nat_enable_fn)(inet_t *self, bool enable);

    /* Multicast */
    bool        (*in_igmp_update_ref_fn)(inet_t *self, bool increase);
    bool        (*in_mld_update_ref_fn)(inet_t *self, bool increase);

    bool        (*in_upnp_mode_set_fn)(inet_t *self, enum osn_upnp_mode mode);

    /* Port forwarding */
    bool        (*in_portforward_set_fn)(inet_t *self, const struct inet_portforward *pf);
    bool        (*in_portforward_del_fn)(inet_t *self, const struct inet_portforward *pf);

    /* Set primary/secondary DNS servers */
    bool        (*in_dns_set_fn)(inet_t *self, osn_ip_addr_t primary, osn_ip_addr_t secondary);

    /* DHCP client options */
    bool        (*in_dhcpc_option_request_fn)(inet_t *self, enum osn_dhcp_option opt, bool req);
    bool        (*in_dhcpc_option_set_fn)(inet_t *self, enum osn_dhcp_option opt, const char *value);
    bool        (*in_dhcpc_option_get_fn)(inet_t *self, enum osn_dhcp_option opt, bool *request, const char **value);
    bool        (*in_dhcpc_option_notify_fn)(inet_t *self, inet_dhcpc_option_notify_fn_t *fn);

    /* True if DHCP server should be enabled on this interface */
    bool        (*in_dhcps_enable_fn)(inet_t *self, bool enabled);

    /* DHCP server otpions */
    bool        (*in_dhcps_lease_set_fn)(inet_t *self, int lease_time_s);
    bool        (*in_dhcps_range_set_fn)(inet_t *self, osn_ip_addr_t start, osn_ip_addr_t stop);
    bool        (*in_dhcps_option_set_fn)(inet_t *self, enum osn_dhcp_option opt, const char *value);
    bool        (*in_dhcps_lease_notify_fn)(inet_t *self, inet_dhcp_lease_fn_t *fn);
    bool        (*in_dhcps_rip_set_fn)(inet_t *super,
                        osn_mac_addr_t macaddr,
                        osn_ip_addr_t ip4addr,
                        const char *hostname);
    bool        (*in_dhcps_rip_del_fn)(inet_t *super, osn_mac_addr_t macaddr);


    /* IPv4 tunnels (GRE, Softwds) */
    bool        (*in_ip4tunnel_set_fn)(inet_t *self,
                        const char *parent,
                        osn_ip_addr_t local,
                        osn_ip_addr_t remote,
                        osn_mac_addr_t macaddr);

    /* IPv6 tunnels (GRE, softwds) */
    bool        (*in_ip6tunnel_set_fn)(inet_t *self,
                        const char *parent,
                        osn_ip6_addr_t local,
                        osn_ip6_addr_t remote,
                        osn_mac_addr_t macaddr);

    /* VLANs */
    bool       (*in_vlanid_set_fn)(inet_t *self, int vlanid);
    bool       (*in_vlan_egress_qos_map_set_fn)(inet_t *self, const char *qos_map);

    /* DHCP sniffing - register callback for DHCP sniffing - if set to NULL sniffing is disabled */
    bool        (*in_dhsnif_lease_notify_fn)(inet_t *self, inet_dhcp_lease_fn_t *func);

    /* Routing table methods  -- if set to NULL route state reporting is disabled */
    bool        (*in_route_notify_fn)(inet_t *self, inet_route_status_fn_t *func);
    bool (*in_route4_add_fn)(inet_t *super, const osn_route4_t *route);
    bool (*in_route4_remove_fn)(inet_t *super, const osn_route4_t *route);

    /* Routing IPv6 table methods  -- if set to NULL route state reporting is disabled */
    bool        (*in_route6_notify_fn)(inet_t *self, inet_route6_status_fn_t *func);
    bool        (*in_route6_add_fn)(inet_t *super, const osn_route6_config_t *route);
    bool        (*in_route6_remove_fn)(inet_t *super, const osn_route6_config_t *route);

    /* Commit all pending changes */
    bool        (*in_commit_fn)(inet_t *self);

    /* Interface status change notification registration method */
    bool        (*in_state_notify_fn)(inet_t *self, inet_state_fn_t *fn);

    /* create Bridge Port */
    bool (*in_bridge_port_set_fn)(inet_t *self, inet_t *port_inet, bool add);

    /* create Bridge */
    bool (*in_bridge_set_fn)(inet_t *self, const char *brname, bool add);

    /*
     * ===========================================================================
     *  IPv6 support
     * ===========================================================================
     */

    /* Static IPv6 configuration: Add/remove IP addresess */
    bool        (*in_ip6_addr_fn)(
                        inet_t *self,
                        bool add,
                        osn_ip6_addr_t *addr);

    /* Static IPv6 configuration: Add/remove DNS servers */
    bool        (*in_ip6_dns_fn)(
                        inet_t *self,
                        bool add,
                        osn_ip6_addr_t *addr);

    bool        (*in_ip6_addr_status_notify_fn)(
                        inet_t *self,
                        inet_ip6_addr_status_fn_t *fn);

    bool        (*in_ip6_neigh_status_notify_fn)(
                        inet_t *self,
                        inet_ip6_neigh_status_fn_t *fn);

    /* DHCPv6 client configuration options */
    bool        (*in_dhcp6_client_fn)(
                        inet_t *self,
                        bool enable,
                        bool request_addr,
                        bool request_prefix,
                        bool rapid_commit);

    /* DHCPv6 options that shall be requested from remote */
    bool        (*in_dhcp6_client_option_request_fn)(inet_t *self, int tag, bool request);

    /* DHCPv6 options that will be sent to the server */
    bool        (*in_dhcp6_client_option_send_fn)(inet_t *self, int tag, char *value);

    /* DHCPv6 client other_config */
    bool        (*in_dhcp6_client_other_config_fn)(inet_t *self, char *key, char *value);


    /* DHCPv6 client status notification */
    bool        (*in_dhcp6_client_notify_fn)(inet_t *self, inet_dhcp6_client_notify_fn_t *fn);

    /* DHCPv6 Server options */
    bool        (*in_dhcp6_server_fn)(inet_t *self, bool enable);

    /* DHCPv6 Server prefixes */
    bool        (*in_dhcp6_server_prefix_fn)(
                        inet_t *self,
                        bool add,
                        struct osn_dhcpv6_server_prefix *prefix);

    /* DHCPv6 options that will be sent to the client */
    bool        (*in_dhcp6_server_option_send_fn)(inet_t *self, int tag, char *value);

    /* DHCPv6 Server static leases */
    bool        (*in_dhcp6_server_lease_fn)(
                        inet_t *self,
                        bool add,
                        struct osn_dhcpv6_server_lease *lease);

    /* DHCPv6 server status notification */
    bool        (*in_dhcp6_server_notify_fn)(inet_t *self, inet_dhcp6_server_notify_fn_t *fn);

    /* Router Advertisement: Set options */
    bool        (*in_radv_fn)(
                        inet_t *self,
                        bool enable,
                        struct osn_ip6_radv_options *opts);

    /* Router Advertisement: Add or remove */
    bool        (*in_radv_prefix_fn)(
                        inet_t *self,
                        bool add,
                        osn_ip6_addr_t *prefix,
                        bool autonomous,
                        bool onlink);

    /* Router Advertisement: Add or remove RDNSS entries */
    bool        (*in_radv_rdnss_fn)(
                        inet_t *self,
                        bool add,
                        osn_ip6_addr_t *dns);

    /* Router Advertisement: Add or remove DNSSL entries */
    bool        (*in_radv_dnssl_fn)(
                        inet_t *self,
                        bool add,
                        char *sl);

};

/*
 * ===========================================================================
 *  Inet Class Interfaces
 * ===========================================================================
 */

/**
 * Static destructor, counterpart to _init()
 */
static inline bool inet_fini(inet_t *self)
{
    if (self->in_dtor_fn == NULL) return false;

    return self->in_dtor_fn(self);
}

/**
 * Dynamic destructor, counterpart to _new()
 */
static inline bool inet_del(inet_t *self)
{
    bool retval = inet_fini(self);

    FREE(self);

    return retval;
}

static inline bool inet_interface_enable(inet_t *self, bool enable)
{
    if (self->in_interface_enable_fn == NULL) return false;

    return self->in_interface_enable_fn(self, enable);
}

static inline bool inet_network_enable(inet_t *self, bool enable)
{
    if (self->in_network_enable_fn == NULL) return false;

    return self->in_network_enable_fn(self, enable);
}

static inline bool inet_mtu_set(inet_t *self, int mtu)
{
    if (self->in_mtu_set_fn == NULL) return false;

    return self->in_mtu_set_fn(self, mtu);
}

static inline bool inet_noflood_set(inet_t *self, bool enable)
{
    if (self->in_noflood_set_fn == NULL) return false;

    return self->in_noflood_set_fn(self, enable);
}

static inline bool inet_parent_ifname_set(inet_t *self, const char *parent_ifname)
{
    /*
     * If the parent_ifname_set method is not set and the parent interface is
     * being cleared (parent_ifname == NULL), it is valid to return success
     * (true).
     *
     * This also ensures backward compatibility.
     */
    if (self->in_parent_ifname_set_fn == NULL)
    {
        return (parent_ifname == NULL ? true : false);
    }

    return self->in_parent_ifname_set_fn(self, parent_ifname);
}

static inline bool inet_credential_set(
        inet_t *self,
        const char *username,
        const char *password)
{
    if (self->in_credential_set_fn == NULL)
    {
        /*
         * If the no in_credential_set method is defined, it is valid to unset
         * the username as it was not possible to set it in the first place.
         */

        return (username == NULL ? true : false);
    }

    return self->in_credential_set_fn(self, username, password);
}

static inline bool inet_br_port_set(inet_t *self, inet_t* port_inet, bool add)
{
    if (self->in_bridge_port_set_fn == NULL) return false;

    return self->in_bridge_port_set_fn(self, port_inet, add);
}

static inline bool inet_br_set(inet_t *self, const char *brname, bool add)
{
    if (self->in_bridge_set_fn == NULL) return false;

    return self->in_bridge_set_fn(self, brname, add);
}

static inline bool inet_assign_scheme_set(inet_t *self, enum inet_assign_scheme scheme)
{
    if (self->in_assign_scheme_set_fn == NULL) return false;

    return self->in_assign_scheme_set_fn(self, scheme);
}

static inline bool inet_dhcp_renew_set(inet_t *self, uint32_t dhcp_renew)
{
    if (self->in_dhcp_renew_set_fn == NULL) return false;

    return self->in_dhcp_renew_set_fn(self, dhcp_renew);
}

static inline bool inet_ipaddr_static_set(
        inet_t *self,
        osn_ip_addr_t ipaddr,
        osn_ip_addr_t netmask,
        osn_ip_addr_t  bcaddr)
{
    if (self->in_ipaddr_static_set_fn == NULL) return false;

    return self->in_ipaddr_static_set_fn(
            self,
            ipaddr,
            netmask,
            bcaddr);
}

static inline bool inet_gateway_set(inet_t *self, osn_ip_addr_t gwaddr)
{
    if (self->in_gateway_set_fn == NULL) return false;

    return self->in_gateway_set_fn(self, gwaddr);
}

static inline bool inet_nat_enable(inet_t *self, bool enable)
{
    if (self->in_nat_enable_fn == NULL) return false;

    return self->in_nat_enable_fn(self, enable);
}

static inline bool inet_igmp_update_ref(inet_t *self, bool increase)
{
    if (self->in_igmp_update_ref_fn == NULL) return false;

    return self->in_igmp_update_ref_fn(self, increase);
}

static inline bool inet_mld_update_ref(inet_t *self, bool increase)
{
    if (self->in_mld_update_ref_fn == NULL) return false;

    return self->in_mld_update_ref_fn(self, increase);
}

static inline bool inet_upnp_mode_set(inet_t *self, enum osn_upnp_mode mode)
{
    if (self->in_upnp_mode_set_fn == NULL) return false;

    return self->in_upnp_mode_set_fn(self, mode);
}


static inline bool inet_portforward_set(inet_t *self, const struct inet_portforward *pf)
{
    if (self->in_portforward_set_fn == NULL) return false;

    return self->in_portforward_set_fn(self, pf);
}

static inline bool inet_portforward_del(inet_t *self, const struct inet_portforward *pf)
{
    if (self->in_portforward_del_fn == NULL) return false;

    return self->in_portforward_del_fn(self, pf);
}

static inline bool inet_dns_set(inet_t *self, osn_ip_addr_t primary, osn_ip_addr_t secondary)
{
    if (self->in_dns_set_fn == NULL) return false;

    return self->in_dns_set_fn(self, primary, secondary);
}

static inline bool inet_dhcpc_option_request(inet_t *self, enum osn_dhcp_option opt, bool req)
{
    if (self->in_dhcpc_option_request_fn == NULL) return false;

    return self->in_dhcpc_option_request_fn(self, opt, req);
}

static inline bool inet_dhcpc_option_set(inet_t *self, enum osn_dhcp_option opt, const char *value)
{
    if (self->in_dhcpc_option_set_fn == NULL) return false;

    return self->in_dhcpc_option_set_fn(self, opt, value);
}

static inline bool inet_dhcpc_option_get(inet_t *self, enum osn_dhcp_option opt, bool *request, const char **value)
{
    return (NULL != self->in_dhcpc_option_get_fn) ? self->in_dhcpc_option_get_fn(self, opt, request, value) : false;
}

static inline bool inet_dhcpc_option_notify(inet_t *self, inet_dhcpc_option_notify_fn_t *fn)
{
    if (self->in_dhcpc_option_notify_fn == NULL) return false;

    return self->in_dhcpc_option_notify_fn(self, fn);
}

static inline bool inet_dhcps_enable(inet_t *self, bool enabled)
{
    if (self->in_dhcps_enable_fn == NULL) return false;

    return self->in_dhcps_enable_fn(self, enabled);
}

static inline bool inet_dhcps_lease_set(inet_t *self, int lease_time_s)
{
    if (self->in_dhcps_lease_set_fn == NULL) return false;

    return self->in_dhcps_lease_set_fn(self, lease_time_s);
}

static inline bool inet_dhcps_range_set(inet_t *self, osn_ip_addr_t start, osn_ip_addr_t stop)
{
    if (self->in_dhcps_range_set_fn == NULL) return false;

    return self->in_dhcps_range_set_fn(self, start, stop);
}

static inline bool inet_dhcps_option_set(inet_t *self, enum osn_dhcp_option opt, const char *value)
{
    if (self->in_dhcps_option_set_fn == NULL) return false;

    return self->in_dhcps_option_set_fn(self, opt, value);
}

static inline bool inet_dhcps_lease_notify(inet_t *self, inet_dhcp_lease_fn_t *fn)
{
    if (self->in_dhcps_lease_notify_fn == NULL) return false;

    return self->in_dhcps_lease_notify_fn(self, fn);
}

static inline bool inet_dhcps_rip_set(inet_t *self, osn_mac_addr_t macaddr,
                                      osn_ip_addr_t ip4addr, const char *hostname)
{
    if (self->in_dhcps_rip_set_fn == NULL) return false;

    return self->in_dhcps_rip_set_fn(self, macaddr, ip4addr, hostname);
}

static inline bool inet_dhcps_rip_del(inet_t *self, osn_mac_addr_t macaddr)
{
    if (self->in_dhcps_rip_del_fn == NULL) return false;

    return self->in_dhcps_rip_del_fn(self, macaddr);
}


/**
 * Set IPv4 tunnel options
 *
 * parent   - parent interface
 * laddr    - local IP address
 * raddr    - remote IP address
 * rmac     - remote MAC address, this field is ignored for some protocols such as GRETAP
 */
static inline bool inet_ip4tunnel_set(
        inet_t *self,
        const char *parent,
        osn_ip_addr_t laddr,
        osn_ip_addr_t raddr,
        osn_mac_addr_t rmac)
{
    if (self->in_ip4tunnel_set_fn == NULL) return false;

    return self->in_ip4tunnel_set_fn(self, parent, laddr, raddr, rmac);
}

/**
 * Set IPv6 tunnel options
 *
 * parent   - parent interface
 * laddr    - local IP address
 * raddr    - remote IP address
 * rmac     - remote MAC address, this field is ignored for some protocols such as GRETAP
 */
static inline bool inet_ip6tunnel_set(
        inet_t *self,
        const char *parent,
        osn_ip6_addr_t laddr,
        osn_ip6_addr_t raddr,
        osn_mac_addr_t rmac)
{
    if (self->in_ip6tunnel_set_fn == NULL) return false;

    return self->in_ip6tunnel_set_fn(self, parent, laddr, raddr, rmac);
}

/*
 * VLAN settings
 *
 * vlanid       - the VLAN id
 */
static inline bool inet_vlanid_set(
        inet_t *self,
        int vlanid)
{
    if (self->in_vlanid_set_fn == NULL) return false;

    return self->in_vlanid_set_fn(self, vlanid);
}

/**
 * @brief Setting of VLAN egress QOS mapping i.e. linux packet priority to PCP field
 * 
 * @param self ptr to inet object
 * @param qos_map new egress qos mapping in FROM:TO pri pairs separated by spaces
 * @return true when setting accepted, false; when rejected or not supported
 */
static inline bool inet_vlan_egress_qos_map_set(inet_t *self, const char *qos_map)
{
    return (self->in_vlan_egress_qos_map_set_fn == NULL) ? false : self->in_vlan_egress_qos_map_set_fn(self, qos_map);
}

/*
 * DHCP sniffing - @p func will be called each time a DHCP packet is sniffed
 * on the interface. This can happen multiple times for the same client
 * (depending on the DHCP negotiation phase, more data can be made available).
 *
 * If func is NULL, DHCP sniffing is disabled on the interface.
 */
static inline bool inet_dhsnif_lease_notify(inet_t *self, inet_dhcp_lease_fn_t *func)
{
    if (self->in_dhsnif_lease_notify_fn == NULL) return false;

    return self->in_dhsnif_lease_notify_fn(self, func);
}

/*
 * Subscribe to route table state changes.
 */
static inline bool inet_route_notify(inet_t *self, inet_route_status_fn_t *func)
{
    if (self->in_route_notify_fn == NULL) return false;

    return self->in_route_notify_fn(self, func);
}

static inline bool inet_route4_add(inet_t *self, const osn_route4_t *route)
{
    return self->in_route4_add_fn ? self->in_route4_add_fn(self, route) : false;
}

static inline bool inet_route4_remove(inet_t *self, const osn_route4_t *route)
{
    return self->in_route4_remove_fn ? self->in_route4_remove_fn(self, route) : false;
}

/*
 * Subscribe to route6 table state changes.
 */
static inline bool inet_route6_notify(inet_t *self, inet_route6_status_fn_t *func)
{
    if (self->in_route6_notify_fn == NULL) return false;

    return self->in_route6_notify_fn(self, func);
}

static inline bool inet_route6_add(inet_t *self, const osn_route6_config_t *route)
{
    return self->in_route6_add_fn ? self->in_route6_add_fn(self, route) : false;
}

static inline bool inet_route6_remove(inet_t *self, const osn_route6_config_t *route)
{
    return self->in_route6_remove_fn ? self->in_route6_remove_fn(self, route) : false;
}

/**
 * Commit all pending changes; the purpose of this function is to figure out the order
 * in which subsystems must be brought up or tore down and call inet_apply() for each
 * one of them
 */
static inline bool inet_commit(inet_t *self)
{
    if (self->in_commit_fn == NULL) return false;

    return self->in_commit_fn(self);
}

/*
 * ===========================================================================
 *  Status reporting
 * ===========================================================================
 */

/* Asynchronous interface state change notification method */
static inline bool inet_state_notify(inet_t *self, inet_state_fn_t *fn)
{
    if (self->in_state_notify_fn == NULL) return false;

    return self->in_state_notify_fn(self, fn);
}

/*
 * ===========================================================================
 *  IPv6 support
 * ===========================================================================
 */
/* Static IPv6 configuration: Add/remove IP addresess */
static inline bool inet_ip6_addr(inet_t *self, bool add, osn_ip6_addr_t *addr)
{
    if (self->in_ip6_addr_fn == NULL) return false;

    return self->in_ip6_addr_fn(self, add, addr);
}

/* Static IPv6 configuration: Add/remove DNS servers */
static inline bool inet_ip6_dns(inet_t *self, bool add, osn_ip6_addr_t *dns)
{
    if (self->in_ip6_dns_fn == NULL) return false;

    return self->in_ip6_dns_fn(self, add, dns);
}

/*
 * IPv6 status change notification callback
 */
static inline bool inet_ip6_addr_status_notify(
        inet_t *self,
        inet_ip6_addr_status_fn_t *fn)
{
    if (self->in_ip6_addr_status_notify_fn == NULL) return false;

    return self->in_ip6_addr_status_notify_fn(self, fn);
}

/*
 * IPv6 neighbor status change notification callback
 */
static inline bool inet_ip6_neigh_status_notify(
        inet_t *self,
        inet_ip6_neigh_status_fn_t *fn)
{
    if (self->in_ip6_neigh_status_notify_fn == NULL) return false;

    return self->in_ip6_neigh_status_notify_fn(self, fn);
}

/* DHCPv6 configuration options */
static inline bool inet_dhcp6_client(
        inet_t *self,
        bool enable,
        bool request_addr,
        bool request_prefix,
        bool rapid_commit)
{
    if (self->in_dhcp6_client_fn == NULL) return false;

    return self->in_dhcp6_client_fn(
            self,
            enable,
            request_addr,
            request_prefix,
            rapid_commit);
}

/* DHCPv6 options that shall be requested from remote */
static inline bool inet_dhcp6_client_option_request(inet_t *self, int tag, bool request)
{
    if (self->in_dhcp6_client_option_request_fn == NULL) return false;

    return self->in_dhcp6_client_option_request_fn(self, tag, request);
}

/* DHCPv6 options that will be sent to the server */
static inline bool inet_dhcp6_client_option_send(inet_t *self, int tag, char *value)
{
    if (self->in_dhcp6_client_option_send_fn == NULL) return false;

    return self->in_dhcp6_client_option_send_fn(self, tag, value);
}

static inline bool inet_dhcp6_client_other_config(inet_t *self, char *key, char *value)
{
    if (self->in_dhcp6_client_other_config_fn == NULL) return false;

    return self->in_dhcp6_client_other_config_fn(self, key, value);
}

static inline bool inet_dhcp6_client_notify(inet_t *self, inet_dhcp6_client_notify_fn_t *fn)
{
    if (self->in_dhcp6_client_notify_fn == NULL) return false;

    return self->in_dhcp6_client_notify_fn(self, fn);
}

/* DHCPv6 Server: Options that will be sent to the server */
static inline bool inet_dhcp6_server(inet_t *self, bool enable)
{
    if (self->in_dhcp6_server_fn == NULL) return false;

    return self->in_dhcp6_server_fn(self, enable);
}

/* DHCPv6 Server: Add an IPv6 range to the server */
static inline bool inet_dhcp6_server_prefix(inet_t *self, bool add, struct osn_dhcpv6_server_prefix *prefix)
{
    if (self->in_dhcp6_server_prefix_fn == NULL) return false;

    return self->in_dhcp6_server_prefix_fn(self, add, prefix);
}

/* DHCPv6 options that will be sent to the client */
static inline bool inet_dhcp6_server_option_send(inet_t *self, int tag, char *value)
{
    if (self->in_dhcp6_server_option_send_fn == NULL) return false;

    return self->in_dhcp6_server_option_send_fn(self, tag, value);
}


/* DHCPv6 Server: Options that will be sent to the server */
static inline bool inet_dhcp6_server_lease(inet_t *self, bool add, struct osn_dhcpv6_server_lease *lease)
{
    if (self->in_dhcp6_server_lease_fn == NULL) return false;

    return self->in_dhcp6_server_lease_fn(self, add, lease);
}

static inline bool inet_dhcp6_server_notify(inet_t *self, inet_dhcp6_server_notify_fn_t *fn)
{
    if (self->in_dhcp6_server_notify_fn == NULL) return false;

    return self->in_dhcp6_server_notify_fn(self, fn);
}

/* Router Advertisement: Push new options for this interface */
static inline bool inet_radv(inet_t *self, bool enable, struct osn_ip6_radv_options *opts)
{
    if (self->in_radv_fn == NULL) return false;

    return self->in_radv_fn(self, enable, opts);
}

/* Router Advertisement: Add or remove an advertised prefix */
static inline bool inet_radv_prefix(
        inet_t *self,
        bool add,
        osn_ip6_addr_t *prefix,
        bool autonomous,
        bool onlink)
{
    if (self->in_radv_prefix_fn == NULL) return false;

    return self->in_radv_prefix_fn(self, add, prefix, autonomous, onlink);
}

static inline bool inet_radv_rdnss(inet_t *self, bool add, osn_ip6_addr_t  *addr)
{
    if (self->in_radv_rdnss_fn == NULL) return false;

    return self->in_radv_rdnss_fn(self, add, addr);
}

static inline bool inet_radv_dnssl(inet_t *self, bool add, char *sl)
{
    if (self->in_radv_dnssl_fn == NULL) return false;

    return self->in_radv_dnssl_fn(self, add, sl);
}

bool inet_igmp_set_config(struct osn_igmp_snooping_config *snooping_config,
                          struct osn_igmp_proxy_config *proxy_config,
                          struct osn_igmp_querier_config *querier_config,
                          struct osn_mcast_other_config *other_config);
bool inet_mld_set_config(struct osn_mld_snooping_config *snooping_config,
                         struct osn_mld_proxy_config *proxy_config,
                         struct osn_mld_querier_config *querier_config,
                         struct osn_mcast_other_config *other_config);

#endif /* INET_H_INCLUDED */
