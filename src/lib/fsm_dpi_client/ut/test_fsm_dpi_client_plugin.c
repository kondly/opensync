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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>

#include "fsm.h"
#include "fsm_dpi_client_plugin.h"
#include "os.h"
#include "network_metadata_utils.h"
#include "policy_tags.h"
#include "unity.h"
#include "unity_internals.h"

static struct fsm_session session;
static union fsm_plugin_ops p_ops;

struct schema_Openflow_Tag g_tags[] =
{
    {
        .name_exists = true,
        .name = "included",
        .device_value_len = 1,
        .device_value =
        {
            "11:11:11:11:11:11",
        },
        .cloud_value_len = 1,
        .cloud_value =
        {
            "11:11:11:11:11:11",
        },
    },
    {
        .name_exists = true,
        .name = "excluded",
        .device_value_len = 1,
        .device_value =
        {
            "22:22:22:22:22:22",
        },
        .cloud_value_len = 1,
        .cloud_value =
        {
            "22:22:22:22:22:22",
        },
    },
};

struct schema_Openflow_Tag_Group g_tag_groups[] =
{
    {
        .name_exists = true,
        .name = "include_group_tag",
        .tags_len = 1,
        .tags =
        {
            "included",
        }
    },
    {
        .name_exists = true,
        .name = "exclude_group_tag",
        .tags_len = 1,
        .tags =
        {
            "excluded",
        }
    },
};

static char *get_config(struct fsm_session *session, char *key)
{
    (void)session;

    if (strcmp(key, "included_devices") == 0) return "included";
    if (strcmp(key, "excluded_devices") == 0) return "excluded";
    return "";
}

static char *get_config_tag(struct fsm_session *session, char *key)
{
    (void)session;

    if (strcmp(key, "included_devices") == 0) return "${included}";
    if (strcmp(key, "excluded_devices") == 0) return "${excluded}";
    return "";
}

static char *get_config_group_tag(struct fsm_session *session, char *key)
{
    (void)session;

    if (strcmp(key, "included_devices") == 0) return "$[include_group_tag]";
    if (strcmp(key, "excluded_devices") == 0) return "$[exclude_group_tag]";
    return "";
}

void
test_init_exit(void)
{
    struct fsm_dpi_client_cache *mgr;
    struct fsm_session another_session;
    int ret;

    ret = fsm_dpi_client_init(NULL);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    fsm_dpi_client_exit(&session);

    MEMZERO(session);
    ret = fsm_dpi_client_init(&session);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    session.node_id = "NODE_ID";
    ret = fsm_dpi_client_init(&session);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    session.location_id = "LOCATION_ID";
    ret = fsm_dpi_client_init(&session);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    mgr = fsm_dpi_client_get_mgr();
    TEST_ASSERT_NOT_NULL(mgr);
    TEST_ASSERT_FALSE(mgr->initialized);

    session.name = "One Session";
    session.ops.get_config = get_config;
    session.p_ops = &p_ops;
    ret = fsm_dpi_client_init(&session);
    TEST_ASSERT_EQUAL_INT(0, ret);
    mgr = fsm_dpi_client_get_mgr();
    TEST_ASSERT_TRUE(mgr->initialized);

    ret = fsm_dpi_client_init(&session);
    TEST_ASSERT_EQUAL_INT(1, ret);

    another_session = session;
    another_session.name = "Two Sessions";
    ret = fsm_dpi_client_init(&another_session);
    TEST_ASSERT_EQUAL_INT(0, ret);

    fsm_dpi_client_exit(&another_session);
    fsm_dpi_client_exit(&session);
}

void
test_update(void)
{
    struct fsm_dpi_client_session *client_session;
    int ret;

    MEMZERO(session);
    session.node_id = "NODE_ID";
    session.location_id = "LOCATION_ID";
    session.name = "Updated Session";
    session.ops.get_config = get_config;
    session.p_ops = &p_ops;
    ret = fsm_dpi_client_init(&session);
    TEST_ASSERT_EQUAL_INT(0, ret);

    fsm_dpi_client_update(&session);
    client_session = session.handler_ctxt;
    TEST_ASSERT_EQUAL_STRING("included", client_session->included_devices);
    TEST_ASSERT_EQUAL_STRING("excluded", client_session->excluded_devices);

    fsm_dpi_client_exit(&session);
}

void
test_periodic(void)
{
    struct fsm_dpi_client_session *client_session;
    time_t now;
    int ret;

    MEMZERO(session);
    session.node_id = "NODE_ID";
    session.location_id = "LOCATION_ID";
    session.name = "Updated Session";
    session.ops.get_config = get_config;
    session.p_ops = &p_ops;
    ret = fsm_dpi_client_init(&session);
    TEST_ASSERT_EQUAL_INT(0, ret);

    client_session = session.handler_ctxt;
    TEST_ASSERT_EQUAL(0, client_session->timestamp);

    fsm_dpi_client_periodic(&session);
    now = time(NULL);
    TEST_ASSERT_EQUAL(now, client_session->timestamp);

    fsm_dpi_client_periodic(&session);
    TEST_ASSERT_EQUAL(now, client_session->timestamp);

    TEST_MESSAGE("Sleep 3 seconds to hit TTL");
    fsm_dpi_client_set_ttl(&session, 2);
    sleep(3);
    fsm_dpi_client_periodic(&session);
    TEST_ASSERT_EQUAL(now + 3, client_session->timestamp);

    fsm_dpi_client_exit(&session);
}

void
test_process_attr(void)
{
    struct fsm_dpi_plugin_client_pkt_info pkt_info;
    struct fsm_dpi_client_session *client_session;
    struct net_md_flow_key acc_key;
    struct net_md_stats_accumulator acc;
    const char a_string[] = "STRING\0\0";
    const uint8_t a_binary[] = "123\0""456\0";
    const int64_t a_number = 1234;
    int ret;

    MEMZERO(session);
    MEMZERO(acc_key);    
    session.node_id = "NODE_ID";
    session.location_id = "LOCATION_ID";
    session.name = "Walleye";
    session.ops.get_config = get_config;
    session.p_ops = &p_ops;
    ret = fsm_dpi_client_init(&session);
    TEST_ASSERT_EQUAL_INT(0, ret);

    ret = fsm_dpi_client_process_attr(NULL, "ATTR", RTS_TYPE_STRING, strlen(a_string), a_string, NULL);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    ret = fsm_dpi_client_process_attr(&session, "ATTR", RTS_TYPE_STRING, strlen(a_string), a_string, NULL);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    /* Now we can actually process the attribute */
    session.service = &session;
    ret = fsm_dpi_client_process_attr(&session, "A_STRING", RTS_TYPE_STRING, strlen(a_string), a_string, NULL);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    ret = fsm_dpi_client_process_attr(&session, "A_BINARY", RTS_TYPE_BINARY, sizeof(a_binary), a_binary, NULL);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    ret = fsm_dpi_client_process_attr(&session, "A_NUMBER", RTS_TYPE_NUMBER, sizeof(a_number), &a_number, NULL);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    ret = fsm_dpi_client_process_attr(&session, "WRONG_TYPE", -1, sizeof(a_number), &a_number, NULL);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    /* Included and excluded */
    client_session = session.handler_ctxt;
    TEST_ASSERT_NOT_NULL(client_session);

    MEMZERO(acc);
    pkt_info.acc = &acc;
    ret = fsm_dpi_client_process_attr(&session, "A_STRING", RTS_TYPE_STRING, strlen(a_string), a_string, &pkt_info);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    acc.originator = NET_MD_ACC_UNKNOWN_ORIGINATOR;
    ret = fsm_dpi_client_process_attr(&session, "A_STRING", RTS_TYPE_STRING, strlen(a_string), a_string, &pkt_info);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    acc.originator = NET_MD_ACC_ORIGINATOR_SRC;
    ret = fsm_dpi_client_process_attr(&session, "A_STRING", RTS_TYPE_STRING, strlen(a_string), a_string, &pkt_info);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    acc.key = &acc_key;
    ret = fsm_dpi_client_process_attr(&session, "A_STRING", RTS_TYPE_STRING, strlen(a_string), a_string, &pkt_info);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    fsm_dpi_client_exit(&session);
}

void
test_process_attr_include_exclude_list(void)
{
    struct fsm_dpi_plugin_client_pkt_info pkt_info;
    struct fsm_dpi_client_session u_session;
    struct net_md_stats_accumulator acc;
    const char a_string[] = "STRING\0\0";
    struct net_md_flow_key acc_key;
    os_macaddr_t mac_n;
    size_t num_c;
    size_t i;
    int ret;

    /* initialize tags */
    num_c = sizeof(g_tags) / sizeof(*g_tags);
    for (i = 0; i < num_c; i++)
    {
        ret = om_tag_add_from_schema(&g_tags[i]);
        TEST_ASSERT_TRUE(ret);
    }

    MEMZERO(session);
    MEMZERO(acc_key);    
    session.node_id = "NODE_ID";
    session.location_id = "LOCATION_ID";
    session.name = "Walleye";
    session.ops.get_config = get_config_tag;
    session.p_ops = &p_ops;
    session.handler_ctxt = &session;
    ret = fsm_dpi_client_init(&session);
    session.handler_ctxt = &u_session;
    fsm_dpi_client_update(&session);
    TEST_ASSERT_EQUAL_INT(0, ret);


    MEMZERO(acc);
    acc.originator = NET_MD_ACC_ORIGINATOR_SRC;
    acc.direction = NET_MD_ACC_OUTBOUND_DIR;

    /* verify include tag */
    ret = str2os_mac_ref("11:11:11:11:11:11", &mac_n);
    TEST_ASSERT_TRUE(ret);
    acc_key.smac = &mac_n;
    acc.key = &acc_key;
    pkt_info.acc = &acc;

    ret = fsm_dpi_client_process_attr(&session, "ATTR", RTS_TYPE_STRING, strlen(a_string), a_string, &pkt_info);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_INSPECT, ret);

    /* verify exclude tag */
    ret = str2os_mac_ref("22:22:22:22:22:22", &mac_n);
    TEST_ASSERT_TRUE(ret);
    acc_key.smac = &mac_n;
    acc.key = &acc_key;

    ret = fsm_dpi_client_process_attr(&session, "ATTR", RTS_TYPE_STRING, strlen(a_string), a_string, &pkt_info);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    /* remove tags */
    for (i = 0; i < num_c; i++)
    {
        ret = om_tag_remove_from_schema(&g_tags[i]);
        TEST_ASSERT_TRUE(ret);
    }

    fsm_dpi_client_exit(&session);
}

void
test_process_attr_group_include_exclude_list(void)
{
    struct fsm_dpi_plugin_client_pkt_info pkt_info;
    struct fsm_dpi_client_session u_session;
    struct net_md_stats_accumulator acc;
    const char a_string[] = "STRING\0\0";
    struct net_md_flow_key acc_key;
    os_macaddr_t mac_n;
    size_t num_c;
    size_t i;
    int ret;

    /* initialize tags */
    num_c = sizeof(g_tags) / sizeof(*g_tags);
    for (i = 0; i < num_c; i++)
    {
        ret = om_tag_add_from_schema(&g_tags[i]);
        TEST_ASSERT_TRUE(ret);
    }

    /* initialize group tags */
    num_c = sizeof(g_tag_groups) / sizeof(*g_tag_groups);
    for (i = 0; i < num_c; i++)
    {
        ret = om_tag_group_add_from_schema(&g_tag_groups[i]);
        TEST_ASSERT_TRUE(ret);
    }

    MEMZERO(session);
    MEMZERO(acc_key);    
    session.node_id = "NODE_ID";
    session.location_id = "LOCATION_ID";
    session.name = "Walleye";
    session.ops.get_config = get_config_group_tag;
    session.p_ops = &p_ops;
    session.handler_ctxt = &session;
    ret = fsm_dpi_client_init(&session);
    session.handler_ctxt = &u_session;
    fsm_dpi_client_update(&session);
    TEST_ASSERT_EQUAL_INT(0, ret);


    MEMZERO(acc);
    acc.originator = NET_MD_ACC_ORIGINATOR_SRC;
    acc.direction = NET_MD_ACC_OUTBOUND_DIR;

    /* verify include tag */
    ret = str2os_mac_ref("11:11:11:11:11:11", &mac_n);
    TEST_ASSERT_TRUE(ret);
    acc_key.smac = &mac_n;
    acc.key = &acc_key;
    pkt_info.acc = &acc;

    ret = fsm_dpi_client_process_attr(&session, "ATTR", RTS_TYPE_STRING, strlen(a_string), a_string, &pkt_info);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_INSPECT, ret);

    /* verify exclude tag */
    ret = str2os_mac_ref("22:22:22:22:22:22", &mac_n);
    TEST_ASSERT_TRUE(ret);
    acc_key.smac = &mac_n;
    acc.key = &acc_key;

    ret = fsm_dpi_client_process_attr(&session, "ATTR", RTS_TYPE_STRING, strlen(a_string), a_string, &pkt_info);
    TEST_ASSERT_EQUAL_INT(FSM_DPI_IGNORED, ret);

    num_c = sizeof(g_tags) / sizeof(*g_tags);

    /* remove tags */
    for (i = 0; i < num_c; i++)
    {
        ret = om_tag_remove_from_schema(&g_tags[i]);
        TEST_ASSERT_TRUE(ret);
    }

    num_c = sizeof(g_tag_groups) / sizeof(*g_tag_groups);

    /* remove group tags */
    for (i = 0; i < num_c; i++)
    {
        ret = om_tag_group_remove_from_schema(&g_tag_groups[i]);
        TEST_ASSERT_TRUE(ret);
    }

    fsm_dpi_client_exit(&session);
}

void
run_test_fsm_dpi_client_plugin(void)
{
    RUN_TEST(test_init_exit);
    RUN_TEST(test_update);
    RUN_TEST(test_periodic);
    RUN_TEST(test_process_attr);
    RUN_TEST(test_process_attr_include_exclude_list);
    RUN_TEST(test_process_attr_group_include_exclude_list);
}
