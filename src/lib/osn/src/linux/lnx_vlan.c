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

#include "ds.h"
#include "execsh.h"
#include "log.h"
#include "util.h"

#include "lnx_vlan.h"

/*
 * Script for creating a VLAN interface.
 *
 * Input parameters:
 *
 * $1 - Interface name
 * $2 - Parent interface name
 * $3 - VLANID
 */
const char lnx_vlan_create[] = _S(
    ip link add link "$2" name "$1" type vlan id "$3");


/*
 * Script for setting default VLAN egress PCP (qos) in VLAN interface.
 *
 * Input parameters:
 *
 * $1 - Interface name
 * $2 - egress QOS map : cannot be quoted, each FROM:TO map entry is 
 * parsed as a separate arg by egress-qos-map command
 */
const char lnx_vlan_set_egress[] = _S(
    ip link set "$1" type vlan egress-qos-map $2);


/*
 * Script for deleting a VLAN interface
 *
 * Input parameters:
 *
 * $1 - Interface name
 */
const char lnx_vlan_delete[] = _S(
    if [ -e "/sys/class/net/$1" ];
    then
        ip link del "$1";
    fi;);

bool lnx_vlan_init(lnx_vlan_t *self, const char *ifname)
{
    memset(self, 0, sizeof(*self));

    self->lv_vlanid = -1;

    if (strscpy(self->lv_ifname, ifname, sizeof(self->lv_ifname)) < 0)
    {
        LOG(ERR, "vlan: %s: Interface name too long.", ifname);
        return false;
    }

    return true;
}

bool lnx_vlan_fini(lnx_vlan_t *self)
{
    int rc;

    if (!self->lv_applied) return true;

    /* Silently delete old interfaces, if there are any */
    rc = execsh_log(LOG_SEVERITY_DEBUG, lnx_vlan_delete, self->lv_ifname);
    if (rc != 0)
    {
        LOG(WARN, "vlan: %s: Error deleting interface.", self->lv_ifname);
    }

    return true;
}

bool lnx_vlan_apply(lnx_vlan_t *self)
{
    char snum[C_INT32_LEN];
    int rc;

    if (self->lv_vlanid < C_VLAN_MIN || self->lv_vlanid > C_VLAN_MAX)
    {
        LOG(ERR, "vlan: %s: Unable to apply configuration, VLAN ID is not set.",
                self->lv_ifname);
        return false;
    }

    if (self->lv_pifname[0] == '\0')
    {
        LOG(ERR, "vlan: %s: Unable to apply configuration, parent interface is not set.",
                self->lv_ifname);
        return false;
    }

    self->lv_applied = true;

    snprintf(snum, sizeof(snum), "%d", self->lv_vlanid);

    /* Silently delete old interfaces, if there are any */
    execsh_log(LOG_SEVERITY_DEBUG, lnx_vlan_delete, self->lv_ifname);

    rc = execsh_log(
            LOG_SEVERITY_DEBUG,
            lnx_vlan_create,
            self->lv_ifname,
            self->lv_pifname,
            snum);
    if (rc != 0)
    {
        LOG(ERR, "vlan: %s: Error creating VLAN interface (parent %s, vlanid %s).",
                self->lv_ifname, self->lv_pifname, snum);
        return false;
    }

    // set egress qos map only if requested i.e. not empty string
    // otherwise leave default system settings
    if (self->lv_egress_qos_map[0] != '\0')
    {
        rc = execsh_log(
                LOG_SEVERITY_DEBUG,
                lnx_vlan_set_egress,
                self->lv_ifname,
                self->lv_egress_qos_map);
        if (rc != 0)
        {
            LOG(ERR, "vlan: %s: Error setting vlan egress qos map to %s.",
                    self->lv_ifname, self->lv_egress_qos_map);
            return false;
        }
    }

    return true;
}

bool lnx_vlan_parent_ifname_set(lnx_vlan_t *self, const char *parent_ifname)
{
    if (strlen(parent_ifname) >= sizeof(self->lv_pifname))
    {
        LOG(ERR, "vlan: %s: Parent interface name too long.", parent_ifname);
        return false;
    }

    STRSCPY(self->lv_pifname, parent_ifname);

    return true;
}

bool lnx_vlan_vid_set(lnx_vlan_t *self, int vid)
{
    if (vid < C_VLAN_MIN || vid > C_VLAN_MAX)
    {
        LOG(ERR, "vlan: %s: Invalid VID: %d",
                self->lv_ifname, vid);
        return false;
    }

    self->lv_vlanid = vid;

    return true;
}

bool lnx_vlan_egress_qos_map_set(lnx_vlan_t *self, const char *qos_map)
{
    if (strlen(qos_map) >= sizeof(self->lv_egress_qos_map))
    {
        LOG(ERR, "vlan: egress qos map too long: %s", qos_map);
        return false;
    }
    STRSCPY(self->lv_egress_qos_map, qos_map);
    return true;
}
