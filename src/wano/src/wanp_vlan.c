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

#include "module.h"
#include "osa_assert.h"
#include "memutil.h"

#include "wano.h"

#include "wanp_vlan_stam.h"
#include "wano_wan.h"

/*
 * Structure representing a single VLAN plug-in instance
 */
struct wanp_vlan
{
    wano_plugin_handle_t        wvl_handle;
    char                        wvl_ifvlan[C_IFNAME_LEN+32];
    struct wano_wan_config      wvl_wan_config;
    bool                        wvl_have_config;
    bool                        wvl_if_created;
    wano_plugin_status_fn_t    *wvl_status_fn;
    wanp_vlan_state_t           wvl_state;
    wano_inet_state_event_t     wvl_inet_state_event;
    wano_ppline_t               wvl_ppl;
    wano_ppline_event_t         wvl_ppe;
};

static void wanp_vlan_module_start(void *data);
static void wanp_vlan_module_stop(void *data);
static wano_plugin_ops_init_fn_t wanp_vlan_init;
static wano_plugin_ops_run_fn_t wanp_vlan_run;
static wano_plugin_ops_fini_fn_t wanp_vlan_fini;
static wano_inet_state_event_fn_t wanp_vlan_inet_state_event_fn;
static wano_ppline_event_fn_t wanp_vlan_ppline_event_fn;

static struct wano_plugin wanp_vlan = WANO_PLUGIN_INIT(
        "vlan",
        40,
        WANO_PLUGIN_MASK_ALL,
        wanp_vlan_init,
        wanp_vlan_run,
        wanp_vlan_fini);

/*
 * ===========================================================================
 *  Plugin implementation
 * ===========================================================================
 */
wano_plugin_handle_t *wanp_vlan_init(
        const struct wano_plugin *wp,
        const char *ifname,
        wano_plugin_status_fn_t *status_fn)
{
    struct wanp_vlan *self;

    self = CALLOC(1, sizeof(struct wanp_vlan));

    STRSCPY(self->wvl_handle.wh_ifname, ifname);
    self->wvl_handle.wh_plugin = wp;
    self->wvl_status_fn = status_fn;

    return &self->wvl_handle;
}

void wanp_vlan_run(wano_plugin_handle_t *wh)
{
    struct wanp_vlan *self = CONTAINER_OF(wh, struct wanp_vlan, wvl_handle);

    /* Load the persistent VLAN configuration and verify that it is valid */
    self->wvl_have_config = wano_wan_config_get(
                wano_wan_from_plugin_handle(&self->wvl_handle),
                WC_TYPE_VLAN,
                &self->wvl_wan_config);

    if (!self->wvl_have_config)
    {
        LOG(INFO, "wanp_vlan: No persistent configuration present.");
        self->wvl_status_fn(wh, &WANO_PLUGIN_STATUS(WANP_SKIP));
        return;
    }

    /* Generate the interface name */
    snprintf(self->wvl_ifvlan, sizeof(self->wvl_ifvlan),
            "%s.%d",
            self->wvl_handle.wh_ifname,
            self->wvl_wan_config.wc_type_vlan.wc_vlanid);

    wano_ppline_event_init(&self->wvl_ppe, wanp_vlan_ppline_event_fn);

    LOG(INFO, "wanp_vlan: Creating interface %s with VLAN ID %d.",
            self->wvl_ifvlan,
            self->wvl_wan_config.wc_type_vlan.wc_vlanid);

    wanp_vlan_state_do(&self->wvl_state, wanp_vlan_do_INIT, NULL);
}

void wanp_vlan_fini(wano_plugin_handle_t *wh)
{
    struct wanp_vlan *self = CONTAINER_OF(wh, struct wanp_vlan, wvl_handle);

    wano_inet_state_event_fini(&self->wvl_inet_state_event);

    wano_ppline_event_stop(&self->wvl_ppe);

    wano_ppline_fini(&self->wvl_ppl);

    if (self->wvl_have_config)
    {
        wano_wan_status_set(
                wano_wan_from_plugin_handle(wh),
                WC_TYPE_VLAN,
                WC_STATUS_ERROR);
    }

    /* If the VLAN interface was created, disable it */
    if (self->wvl_if_created)
    {
        if (!WANO_INET_CONFIG_UPDATE(
                self->wvl_ifvlan,
                .enabled = WANO_TRI_FALSE,
                .network = WANO_TRI_FALSE))
        {
            LOG(WARN, "wanp_vlan: %s: Error disabling VLAN interface %s.",
                    self->wvl_handle.wh_ifname, self->wvl_ifvlan);
        }
    }

    FREE(self);
}

void wanp_vlan_inet_state_event_fn(wano_inet_state_event_t *ise, struct wano_inet_state *is)
{
    struct wanp_vlan *self = CONTAINER_OF(ise, struct wanp_vlan, wvl_inet_state_event);

    wanp_vlan_state_do(&self->wvl_state, wanp_vlan_do_INET_STATE_UPDATE, is);
}

void wanp_vlan_ppline_event_fn(
        wano_ppline_event_t *wpe,
        enum wano_ppline_status ps)
{
    struct wanp_vlan *self = CONTAINER_OF(wpe, struct wanp_vlan, wvl_ppe);

    wanp_vlan_state_do(&self->wvl_state, wanp_vlan_do_PPLINE_UPDATE, &ps);
}

/*
 * ===========================================================================
 *  OVSDB support functions
 * ===========================================================================
 */

/*
 * ===========================================================================
 *  State Machine
 * ===========================================================================
 */
enum wanp_vlan_state wanp_vlan_state_INIT(
        wanp_vlan_state_t *state,
        enum wanp_vlan_action action,
        void *data)
{
    (void)state;
    (void)data;

    switch (action)
    {
        case wanp_vlan_do_INIT:
            return wanp_vlan_IF_CREATE;

        default:
            break;
    }

    return 0;
}

enum wanp_vlan_state wanp_vlan_state_IF_CREATE(
        wanp_vlan_state_t *state,
        enum wanp_vlan_action action,
        void *data)
{
    (void)data;

    struct wanp_vlan *self = CONTAINER_OF(state, struct wanp_vlan, wvl_state);
    struct wano_inet_state *is = data;

    switch (action)
    {
        case wanp_vlan_do_STATE_INIT:
            /*
             * Register to Wifi_Inet_State events and create the VLAN interface
             * by populating Wifi_Inet_Config.
             */
            if (!WANO_INET_CONFIG_UPDATE(
                    self->wvl_ifvlan,
                    .if_type = "vlan",
                    .parent_ifname = self->wvl_handle.wh_ifname,
                    .vlan_id = self->wvl_wan_config.wc_type_vlan.wc_vlanid,
                    .network = WANO_TRI_TRUE,
                    .enabled = WANO_TRI_TRUE,
                    .nat = WANO_TRI_TRUE,
                    .ip_assign_scheme = "none"))
            {
                LOG(ERR, "wanp_vlan: %s: Error creating VLAN interface %s.",
                        self->wvl_handle.wh_ifname,
                        self->wvl_ifvlan);
                return wanp_vlan_ERROR;
            }

            wano_inet_state_event_init(
                    &self->wvl_inet_state_event,
                    self->wvl_handle.wh_ifname,
                    wanp_vlan_inet_state_event_fn);

            self->wvl_if_created = true;
            break;

        case wanp_vlan_do_INET_STATE_UPDATE:
            if (!is->is_enabled) break;
            if (!is->is_network) break;
            if (strcmp(is->is_ip_assign_scheme, "none") != 0) break;

            LOG(INFO, "wanp_vlan: %s: VLAN interface %s was successfully created.",
                    self->wvl_handle.wh_ifname, self->wvl_ifvlan);

            wano_inet_state_event_fini(&self->wvl_inet_state_event);

            return wanp_vlan_PPLINE_CREATE;

        default:
            break;
    }

    return 0;
}

enum wanp_vlan_state wanp_vlan_state_PPLINE_CREATE(
        wanp_vlan_state_t *state,
        enum wanp_vlan_action action,
        void *data)
{
    (void)data;

    struct wanp_vlan *self = CONTAINER_OF(state, struct wanp_vlan, wvl_state);
    enum wano_ppline_status *ps = data;
    wano_wan_t *wan;

    switch (action)
    {
        case wanp_vlan_do_STATE_INIT:
            self->wvl_status_fn(&self->wvl_handle, &WANO_PLUGIN_STATUS(WANP_BUSY));
            /* Create a new IPv4/IPV6 plug-in pipeline on the VLAN interface */
            if (!wano_ppline_init(
                    &self->wvl_ppl,
                    self->wvl_ifvlan,
                    "vlan",
                    ~(WANO_PLUGIN_MASK_IPV4 | WANO_PLUGIN_MASK_IPV6)))
            {
                LOG(ERR, "wanp_vlan: %s: Error creating new plug-in pipeline on interface %s.",
                        self->wvl_handle.wh_ifname, self->wvl_ifvlan);
                return wanp_vlan_ERROR;
            }

            /* Inherit the WAN configuration from the parent */
            wan = wano_wan_from_plugin_handle(&self->wvl_handle);
            if (wan != NULL)
            {
                wano_ppline_wan_set(&self->wvl_ppl, wan);
            }

            wano_ppline_event_start(&self->wvl_ppe, &self->wvl_ppl);
            break;

        case wanp_vlan_do_PPLINE_UPDATE:
            switch (*ps)
            {
                case WANO_PPLINE_OK:
                    LOG(INFO, "wanp_vlan: %s: Plug-in pipeline success.",
                            self->wvl_handle.wh_ifname);
                    self->wvl_status_fn(&self->wvl_handle, &WANO_PLUGIN_STATUS(WANP_RESERVED));
                    return wanp_vlan_IDLE;

                case WANO_PPLINE_FREEZE:
                    LOG(INFO, "wanp_vlan: %s: Plug-in pipeline was frozen.",
                            self->wvl_handle.wh_ifname);
                    self->wvl_status_fn(&self->wvl_handle, &WANO_PLUGIN_STATUS(WANP_RESERVED));
                    return wanp_vlan_IDLE;

                case WANO_PPLINE_IDLE:
                    LOG(INFO, "wanp_vlan: %s: Pipeline wasn't able to provision any plug-ins on %s.",
                            self->wvl_handle.wh_ifname, self->wvl_ifvlan);
                    return wanp_vlan_ERROR;

                case WANO_PPLINE_ABORT:
                    LOG(INFO, "wanp_vlan: %s: Pipeline was aborted %s.",
                            self->wvl_handle.wh_ifname, self->wvl_ifvlan);
                    return wanp_vlan_ERROR;

                case WANO_PPLINE_RESTART:
                    LOG(INFO, "wanp_vlan: %s: Plug-in pipeline started on %s.",
                            self->wvl_handle.wh_ifname, self->wvl_ifvlan);
                    break;
            }
            break;

        default:
            break;
    }

    return 0;
}

enum wanp_vlan_state wanp_vlan_state_IDLE(
        wanp_vlan_state_t *state,
        enum wanp_vlan_action action,
        void *data)
{
    (void)data;

    struct wanp_vlan *self = CONTAINER_OF(state, struct wanp_vlan, wvl_state);
    enum wano_ppline_status *ps = data;

    switch (action)
    {
        case wanp_vlan_do_STATE_INIT:
            wano_wan_status_set(
                    wano_wan_from_plugin_handle(&self->wvl_handle),
                    WC_TYPE_VLAN,
                    WC_STATUS_SUCCESS);
            break;

        case wanp_vlan_do_PPLINE_UPDATE:
            switch (*ps)
            {
                case WANO_PPLINE_OK:
                    break;

                case WANO_PPLINE_RESTART:
                    LOG(INFO, "wanp_vlan: %s: Plug-in pipeline started on %s.",
                            self->wvl_handle.wh_ifname, self->wvl_ifvlan);
                    break;

                case WANO_PPLINE_FREEZE:
                    LOG(ERR, "wano_vlan: %s: Plug-in pipeline frozen on %s.",
                            self->wvl_handle.wh_ifname, self->wvl_ifvlan);
                    break;

                case WANO_PPLINE_IDLE:
                    LOG(INFO, "wano_vlan: %s: Plug-in pipeline exhausted on %s.",
                            self->wvl_handle.wh_ifname, self->wvl_ifvlan);
                    return wanp_vlan_ERROR;

                case WANO_PPLINE_ABORT:
                    LOG(ERR, "wano_vlan: %s: Plug-in pipeline aborted on %s.",
                            self->wvl_handle.wh_ifname, self->wvl_ifvlan);
                    return wanp_vlan_ERROR;

            }
            break;

        default:
            break;
    }

    return 0;
}

enum wanp_vlan_state wanp_vlan_state_ERROR(
        wanp_vlan_state_t *state,
        enum wanp_vlan_action action,
        void *data)
{
    (void)data;

    struct wanp_vlan *self = CONTAINER_OF(state, struct wanp_vlan, wvl_state);

    wano_ppline_event_stop(&self->wvl_ppe);

    switch (action)
    {
        case wanp_vlan_do_STATE_INIT:
        {
            struct wano_plugin_status ws = WANO_PLUGIN_STATUS(WANP_SKIP);
            self->wvl_status_fn(&self->wvl_handle, &ws);

            if (self->wvl_have_config)
            {
                wano_wan_status_set(
                        wano_wan_from_plugin_handle(&self->wvl_handle),
                        WC_TYPE_VLAN,
                        WC_STATUS_ERROR);
            }
            break;
        }

        default:
            break;
    }

    return 0;
}

/*
 * ===========================================================================
 *  Module Support
 * ===========================================================================
 */
void wanp_vlan_module_start(void *data)
{
    (void)data;
    wano_plugin_register(&wanp_vlan);
}

void wanp_vlan_module_stop(void *data)
{
    (void)data;
    wano_plugin_unregister(&wanp_vlan);
}

MODULE(wanp_vlan, wanp_vlan_module_start, wanp_vlan_module_stop)
