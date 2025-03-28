UNIT_DISABLE := $(if $(CONFIG_LIB_LEGACY_FSM_HTTP_PARSER),n,y)

UNIT_NAME := test_http_parse

UNIT_TYPE := TEST_BIN

UNIT_SRC := test_http_parse.c

UNIT_DEPS := src/lib/log
UNIT_DEPS += src/lib/common
UNIT_DEPS += src/lib/json_util
UNIT_DEPS += src/qm/qm_conn
UNIT_DEPS += src/lib/http_parse
UNIT_DEPS += src/lib/unity
UNIT_DEPS += src/lib/unit_test_utils
