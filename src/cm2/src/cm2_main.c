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
#include <ev.h>
#include <syslog.h>
#include <getopt.h>

#include "ds_tree.h"
#include "log.h"
#include "timevt.h"
#include "os.h"
#include "os_socket.h"
#include "ovsdb.h"
#include "evext.h"
#include "os_backtrace.h"
#include "json_util.h"
#include "target.h"
#include "kconfig.h"
#include "cm2.h"
#include "cm2_uplink_event.h"

/******************************************************************************/

#define MODULE_ID LOG_MODULE_ID_MAIN
#define CM2_DMP_MEM_USAGE_TIMER 120

/******************************************************************************/

cm2_state_t             g_state;

static log_severity_t   cm2_log_severity = LOG_SEVERITY_INFO;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/

static void dump_proc_mem_usage(void)
{
    char fname[128] = {};
    char line[1024] = {};
    int pid = getpid();
    int vmrss = 0;
    int vmsize = 0;

    snprintf(fname, sizeof(fname), "/proc/%d/status", pid);

    FILE *file = fopen(fname, "r");
    if (file) {

        while (fgets(line, sizeof(line), file)) {
            if (strstr(line, "VmRSS:"))
            {
                vmrss = atoi(line + strlen("VmRSS: "));
            }
            else if (strstr(line, "VmSize:"))
            {
                 vmsize = atoi(line + strlen("VmSize: "));
            }
        }

        fclose(file);
        LOGI("pid %d: mem usage: real mem: %d, virt mem %d\n", pid, vmrss, vmsize);
    }
    return;
}

static void cm2_dmp_mem_usage_timer_cb(struct ev_loop *loop, ev_timer *timer, int revents)
{
    dump_proc_mem_usage();
}

static const char *cm2_sta_list(void)
{
    return
#ifdef CONFIG_OVSDB_BOOTSTRAP_WIFI_STA_LIST
    CONFIG_OVSDB_BOOTSTRAP_WIFI_STA_LIST
#else
    NULL
#endif
    ;
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/

bool cm2_wan_link_selection_enabled(void)
{
    return kconfig_enabled(CONFIG_TARGET_ENABLE_WAN_LINK_SELECTION);
}

int main(int argc, char ** argv)
{
    struct ev_loop *loop = EV_DEFAULT;

    // Parse command-line arguments
    if (os_get_opt(argc, argv, &cm2_log_severity)){
        return -1;
    }

    // enable logging
    target_log_open("CM", 0);
    LOGN("Starting CM (connection manager)");
    log_severity_set(cm2_log_severity);

    /* Register to dynamic severity updates */
    log_register_dynamic_severity(loop);

    (void)te_client_init(NULL);
    TELOG_STEP("MANAGER", argv[0], "start", NULL);

    backtrace_init();

    json_memdbg_init(loop);

    if (!target_init(TARGET_INIT_MGR_CM, loop)) {
        return -1;
    }

    if (!ovsdb_init_loop(loop, "CM")) {
        LOGE("Initializing CM "
             "(Failed to initialize OVSDB)");
        return -1;
    }

    cm2_event_init(loop);

    if (cm2_ovsdb_init()) {
        LOGE("Initializing CM "
             "(Failed to initialize CM tables)");
        return -1;
    }

    if (cm2_wan_link_selection_enabled()) {
        g_state.bh_dhcp = cm2_bh_dhcp_from_list(cm2_sta_list());
        g_state.bh_gre = cm2_bh_gre_from_list(cm2_sta_list());
        g_state.bh_cmu = cm2_bh_cmu_from_list(cm2_sta_list());
        g_state.bh_mlo = cm2_bh_mlo_alloc(g_state.bh_dhcp, g_state.bh_gre, g_state.bh_cmu);
        cm2_wdt_init(loop);
        cm2_update_uplinks_init(loop);
    }

    cm2_stability_init(loop);
    cm2_uplink_event_init();

    if (cm2_start_cares()) {
        LOGW("Ares init failed");
        return -1;
    }

    ev_timer dump_mem_usage_timer;
    ev_timer_init(&dump_mem_usage_timer, cm2_dmp_mem_usage_timer_cb, 0., CM2_DMP_MEM_USAGE_TIMER);
    ev_timer_start (loop, &dump_mem_usage_timer);

    ev_run(loop, 0);

    if (cm2_wan_link_selection_enabled()) {
        cm2_wdt_close(loop);
        cm2_update_uplinks_close(loop);
    }

    cm2_stability_close(loop);
    cm2_uplink_event_close();
    cm2_event_close(loop);

    target_close(TARGET_INIT_MGR_CM, loop);

    if (!ovsdb_stop_loop(loop)) {
        LOGE("Stopping CM "
             "(Failed to stop OVSDB");
    }

    cm2_stop_cares();
    ev_default_destroy();

    LOGN("Exiting CM");

    TELOG_STEP("MANAGER", argv[0], "stop", NULL);
    te_client_deinit();

    return 0;
}
