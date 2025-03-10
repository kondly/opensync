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

#ifndef GK_SINGLE_CURL_H_INCLUDED
#define GK_SINGLE_CURL_H_INCLUDED

#include <curl/curl.h>
#include "gatekeeper.pb-c.h"
#include "gatekeeper_msg.h"
#include "gatekeeper.h"


/**
 * @brief clean up curl handler
 *
 * @param mgr the gate keeper session
 */
void
gk_curl_easy_cleanup(struct gk_curl_easy_info *curl_info);

/**
 * @brief sends request to the gatekeeper services
 *        parses and updates the action.
 *
 * @param gk_verdict structure containing gatekeeper server
 *        information, proto buffer and fsm_policy_req
 * @return gatekeeper response code.
 */
int gk_gatekeeper_lookup(struct fsm_session *session,
                          struct fsm_gk_session *fsm_gk_session,
                          struct fsm_gk_verdict *gk_verdict,
                          struct fsm_policy_reply *policy_reply);

bool
gk_set_policy(Gatekeeper__Southbound__V1__GatekeeperReply *response,
              struct fsm_gk_verdict *gk_verdict);

/**
 * @brief Adjust redirect TTL value based on redirect
 * action.
 *
 * @param policy_reply FSM policy_reply
 * @return None
 */
bool
gk_is_redirect_reply(int req_type, int action);

#endif /* GK_SINGLE_CURL_H_INCLUDED */
