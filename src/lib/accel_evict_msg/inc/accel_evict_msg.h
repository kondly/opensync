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

#ifndef ACCEL_EVICT_MESSAGE_H_INCLUDED
#define ACCEL_EVICT_MESSAGE_H_INCLUDED

#ifdef CONFIG_ACCEL_FLOW_EVICT_MESSAGE
int accel_evict_msg_send(const char *ifname, const os_macaddr_t *target_mac, const uint8_t *data, size_t data_len);
int accel_evict_msg_socket_send(const os_macaddr_t *target_mac, const uint8_t *data, size_t data_len);
int accel_evict_msg_socket_init(const char *ifname);
int accel_evict_msg_socket_exit(void);
#else
static inline int accel_evict_msg(const char *ifname, const os_macaddr_t *target_mac, const uint8_t *data, size_t data_len)
{
    return 0;
}

static inline int accel_evict_msg_socket_send(const os_macaddr_t *target_mac, const uint8_t *data, size_t data_len)
{
    return 0;
}

static inline int accel_evict_msg_socket_init(const char *ifname)
{
    return 0;
}

static inline int accel_evict_msg_socket_exit(void)
{
    return 0;
}

#endif

#endif /* ACCEL_EVICT_MESSAGE_H_INCLUDED */
