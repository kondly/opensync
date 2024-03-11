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
 * DNSMASQ library -- API for handling IPv4 DNSMASQ instances and their
 * configuration
 * ===========================================================================
 */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>

#include "log.h"
#include "evx.h"
#include "util.h"
#include "memutil.h"
#include "daemon.h"
#include "os_file.h"
#include "os_regex.h"
#include "kconfig.h"

#include "dnsmasq_server.h"

/*
 * ===========================================================================
 * DHCPv4 Server
 * ===========================================================================
 */
/* Debounce timer, the maximum amount time to wait before doing a full dnsmasq service restart */
#define DNSMASQ_SERVER_DEBOUNCE_TIMER      0.3

/* Number of elements to add to the lease array each time it is resized */
#define DNSMASQ_LEASE_RE_GROW              16

static char dnsmasq_server_conf_header[] =
    "#\n"
    "# Auto-generated by OpenSync\n"
    "#\n"
    "\n"
    "### Global section\n"
    "\n"
    "dhcp-authoritative\n"
    "domain-needed\n"
    "localise-queries\n"
    "read-ethers\n"
    "bogus-priv\n"
    "expand-hosts\n"
    "domain=lan\n"
    "server=/lan/\n"
    "dhcp-leasefile="CONFIG_OSN_DNSMASQ_LEASE_PATH"\n"
    "resolv-file="CONFIG_OSN_DNSMASQ_RESOLV_CONF_PATH"\n"
    "no-dhcp-interface=br-wan,eth0,eth1\n"
    "address=/osync.lan/192.168.1.1\n"
    "ptr-record=1.1.168.192.in-addr.arpa,osync.lan\n"
    //>>SIT-18: VU#598349
    "#dhcp-name-match=set:wpad-ignore,wpad\n"
    "#dhcp-ignore-names=tag:wpad-ignore\n"
    //<<SIT-18
    "# Disable the default gateway and DNS server options.\n"
    "# Cloud needs to set these explicitly.\n"
    "dhcp-option=3\n"
    "dhcp-option=6\n"
    "\n"
    "### Interface section\n"
    "\n";

/*
 * IP range structure
 */
struct dnsmasq_range
{
    osn_ip_addr_t                   dr_range_start;
    osn_ip_addr_t                   dr_range_stop;
    ds_tree_node_t                  dr_tnode;
};

/* IP range comparison function */
int dnsmasq_range_cmp(const void *_a, const void *_b)
{
    int rc;

    const struct dnsmasq_range *a = _a;
    const struct dnsmasq_range *b = _b;

    rc = osn_ip_addr_cmp(&a->dr_range_start, &b->dr_range_start);
    if (rc != 0) return rc;

    rc = osn_ip_addr_cmp(&a->dr_range_stop, &b->dr_range_stop);
    if (rc != 0) return rc;

    return 0;
}

/*
 * IP reservation structure
 */
struct dnsmasq_reservation
{
    char                           *ds_hostname;
    ds_tree_node_t                  ds_tnode;
    osn_mac_addr_t                  ds_macaddr;
    osn_ip_addr_t                   ds_ip4addr;
};

#define DNSMASQ_RESERVATION_INIT (struct dnsmasq_reservation)   \
{                                                               \
    .ds_macaddr = OSN_MAC_ADDR_INIT,                            \
    .ds_ip4addr = OSN_IP_ADDR_INIT,                             \
}

/*
 * Static functions
 */
static void __dnsmasq_server_apply(struct ev_loop *loop, struct ev_debounce *ev, int revent);
static bool dnsmasq_server_atexit(daemon_t *proc);
static bool dnsmasq_server_config_write(void);
static void dnsmasq_server_dispatch_error(void);
static void dnsmasq_lease_onchange(struct ev_loop *loop, ev_stat *w, int revent);

/*
 * Globals
 */
static ds_dlist_t   dnsmasq_server_list = DS_DLIST_INIT(dnsmasq_server_t, ds_dnode);
static daemon_t     dnsmasq_server_daemon;
static ev_debounce  dnsmasq_server_debounce;
static ev_stat      dnsmasq_lease_watcher;

/*
 * ===========================================================================
 *  DHCP Server Implementation
 * ===========================================================================
 */
bool dnsmasq_server_init(dnsmasq_server_t *self, const char *ifname)
{
    static bool dnsmasq_server_init = false;

    /* Initialize global variables */
    if (!dnsmasq_server_init)
    {
        LOG(INFO, "dhcpv4_server: Initializing global dnsmasq instance.");

        char *var_folders[] = { "/var/etc", "/var/run", "/var/run/dnsmasq", NULL };
        char **pvar;

        for (pvar = var_folders; *pvar != NULL; pvar++)
        {
            /* Create intermediate folders if they do not exists */
            if (mkdir(*pvar, 0755) != 0 && errno != EEXIST)
            {
                LOG(CRIT, "dhcpv4_server: Unable to create folder %s", *pvar);
                return false;
            }
        }

        if (!daemon_init(&dnsmasq_server_daemon, CONFIG_OSN_DNSMASQ_PATH, DAEMON_LOG_ALL))
        {
            LOG(CRIT, "dhcpv4_server: Error initializing global DNSMASQ daemon instance.");
            return false;
        }

        /* Set the PID file location -- necessary to kill stale instances */
        if (!daemon_pidfile_set(&dnsmasq_server_daemon, CONFIG_OSN_DNSMASQ_PID_PATH, false))
        {
            LOG(WARN, "dhcpv4_server: Error setting the PID file path.");
        }

        if (!daemon_restart_set(&dnsmasq_server_daemon, true, 3.0, 10))
        {
            LOG(WARN, "dhcpv4_server: Error enabling daemon auto-restart on global instance.");
        }

        if (!daemon_atexit(&dnsmasq_server_daemon, dnsmasq_server_atexit))
        {
            LOG(WARN, "dhcpv4_server: Error setting the atexit callback.");
        }

        /* Initialize arguments */
        daemon_arg_add(&dnsmasq_server_daemon, "--keep-in-foreground");                /* Do not fork to background */
        daemon_arg_add(&dnsmasq_server_daemon, "--bind-interfaces");                   /* Bind to only interfaces in use */
        daemon_arg_add(&dnsmasq_server_daemon, "-p", CONFIG_OSN_DNSMASQ_PORT);         /* Configure dnsmasq port, default 53 */
        daemon_arg_add(&dnsmasq_server_daemon, "-u", CONFIG_OSN_DNSMASQ_USER);         /* Change user (default nobody)*/
        daemon_arg_add(&dnsmasq_server_daemon, "-C", CONFIG_OSN_DNSMASQ_ETC_PATH);     /* Config file path */
        daemon_arg_add(&dnsmasq_server_daemon, "-x", CONFIG_OSN_DNSMASQ_PID_PATH);     /* PID file */

        ev_debounce_init(&dnsmasq_server_debounce, __dnsmasq_server_apply, DNSMASQ_SERVER_DEBOUNCE_TIMER);

        /* Initialize the DHCP leases file watcher */
        ev_stat_init(
                &dnsmasq_lease_watcher,
                dnsmasq_lease_onchange,
                CONFIG_OSN_DNSMASQ_LEASE_PATH,
                0.0);

        dnsmasq_server_init = true;

#if defined(CONFIG_OSN_DNSMASQ_KEEP_RUNNING)
        /* Start dnsmasq immediately */
        dnsmasq_server_apply(self, dnsmasq_server_init);
#endif
    }

    /* Initialize this instance */
    memset(self, 0 ,sizeof(*self));

    if (strscpy(self->ds_ifname, ifname, sizeof(self->ds_ifname)) < 0)
    {
        LOG(ERR, "dhcpv4_server: Error initializing instance for %s, interface name too long.", ifname);
        return false;
    }

    /* Use default settings */
    self->ds_cfg = OSN_DHCP_SERVER_CFG_INIT;

    /* Initialize IP range list */
    ds_tree_init(&self->ds_range_list, dnsmasq_range_cmp, struct dnsmasq_range, dr_tnode);
    /* Initialize IP reservation list */
    ds_tree_init(&self->ds_reservation_list, osn_mac_addr_cmp, struct dnsmasq_reservation, ds_tnode);

    /* Insert itself into the global list */
    ds_dlist_insert_tail(&dnsmasq_server_list, self);

    return true;
}

bool dnsmasq_server_fini(dnsmasq_server_t *self)
{
    int ii;
    ds_tree_iter_t iter;
    struct dnsmasq_range *dr;
    struct dnsmasq_reservation *rip;

    bool retval = true;

    LOG(INFO, "dhcpv4_server: Stopping dnsmasq service on interface %s.", self->ds_ifname);

    /* Remove the DHCP server object instance from the global list */
    ds_dlist_remove(&dnsmasq_server_list, self);

    /* Issue a delayed server restart */
    dnsmasq_server_apply(self, false);

    /* Free DHCP options array */
    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        if (self->ds_opts[ii] != NULL)
        {
            FREE(self->ds_opts[ii]);
            self->ds_opts[ii] = NULL;
        }
    }

    /* Free DHCP range list */
    ds_tree_foreach_iter(&self->ds_range_list, dr, &iter)
    {
        ds_tree_iremove(&iter);
        FREE(dr);
    }

    /* Free DHCP reservations list */
    ds_tree_foreach_iter(&self->ds_reservation_list, rip, &iter)
    {
        ds_tree_iremove(&iter);
        if (rip->ds_hostname != NULL) FREE(rip->ds_hostname);
        FREE(rip);
    }

    return retval;
}

/*
 * Write the configuration file and restart dnsmasq
 */
void __dnsmasq_server_apply(struct ev_loop *loop, struct ev_debounce *ev, int revent)
{
    (void)loop;
    (void)ev;
    (void)revent;

    bool have_config;

    bool status = false;

    LOG(INFO, "Restarting dnsmasq.");

    if (!daemon_stop(&dnsmasq_server_daemon))
    {
        LOG(ERR, "Error stopping dnsmasq.");
        // goto exit;
    }

    /* Stop the lease watcher */
    if (ev_is_active(&dnsmasq_lease_watcher))
    {
        ev_stat_stop(EV_DEFAULT, &dnsmasq_lease_watcher);
    }

    /* Write out the new configuration file */
    have_config = dnsmasq_server_config_write();

    if (have_config || kconfig_enabled(CONFIG_OSN_DNSMASQ_KEEP_RUNNING))
    {
        ev_stat_start(EV_DEFAULT, &dnsmasq_lease_watcher);

        /* Trigger an update event right away */
        dnsmasq_lease_onchange(EV_DEFAULT, &dnsmasq_lease_watcher, 0);

        if (!daemon_start(&dnsmasq_server_daemon))
        {
            LOG(ERR, "dhcpv4_server: Error starting dnsmasq daemon.");
            goto exit;
        }
    }

    status = true;

exit:
    if (!status)
    {
        /* Propagate the error */
        dnsmasq_server_dispatch_error();
    }
}

void dnsmasq_server_enable(dnsmasq_server_t *self, bool enable)
{
    self->ds_enabled = enable;
}

/*
 * Schedule a debounced global configuration apply
 */
void dnsmasq_server_apply(dnsmasq_server_t *self, bool dnsmasq_server_init)
{
    if (self->ds_enabled || dnsmasq_server_init)
        ev_debounce_start(EV_DEFAULT, &dnsmasq_server_debounce);
}

bool dnsmasq_server_option_set(
        dnsmasq_server_t *self,
        enum osn_dhcp_option opt,
        const char *value)
{
    if (opt >= DHCP_OPTION_MAX) return false;

    if (self->ds_opts[opt] != NULL)
    {
        FREE(self->ds_opts[opt]);
        self->ds_opts[opt] = NULL;
    }

    if (value != NULL)
    {
        self->ds_opts[opt] = strdup(value);
    }

    return true;
}

bool dnsmasq_server_cfg_set(dnsmasq_server_t *self, struct osn_dhcp_server_cfg *cfg)
{
    self->ds_cfg = *cfg;
    return true;
}

bool dnsmasq_server_range_add(dnsmasq_server_t *self, osn_ip_addr_t start, osn_ip_addr_t stop)
{
    struct dnsmasq_range *dr;

    struct dnsmasq_range rfind = { .dr_range_start = start, .dr_range_stop = stop };

    dr = ds_tree_find(&self->ds_range_list, &rfind);
    if (dr != NULL)
    {
        /* Range already exists */
        return true;
    }

    dr = CALLOC(1, sizeof(struct dnsmasq_range));
    dr->dr_range_start = start;
    dr->dr_range_stop = stop;
    ds_tree_insert(&self->ds_range_list, dr, dr);

    return true;
}

bool dnsmasq_server_range_del(dnsmasq_server_t *self, osn_ip_addr_t start, osn_ip_addr_t stop)
{
    struct dnsmasq_range *dr;

    struct dnsmasq_range rfind = { .dr_range_start = start, .dr_range_stop = stop };

    dr = ds_tree_find(&self->ds_range_list, &rfind);
    if (dr == NULL) return true;

    ds_tree_remove(&self->ds_range_list, dr);

    FREE(dr);

    return true;
}

/*
 * IP reservation handling
 */
bool dnsmasq_server_reservation_add(
        dnsmasq_server_t *self,
        osn_mac_addr_t macaddr,
        osn_ip_addr_t ipaddr,
        const char *hostname)

{
    struct dnsmasq_reservation *rip = 0;

    rip = ds_tree_find(&self->ds_reservation_list, &macaddr);
    if (rip == NULL)
    {
        /* New reservation, add it */
        rip = CALLOC(1, sizeof(struct dnsmasq_reservation));
        *rip = DNSMASQ_RESERVATION_INIT;
        rip->ds_macaddr = macaddr;
        ds_tree_insert(&self->ds_reservation_list, rip, &rip->ds_macaddr);
    }

    rip->ds_ip4addr = ipaddr;

    /* Update the hostname */
    if (rip->ds_hostname != NULL)
    {
        FREE(rip->ds_hostname);
        rip->ds_hostname = NULL;
    }

    if (hostname != NULL) rip->ds_hostname = strdup(hostname);

    return true;
}

bool dnsmasq_server_reservation_del(dnsmasq_server_t *self, osn_mac_addr_t macaddr)
{
    struct dnsmasq_reservation *rip = NULL;

    rip = ds_tree_find(&self->ds_reservation_list, &macaddr);
    if (rip == NULL) return true;

    ds_tree_remove(&self->ds_reservation_list, rip);

    if (rip->ds_hostname != NULL) FREE(rip->ds_hostname);
    FREE(rip);

    return true;
}

void dnsmasq_server_status_notify(dnsmasq_server_t *self, dnsmasq_server_status_fn_t *fn)
{
    self->ds_status_fn = fn;
}

void dnsmasq_server_error_notify(dnsmasq_server_t *self, dnsmasq_server_error_fn_t *fn)
{
    self->ds_error_fn = fn;
}

/*
 * ===========================================================================
 *  Private functions
 * ===========================================================================
 */

/*
 * Write-out global dnsmasq config file
 *
 * have_config is set to false when there's no dnsmasq instances registered
 */
bool dnsmasq_server_config_write(void)
{
    FILE *fconf;
    size_t hsz;
    ds_dlist_iter_t iter;
    dnsmasq_server_t *pds;
    struct dnsmasq_reservation *rip;
    int ii;

    bool have_config = false;

    /*
     * Write out the DNSMASQ configuration file
     */
    fconf = fopen(CONFIG_OSN_DNSMASQ_ETC_PATH, "w");
    if (fconf == NULL)
    {
        LOG(ERR, "dhcpv4_server: Error opening config file for writing: %s", CONFIG_OSN_DNSMASQ_ETC_PATH);
        dnsmasq_server_dispatch_error();
        goto exit;
    }

    /* Write out the header */
    hsz = strlen(dnsmasq_server_conf_header);
    if (fwrite(dnsmasq_server_conf_header, 1, hsz, fconf) != hsz)
    {
        LOG(ERR, "dhcpv4_server: Error writing config file header: %s", CONFIG_OSN_DNSMASQ_ETC_PATH);
        dnsmasq_server_dispatch_error();
        goto exit;
    }

    /* Traverse all instances of dhcp_server_t and write their specific configs. */
    for (pds = ds_dlist_ifirst(&iter, &dnsmasq_server_list); pds != NULL; pds = ds_dlist_inext(&iter))
    {
        struct dnsmasq_range *dr;

        if (!pds->ds_enabled) continue;

        /* Write out the range */
        fprintf(fconf, "# Interface: %s\n", pds->ds_ifname);

        /* ensure that dnsmasq is listening only this interface using interface= config entry*/
        fprintf(fconf, "interface=%s\n", pds->ds_ifname);

        /* Write out the IP pool ranges */
        ds_tree_foreach(&pds->ds_range_list, dr)
        {
            fprintf(fconf, "dhcp-range=%s,"PRI_osn_ip_addr","PRI_osn_ip_addr,
                    pds->ds_ifname,
                    FMT_osn_ip_addr(dr->dr_range_start),
                    FMT_osn_ip_addr(dr->dr_range_stop));
        }

        /* Append the lease time */
        if (pds->ds_cfg.ds_lease_time > 0.0)
        {
            fprintf(fconf, ",%d", (int)pds->ds_cfg.ds_lease_time);
        }

        fprintf(fconf, "\n");

        for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
        {
            if (pds->ds_opts[ii] != NULL)
            {
                fprintf(fconf, "dhcp-option=%s,%d,%s\n", pds->ds_ifname, ii, pds->ds_opts[ii]);
            }
        }

        /* Reserved IPs section:  */
        ds_tree_foreach(&pds->ds_reservation_list, rip)
        {
            if (rip->ds_hostname)
            {
                fprintf(fconf, "dhcp-host="PRI_osn_mac_addr",%s,"PRI_osn_ip_addr"\n",
                        FMT_osn_mac_addr(rip->ds_macaddr),
                        rip->ds_hostname,
                        FMT_osn_ip_addr(rip->ds_ip4addr));
            }
            else
            {
                fprintf(fconf, "dhcp-host="PRI_osn_mac_addr","PRI_osn_ip_addr"\n",
                        FMT_osn_mac_addr(rip->ds_macaddr),
                        FMT_osn_ip_addr(rip->ds_ip4addr));
            }
        }
        fprintf(fconf, "# End: %s\n\n", pds->ds_ifname);

        have_config = true;
    }

    fprintf(fconf, "### End Interface\n");

    fflush(fconf);

exit:
    if (fconf != NULL) fclose(fconf);

    return have_config;
}

/**
 * Propagate the error to all registered instances of dnsmasq_server_t; this
 * is mainly used when a fatal error occurs which prevents dnsmasq from
 * running.
 */
void dnsmasq_server_dispatch_error(void)
{
    dnsmasq_server_t *ds;

    ds_dlist_iter_t iter;

    for (ds = ds_dlist_ifirst(&iter, &dnsmasq_server_list); ds != NULL; ds = ds_dlist_inext(&iter))
    {
        if (ds->ds_error_fn == NULL) continue;

        ds->ds_error_fn(ds);
    }
}

/* daemon_atexit() handler -- dispatches error to all active instances */
bool dnsmasq_server_atexit(daemon_t *proc)
{
    (void)proc;

    LOG(ERR, "dhcpv4_server: Global instance failed to start.");

    dnsmasq_server_dispatch_error();

    return true;
}

/*
 * Broadcast a status update to all DHCP server instances
 */
void dnsmasq_server_status_dispatch(void)
{
    dnsmasq_server_t *ds;

    /*
     * The osn_dhcp_server_status structure is cached within the osn_dhcp_server_t structure.
     * Iterate the server list and call the notification callbacks.
     */
    ds_dlist_foreach(&dnsmasq_server_list, ds)
    {
        if (ds->ds_status_fn != NULL)
        {
            ds->ds_status_fn(ds, &ds->ds_status);
        }
    }
}

/*
 * ===========================================================================
 *  DHCP lease handling
 * ===========================================================================
 */

/*
 * Clear leases associated with the server instance -- lease information is cached using the osn_dhcp_server_status
 * structure
 */
void dnsmasq_lease_clear(dnsmasq_server_t *self)
{
    if (self->ds_status.ds_leases != NULL)
    {
        FREE(self->ds_status.ds_leases);
        self->ds_status.ds_leases = NULL;
    }

    self->ds_status.ds_leases_len = 0;
}


/*
 * Add a single lease entry to the cache
 */
void dnsmasq_lease_add(dnsmasq_server_t *self, struct osn_dhcp_server_lease *dl)
{
    struct osn_dhcp_server_status *st = &self->ds_status;

    /* Resize the leases array */
    if ((st->ds_leases_len % DNSMASQ_LEASE_RE_GROW) == 0)
    {
        /* Reallocate buffer */
        st->ds_leases = REALLOC(
                st->ds_leases,
                (st->ds_leases_len + DNSMASQ_LEASE_RE_GROW) * sizeof(struct osn_dhcp_server_lease));
    }

    /* Append to array */
    st->ds_leases[st->ds_leases_len++] = *dl;
}

/*
 * Given lease info @p dl, find the matching DHCP server instance
 *
 * TODO: This function is less than optimal -- consider implementing it using some sort of
 * global cache using rb-trees of ranges and reservations
 */
dnsmasq_server_t *dnsmasq_server_find_by_lease(struct osn_dhcp_server_lease *dl)
{
    dnsmasq_server_t *ds;
    struct dnsmasq_range *range;
    struct dnsmasq_reservation *rip;

    /* Traverse list of server instances*/
    ds_dlist_foreach(&dnsmasq_server_list, ds)
    {
        /* Traverse list of DHCP reservations */
        ds_tree_foreach(&ds->ds_reservation_list, rip)
        {
            if (osn_ip_addr_cmp(&rip->ds_ip4addr, &dl->dl_ipaddr) == 0)
            {
                LOG(DEBUG, "dhcpv4_server: Lease "PRI_osn_ip_addr" is a reservation on server %s.",
                        FMT_osn_ip_addr(dl->dl_ipaddr),
                        ds->ds_ifname);
                return ds;
            }
        }

        /* Traverse list of DHCP ranges */
        ds_tree_foreach(&ds->ds_range_list, range)
        {
            /* Check if the leased IP is inside the DHCP pool range */
            if (osn_ip_addr_cmp(&dl->dl_ipaddr, &range->dr_range_start) >= 0 &&
                    osn_ip_addr_cmp(&dl->dl_ipaddr, &range->dr_range_stop) <= 0)
            {
                LOG(DEBUG, "dhcpv4_server: Lease "PRI_osn_ip_addr" is inside the pool range of server %s"
                        "("PRI_osn_ip_addr" -> "PRI_osn_ip_addr").",
                        FMT_osn_ip_addr(dl->dl_ipaddr),
                        ds->ds_ifname,
                        FMT_osn_ip_addr(range->dr_range_start),
                        FMT_osn_ip_addr(range->dr_range_stop));
                return ds;
            }
        }
    }

    LOG(DEBUG, "dhcpv4_server: Lease "PRI_osn_ip_addr" doesn't belong to any DHCP server instance.",
            FMT_osn_ip_addr(dl->dl_ipaddr));

    return NULL;
}

/*
 * Parse a single entry in the dnsmasq lease file and fill in the @p dl structure
 */
bool dnsmasq_lease_parse_line(struct osn_dhcp_server_lease *dl, const char *line)
{
    /*
     * Regular expression to match a line in the "dhcp.lease" file of the
     * following format:
     *
     * 1461412276 f4:09:d8:89:54:4f 192.168.0.181 android-c992b284e24fdd69 1,33,3,6,15,28,51,58,59 "dhcpc-1.1:ARMv4" 01:f4:09:d8:89:54:4f
     */
    static const char dnsmasq_lease_parse_re[] =
        "(^[0-9]+) "                            /* 1: Match timestamp */
        "(([a-fA-F0-9]{2}:?){6}) "              /* 2,3: Match MAC address */
        "(([0-9]+\\.?){4}) "                    /* 4,5: Match IP address */
        "(\\*|[a-zA-Z0-9_-]+) "                 /* 6: Match hostname, can be "*" */
        "(\\*|([0-9]+,?)+) "                    /* 7,8: Match fingerprint, can be "*" */
        "\"(\\*|[^\"]+)\" "                     /* 9: Match vendor-class, can be "*" */
        "[^ ]+$";                               /* Match CID, can be "*" */

    static bool parse_re_compiled = false;
    static regex_t parse_re;

    char sleasetime[C_INT32_LEN];
    char shwaddr[C_MACADDR_LEN];
    char sipaddr[C_IP4ADDR_LEN];

    regmatch_t rm[10];

    if (!parse_re_compiled && regcomp(&parse_re, dnsmasq_lease_parse_re, REG_EXTENDED) != 0)
    {
        LOG(ERR, "dhcpv4_server: Error compiling DHCP lease parsing regular expression.");
        return false;
    }

    parse_re_compiled = true;

    /* Parse the lease line using regular expressions */
    if (regexec(&parse_re, line, ARRAY_LEN(rm), rm, 0) != 0)
    {
        LOG(ERR, "NM: Invalid DHCP lease line (ignoring): %s", line);
        return false;
    }

    memset(dl, 0, sizeof(*dl));

    /* Extract the data */
    os_reg_match_cpy(
            sleasetime,
            sizeof(sleasetime),
            line,
            rm[1]);

    os_reg_match_cpy(
            shwaddr,
            sizeof(shwaddr),
            line,
            rm[2]);

    os_reg_match_cpy(
            sipaddr,
            sizeof(sipaddr),
            line, rm[4]);

    os_reg_match_cpy(dl->dl_hostname,
            sizeof(dl->dl_hostname),
            line,
            rm[6]);

    /* Copy the fingerprint */
    os_reg_match_cpy(dl->dl_fingerprint,
            sizeof(dl->dl_fingerprint),
            line,
            rm[7]);

    /* Copy the vendor-class */
    os_reg_match_cpy(dl->dl_vendorclass,
            sizeof(dl->dl_vendorclass),
            line,
            rm[9]);

    dl->dl_leasetime = strtod(sleasetime, NULL);

    if (!osn_mac_addr_from_str(&dl->dl_hwaddr, shwaddr))
    {
        LOG(ERR, "dhcpv4_server: Invalid DHCP MAC address obtained from lease file: %s", shwaddr);
        return false;
    }

    if (!osn_ip_addr_from_str(&dl->dl_ipaddr, sipaddr))
    {
        LOG(ERR, "dhcpv4_server: Invalid DHCP IPv4 address obtained from lease file: %s", sipaddr);
        return false;
    }

    return true;
}

/*
 * Parse the dnsmasq lease file and update the lease cache
 */
bool dnsmasq_lease_parse(void)
{
    dnsmasq_server_t *ds;
    FILE *lf;
    char line[1024];

    bool retval = false;

    lf = fopen(CONFIG_OSN_DNSMASQ_LEASE_PATH, "r");
    if (lf == NULL)
    {
        LOG(ERR, "dhcpv4_server: Error opening lease file: %s", CONFIG_OSN_DNSMASQ_LEASE_PATH);
        goto exit;
    }

    /*
     * Lock the file -- dnsmasq was patched to to the same -- this ensures we're
     * not reading a half-written file.
     *
     * When the file descriptor is closed, the associated lock should be released
     * as well.
     */
    if (!os_file_lock(fileno(lf), OS_LOCK_READ))
    {
        LOG(ERR, "dhcpv4_server: Error locking lease file: %s", CONFIG_OSN_DNSMASQ_LEASE_PATH);
        goto exit;
    }

    while (fgets(line, sizeof(line), lf) != NULL)
    {
        struct osn_dhcp_server_lease dl;

        if (!dnsmasq_lease_parse_line(&dl, line))
        {
            LOG(WARN, "dhcpv4_server: Error parsing DHCP lease line: %s", line);
            continue;
        }

        /* Find the server instance that this lease belongs to */
        ds = dnsmasq_server_find_by_lease(&dl);
        if (ds == NULL)
        {
            LOG(NOTICE, "dhcpv4_server: Unable find server instance associated with lease: "PRI_osn_ip_addr,
                    FMT_osn_ip_addr(dl.dl_ipaddr));
            continue;
        }

        dnsmasq_lease_add(ds, &dl);
    }

    retval = true;
exit:
    if (lf != NULL) fclose(lf);

    return retval;
}

/*
 * Callback function triggered by file status change on the lease file
 */
void dnsmasq_lease_onchange(struct ev_loop *loop, ev_stat *w, int revent)
{
    (void)loop;
    (void)revent;

    dnsmasq_server_t *ds;

    /* Clear all leases */
    ds_dlist_foreach(&dnsmasq_server_list, ds)
    {
        dnsmasq_lease_clear(ds);
    }

    if (w->attr.st_nlink)
    {
        LOG(INFO, "dhcpv4_server: Lease file changed.");
        dnsmasq_lease_parse();
    }
    else
    {
        LOG(INFO, "dhcpv4_server: Lease file removed, flushing all entries.");
    }

    /* Send out status change notifications */
    dnsmasq_server_status_dispatch();
}

