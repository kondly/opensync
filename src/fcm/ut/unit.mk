# Copyright (c) 2015, Plume Design Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#    1. Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#    2. Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#    3. Neither the name of the Plume Design Inc. nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

UNIT_DISABLE := $(if $(CONFIG_MANAGER_FCM),n,y)

UNIT_NAME := test_fcm

UNIT_TYPE := TEST_BIN

UNIT_SRC := test_fcm_mgr.c
UNIT_SRC += test_fcm_mock_fun.c
UNIT_SRC += ../src/fcm_ovsdb.c
UNIT_SRC += ../src/fcm_mgr.c
UNIT_SRC += ../src/fcm_event.c
UNIT_SRC += ../src/fcm_gatekeeper.c

UNIT_CFLAGS := -I$(UNIT_PATH)/../inc

UNIT_LDFLAGS := -lev
UNIT_LDFLAGS += -Wl,--wrap=dlopen
UNIT_LDFLAGS += -Wl,--wrap=dlsym
UNIT_LDFLAGS += -Wl,--wrap=dlclose

UNIT_DEPS := src/lib/log
UNIT_DEPS += src/lib/common
UNIT_DEPS += src/lib/unity
UNIT_DEPS += src/lib/osa
UNIT_DEPS += src/lib/fcm_filter
UNIT_DEPS += src/lib/network_metadata
UNIT_DEPS += src/lib/ovsdb
UNIT_DEPS += src/lib/neigh_table
UNIT_DEPS += src/lib/unit_test_utils
UNIT_DEPS += src/lib/data_report_tags
UNIT_DEPS += src/lib/gatekeeper_msg
UNIT_DEPS += src/lib/fsm_policy
