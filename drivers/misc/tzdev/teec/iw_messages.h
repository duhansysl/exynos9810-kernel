/**
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __IW_MESSAGES_H__
#define __IW_MESSAGES_H__

#ifndef __USED_BY_TZSL__
#include "core/cred.h"
#else
#include <stdint.h>
#include <uuid/uuid.h>
#endif

#define PROFILE_NAME_LEN		16
#define PROFILE_STR_LEN			(PROFILE_NAME_LEN + 1)

#define CA_DIG_LEN			32

#define TEE_MAX_CONTEXT_NAME_LEN	64

#define MAX_TA_NAME_LEN 		256

/* This macro should be used for any inter-world structures, to avoid possible
 * problems with structure padding */
#define IW_STRUCTURE			__attribute__((packed))

#define IWD_TIMEOUT_INFINITY		((uint64_t)(-1))

#define USERBOOT_SOCK			"/service/userboot"
#define ROOT_TASK_SOCK			"/service/root_task"

#ifndef __USED_BY_TZSL__
typedef struct tz_uuid tz_uuid_t;
#else
typedef uuid_t tz_uuid_t;
#endif

/*
  Used to transfer information about TEE_Param between SW and NW, and for inter ta communication
*/
struct shared_buffer_description {
    uint32_t offset;
    uint32_t size;
    uint32_t id;
} IW_STRUCTURE;

struct value {
    uint32_t a, b;
} IW_STRUCTURE;

struct entry_point_params {
    union {
        struct shared_buffer_description shared_buffer_description;
        struct value val;
    };
} IW_STRUCTURE;

enum iw_cmd_type {
    CMD_INVALID_ID = 0,
    CMD_INITIALIZE_CONTEXT = 1,
    CMD_FINALIZE_CONTEXT = 2,
    CMD_SET_SHMEM_RIGHTS = 3,
    CMD_PREPARE_SESSION = 4,
    CMD_OPEN_SESSION = 5,
    CMD_CLOSE_SESSION = 6,
    CMD_INVOKE_COMMAND = 7,
    CMD_SET_CLUSTER = 8,
    CMD_CANCELLATION = 9,
    CMD_OPEN_TZDAEMON_CONNECTION = 10,

    CMD_REPLY = 11,
    CMD_REPLY_INITIALIZE_CONTEXT = 12,
    CMD_REPLY_FINALIZE_CONTEXT = 13,
    CMD_REPLY_PREPARE_SESSION = 14,
    CMD_REPLY_SET_SHMEM_RIGHTS = 15,
    CMD_REPLY_OPEN_SESSION = 16,
    CMD_REPLY_CLOSE_SESSION = 17,
    CMD_REPLY_INVOKE_COMMAND = 18,
    CMD_REPLY_SET_CLUSTER = 19,
    CMD_REPLY_OPEN_TZDAEMON_CONNECTION = 20,

    CMD_TUI_START = 21,
    CMD_TUI_GET_RESOURCE = 22,
    CMD_TUI_GET_RESOLUTION = 23,
    CMD_TUI_STOP = 24,

    CMD_REPLY_TUI_START = 25,
    CMD_REPLY_TUI_GET_RESOLUTION = 26,
    CMD_REPLY_TUI_STOP = 27,

    CMD_INIT_PMF = 28,
    CMD_REPLY_INIT_PMF = 29,
    CMD_FINI_PMF = 30,
    CMD_REPLY_FINI_PMF = 31,

    CMD_CAN_CLOSE = 32,
    CMD_REPLY_CAN_CLOSE = 33,

    CMD_MAX
};

struct tee_operation {
    uint32_t tee_param_types;
    struct entry_point_params tee_params[4];
} IW_STRUCTURE;

struct cmd_request {
    uint32_t cmd;
    uint32_t serial;
} IW_STRUCTURE;

struct cmd_reply {
    struct cmd_request base;
    uint32_t result;
    uint32_t origin;
} IW_STRUCTURE;

struct cmd_initialize_context {
    struct cmd_request base;
} IW_STRUCTURE;

struct cmd_reply_initialize_context {
    struct cmd_reply base;
    uint32_t ctx_id;
    uint32_t group_id;
} IW_STRUCTURE;

struct cmd_finalize_context {
    struct cmd_request base;
} IW_STRUCTURE;

struct cmd_reply_finalize_context {
    struct cmd_reply base;
} IW_STRUCTURE;

struct cmd_set_shared_memory_rights {
    struct cmd_request base;
    struct shared_buffer_description buf_desc;
} IW_STRUCTURE;

struct cmd_reply_set_shared_memory_rights {
    struct cmd_reply base;
} IW_STRUCTURE;

struct cmd_prepare_session {
    struct cmd_request base;
    tz_uuid_t ta_uuid;
    uint32_t ctx_id;
    struct shared_buffer_description buf_desc;
} IW_STRUCTURE;

struct cmd_prepare_session_reply {
    struct cmd_reply base;
} IW_STRUCTURE;

struct cmd_open_session {
    struct cmd_request base;
    uint32_t conn_meth;
    char conn_data[PROFILE_STR_LEN];
    uint32_t fips_enabled;
    struct tee_operation op;
    uint64_t cancel_time; /* < Number of nanoseconds since Epoch. */
} IW_STRUCTURE;

struct cmd_reply_open_session {
    struct cmd_reply base;
    tz_uuid_t session_id;
    struct tee_operation op;
} IW_STRUCTURE;

struct cmd_invoke_command {
    struct cmd_request base;
    uint32_t tee_cmd_id;
    struct tee_operation op;
    uint64_t cancel_time; /* < Number of nanoseconds since Epoch. */
} IW_STRUCTURE;

struct cmd_reply_invoke_command {
    struct cmd_reply base;
    struct tee_operation op;
} IW_STRUCTURE;

struct cmd_close_session {
    struct cmd_request base;
} IW_STRUCTURE;

struct cmd_reply_close_session {
    struct cmd_reply base;
} IW_STRUCTURE;

/* No reply message */
struct cmd_cancellation {
    struct cmd_request base;
    uint32_t op_serial;
} IW_STRUCTURE;

struct cmd_cancellation_root {
    struct cmd_request base;
    uint32_t reason;
} IW_STRUCTURE;

/* Use struct cmd_reply for reply message */
struct cmd_set_cluster {
    struct cmd_request base;
    uint32_t cluster;
} IW_STRUCTURE;

struct cmd_reply_set_cluster {
    struct cmd_reply base;
} IW_STRUCTURE;

struct cmd_open_tzdaemon_connection {
    struct cmd_request base;
} IW_STRUCTURE;

struct cmd_reply_open_tzdaemon_connection {
    struct cmd_reply base;
} IW_STRUCTURE;

struct cmd_ta_image {
    struct cmd_request base;
    char name[MAX_TA_NAME_LEN];
    char new_name[MAX_TA_NAME_LEN];
    tz_uuid_t uuid;
} IW_STRUCTURE;

struct cmd_reply_ta_image {
    struct cmd_reply base;
    uint32_t version;
    struct shared_buffer_description buf_desc;
} IW_STRUCTURE;

struct cmd_shutdown_ta {
    struct cmd_request base;
    tz_uuid_t uuid;
    uint32_t error_code;
} IW_STRUCTURE;

struct cmd_reply_shutdown_ta {
    struct cmd_reply base;
    uint32_t ta_in_progress;
} IW_STRUCTURE;

//TODO: remove this code when TEEGRIS-1646 get closed
struct cmd_unlock_tmf_ta {
    struct cmd_request base;
} IW_STRUCTURE;

struct cmd_reply_unlock_tmf_ta {
    struct cmd_reply base;
} IW_STRUCTURE;
//end TODO

struct cmd_sync_tee_state {
    struct cmd_request base;
} IW_STRUCTURE;

struct cmd_reply_sync_tee_state {
    struct cmd_reply base;
} IW_STRUCTURE;

struct cmd_request_tmf_parent_sd_uuid {
    struct cmd_request base;
    tz_uuid_t uuid; /* Currently, structs TEEC_UUID and uuid_t and tz_uuid_t are equal
                     * and already packed */
} IW_STRUCTURE;

struct cmd_reply_tmf_parent_sd_uuid {
    struct cmd_reply base;
    tz_uuid_t parent_uuid;
    uint32_t error_code;
} IW_STRUCTURE;

struct cmd_init_pmf {
    struct cmd_request base;
    int32_t pmf_shmem_id;
} IW_STRUCTURE;

struct cmd_reply_init_pmf {
    struct cmd_reply base;
} IW_STRUCTURE;

struct cmd_can_close {
    struct cmd_request base;
    uint32_t sockets_recieved_cnt;
} IW_STRUCTURE;

struct cmd_reply_can_close {
    struct cmd_reply base;
    uint8_t can_close;
} IW_STRUCTURE;

#endif /* __IW_MESSAGES_H__ */
