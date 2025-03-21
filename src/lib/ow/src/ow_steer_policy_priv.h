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

#ifndef OW_STEER_POLICY_PRIV_H
#define OW_STEER_POLICY_PRIV_H

void*
ow_steer_policy_get_priv(struct ow_steer_policy *policy);

void
ow_steer_policy_set_bssid(struct ow_steer_policy *policy,
                          const struct osw_hwaddr *bssid);

const char*
ow_steer_policy_get_prefix(struct ow_steer_policy *policy);

void
ow_steer_policy_schedule_stack_recalc(struct ow_steer_policy *policy);

bool
ow_steer_policy_trigger_executor(struct ow_steer_policy *policy);

void
ow_steer_policy_dismiss_executor(struct ow_steer_policy *policy);

void
ow_steer_policy_notify_backoff(struct ow_steer_policy *policy,
                               const bool enabled,
                               const unsigned int period);

void
ow_steer_policy_notify_steering_attempt(struct ow_steer_policy *policy,
                                        const char *vif_name);

#endif /* OW_STEER_POLICY_PRIV_H */
