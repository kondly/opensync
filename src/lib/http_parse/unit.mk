###############################################################################
#
# HTTP Parsing library
#
###############################################################################
UNIT_NAME := fsm_http

UNIT_DISABLE := $(if $(CONFIG_LIB_LEGACY_FSM_HTTP_PARSER),n,y)

ifeq ($(CONFIG_FSM_NO_DSO),y)
	UNIT_TYPE := LIB
else
	UNIT_TYPE := SHLIB
	UNIT_DIR := lib
endif

UNIT_SRC := src/http_parse.c
UNIT_SRC += src/http_parser.c

UNIT_CFLAGS := -I$(UNIT_PATH)/inc
UNIT_CFLAGS += -Isrc/fsm/inc

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS :=

UNIT_DEPS := src/lib/const
UNIT_DEPS += src/lib/log
UNIT_DEPS += src/lib/ovsdb
UNIT_DEPS += src/lib/ustack
UNIT_DEPS += src/lib/json_mqtt
