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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "build_version.h"
#include "util.h"
#include "log.h"
#include "os_nif.h"

#include "osp_unit.h"

#ifndef TARGET_NATIVE
#pragma message("WARNING: you are using default osp_unit implementation")
#endif

static bool util_if_mac_get(void *buff, size_t buffsz, char *if_name)
{
    memset(buff, 0, buffsz);

    os_macaddr_t mac;
    size_t n;

    // get if MAC address
    if (true == os_nif_macaddr(if_name, &mac))
    {
        n = snprintf(buff, buffsz, PRI(os_macaddr_plain_t), FMT(os_macaddr_t, mac));
        if (n >= buffsz) {
            LOG(ERR, "buffer not large enough");
            return false;
        }
        return true;
    }
    LOG(ERR, "Unable to retrieve MAC address for %s.", if_name);

    return false;
}

/**
  * @brief get the serial number
  *        Serial number is defined by kconfig
  *        CONFIG_TARGET_ETH0_NAME. If it is not defined,
  *        get it from eth0 interface or en* interface.
  */
bool osp_unit_serial_get(char *buff, size_t buffsz)
{
    memset(buff, 0, buffsz);

#if defined(CONFIG_TARGET_ETH0_NAME)
    if (util_if_mac_get(buff, buffsz, CONFIG_TARGET_ETH0_NAME))
    {
        return true;
    }
#else
    if (util_if_mac_get(buff, buffsz, "eth0"))
    {
        return true;
    }
#endif

    // eth0 not found, find en* interface
    char interface[256];
    FILE *f;
    int r;
    *interface = 0;
    f = popen("cd /sys/class/net; ls -d en* | head -1", "r");
    if (!f) return false;
    r = fread(interface, 1, sizeof(interface), f);
    if (r > 0) {
        if (interface[r - 1] == '\n') r--;
        interface[r] = 0;
    }
    pclose(f);
    if (!*interface) return false;
    if (util_if_mac_get(buff, buffsz, interface))
    {
        return true;
    }

    return false;
}

bool osp_unit_id_get(char *buff, size_t buffsz)
{
    return osp_unit_serial_get(buff, buffsz);
}

bool osp_unit_model_get(char *buff, size_t buffsz)
{
    strscpy(buff, CONFIG_TARGET_MODEL, buffsz);
    return true;
}

bool osp_unit_sku_get(char *buff, size_t buffsz)
{
    return false;
}

bool osp_unit_hw_revision_get(char *buff, size_t buffsz)
{
    return false;
}

bool osp_unit_platform_version_get(char *buff, size_t buffsz)
{
    return false;
}

bool osp_unit_sw_version_get(char *buff, size_t buffsz)
{
    strscpy(buff, app_build_ver_get(), buffsz);
    return true;
}

bool osp_unit_vendor_name_get(char *buff, size_t buffsz)
{
    return false;
}

bool osp_unit_vendor_part_get(char *buff, size_t buffsz)
{
    return false;
}

bool osp_unit_manufacturer_get(char *buff, size_t buffsz)
{
    return false;
}

bool osp_unit_factory_get(char *buff, size_t buffsz)
{
    return false;
}

bool osp_unit_mfg_date_get(char *buff, size_t buffsz)
{
    return false;
}

bool osp_unit_dhcpc_hostname_get(void *buff, size_t buffsz)
{
    char serial_num[buffsz];
    char model_name[buffsz];

    memset(serial_num, 0, (sizeof(char) * buffsz));
    memset(model_name, 0, (sizeof(char) * buffsz));

    if (!osp_unit_serial_get(serial_num, sizeof(serial_num)))
    {
        LOG(ERR, "Unable to get serial number");
        return false;
    }
    if (!osp_unit_model_get(model_name, sizeof(model_name)))
    {
        LOG(ERR, "Unable to get model name");
        return false;
    }

    snprintf(buff, buffsz, "%s_%s", serial_num, model_name);

    return true;
}
