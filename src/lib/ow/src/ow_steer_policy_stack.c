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

#include <assert.h>
#include <log.h>
#include <util.h>
#include <const.h>
#include <ds_dlist.h>
#include <ds_tree.h>
#include <memutil.h>
#include <module.h>
#include <osw_timer.h>
#include <osw_state.h>
#include <osw_bss_map.h>
#include <osw_conf.h>
#include <osw_diag.h>
#include "ow_steer_candidate_list.h"
#include "ow_steer_policy.h"
#include "ow_steer_policy_i.h"
#include "ow_steer_policy_priv.h"
#include "ow_steer_sta.h"
#include "ow_steer_sta_priv.h"
#include "ow_steer_policy_stack.h"

#define LOG_PREFIX(fmt, ...) "ow: steer: " fmt, ##__VA_ARGS__
#define LOG_WITH_PREFIX(stack, fmt, ...)                      \
    LOG_PREFIX(                                               \
        "%s" fmt,                                             \
        ((stack) != NULL ? ((stack)->log_prefix ?: "") : ""), \
        ##__VA_ARGS__)

struct ow_steer_policy_stack {
    struct ds_dlist policy_list;
    struct ow_steer_sta *sta;
    char* log_prefix;
    struct osw_timer work;
};

static void
ow_steer_policy_stack_work_cb(struct osw_timer *timer)
{
    struct ow_steer_policy_stack *stack = container_of(timer, struct ow_steer_policy_stack, work);
    struct ow_steer_candidate_list *candidate_list = ow_steer_sta_get_candidate_list(stack->sta);
    size_t i;

    LOGD(LOG_WITH_PREFIX(stack, "recalc candidates"));
    ow_steer_candidate_list_clear(candidate_list);

    struct ow_steer_policy *policy;
    ds_dlist_foreach(&stack->policy_list, policy) {
        if (WARN_ON(policy->ops.recalc_fn == NULL))
            continue;

        policy->ops.recalc_fn(policy, candidate_list);
    }

    for (i = 0; i < ow_steer_candidate_list_get_length(candidate_list); i++) {
        struct ow_steer_candidate *candidate = ow_steer_candidate_list_get(candidate_list, i);
        const struct osw_hwaddr *bssid = ow_steer_candidate_get_bssid(candidate);
        const enum ow_steer_candidate_preference preference = ow_steer_candidate_get_preference(candidate);
        const char *reason = "default";

        if (preference == OW_STEER_CANDIDATE_PREFERENCE_NONE)
            ow_steer_candidate_set_preference(candidate, reason, OW_STEER_CANDIDATE_PREFERENCE_AVAILABLE);

        LOGD(LOG_WITH_PREFIX(stack, "candidate: "OSW_HWADDR_FMT": preference: %s",
             OSW_HWADDR_ARG(bssid), ow_steer_candidate_preference_to_cstr(ow_steer_candidate_get_preference(candidate))));
     }

    ow_steer_sta_schedule_executor_call(stack->sta);
}

struct ow_steer_policy_stack*
ow_steer_policy_stack_create(struct ow_steer_sta *sta, const char* log_prefix)
{
    assert(sta != NULL);

    struct ow_steer_policy_stack *stack = CALLOC(1, sizeof(*stack));

    ds_dlist_init(&stack->policy_list, struct ow_steer_policy, stack_node);
    stack->sta = sta;
    stack->log_prefix = strfmt("%spolicy_stack: ", log_prefix);
    osw_timer_init(&stack->work, ow_steer_policy_stack_work_cb);

    return stack;
}

void
ow_steer_policy_stack_free(struct ow_steer_policy_stack *stack)
{
    assert(stack != NULL);

    osw_timer_disarm(&stack->work);
    assert(ds_dlist_is_empty(&stack->policy_list) == true);
    FREE(stack->log_prefix);
    FREE(stack);
}

void
ow_steer_policy_stack_add(struct ow_steer_policy_stack *stack,
                          struct ow_steer_policy *policy)
{
    assert(stack != NULL);
    assert(policy != NULL);
    assert(policy->stack == NULL);

    policy->stack = stack;
    ds_dlist_insert_tail(&stack->policy_list, policy);
    ow_steer_policy_stack_schedule_recalc(stack);
}

void
ow_steer_policy_stack_remove(struct ow_steer_policy_stack *stack,
                             struct ow_steer_policy *policy)
{
    assert(stack != NULL);
    assert(policy != NULL);
    assert(policy->stack != NULL);

    ds_dlist_remove(&stack->policy_list, policy);
    ow_steer_policy_stack_schedule_recalc(stack);
    policy->stack = NULL;
}

bool
ow_steer_policy_stack_is_empty(struct ow_steer_policy_stack *stack)
{
    assert(stack != NULL);
    return ds_dlist_is_empty(&stack->policy_list);
}

const struct ow_steer_policy *
ow_steer_policy_get_more_important(const struct ow_steer_policy *a,
                                   const struct ow_steer_policy *b)
{
    if (a != NULL && a->stack == NULL) a = NULL;
    if (b != NULL && b->stack == NULL) b = NULL;
    if (a != NULL && b != NULL && a->stack != b->stack) return NULL;

    struct ow_steer_policy_stack *stack = a != NULL ? a->stack
                                        : b != NULL ? b->stack
                                        : NULL;
    if (stack == NULL) return NULL;

    struct ow_steer_policy *i;
    ds_dlist_foreach(&a->stack->policy_list, i) {
        if (i == a) return a;
        if (i == b) return b;
    }

    return NULL;
}

void
ow_steer_policy_stack_schedule_recalc(struct ow_steer_policy_stack *stack)
{
    assert(stack != NULL);
    osw_timer_arm_at_nsec(&stack->work, 0);
}

void
ow_steer_policy_stack_sigusr1_dump(osw_diag_pipe_t *pipe,
                                   struct ow_steer_policy_stack *stack)
{
    assert(stack != NULL);

    struct ow_steer_policy *policy;
    ds_dlist_foreach(&stack->policy_list, policy) {
        osw_diag_pipe_writef(pipe, LOG_WITH_PREFIX(stack, "%s", policy->name));
        if (policy->ops.sigusr1_dump_fn != NULL)
            policy->ops.sigusr1_dump_fn(pipe, policy);
    }
}

void
ow_steer_policy_stack_sta_snr_change(struct ow_steer_policy_stack *stack,
                                     const struct osw_hwaddr *sta_addr,
                                     const struct osw_hwaddr *bssid,
                                     uint32_t snr_db)
{
    assert(stack != NULL);
    assert(sta_addr != NULL);
    assert(bssid != NULL);

    const struct osw_hwaddr wildcard_bssid = OSW_HWADDR_BROADCAST;
    struct ow_steer_policy *policy;

    ds_dlist_foreach(&stack->policy_list, policy) {
        if (policy->ops.sta_snr_change_fn == NULL)
            continue;

        const struct osw_hwaddr *policy_bssid = ow_steer_policy_get_bssid(policy);
        if (osw_hwaddr_cmp(policy_bssid, bssid) != 0 && osw_hwaddr_cmp(policy_bssid, &wildcard_bssid) != 0)
            continue;

        policy->ops.sta_snr_change_fn(policy, bssid, snr_db);
    }
}

void
ow_steer_policy_stack_sta_data_vol_change(struct ow_steer_policy_stack *stack,
                                          const struct osw_hwaddr *sta_addr,
                                          const struct osw_hwaddr *bssid,
                                          uint64_t data_vol_bytes)
{
    assert(stack != NULL);
    assert(sta_addr != NULL);
    assert(bssid != NULL);

    const struct osw_hwaddr wildcard_bssid = OSW_HWADDR_BROADCAST;
    struct ow_steer_policy *policy;

    ds_dlist_foreach(&stack->policy_list, policy) {
        if (policy->ops.sta_data_vol_change_fn == NULL)
            continue;

        const struct osw_hwaddr *policy_bssid = ow_steer_policy_get_bssid(policy);
        if (osw_hwaddr_cmp(policy_bssid, bssid) != 0 && osw_hwaddr_cmp(policy_bssid, &wildcard_bssid) != 0)
            continue;

        policy->ops.sta_data_vol_change_fn(policy, bssid, data_vol_bytes);
    }
}

#include "ow_steer_policy_stack_ut.c"
