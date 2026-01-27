#pragma once
#include <stdint.h>
#include <sys/types.h>
#include "ipc/shared.h"

#define MQ_KEY_REQ 0x12345
#define MQ_KEY_REP 0x12346

enum { MSG_SPAWN = 1 };

typedef struct {
    long mtype;          // MSG_SPAWN
    pid_t sender;        // BS pid
    unit_id_t sender_id;   // BS unit_id
    point_t pos;        // desired spawn coords
    unit_type_t utype;       // unit_type_t to spawn
    uint32_t req_id;     // optional: correlate replies
} mq_spawn_req_t;

typedef struct {
    long mtype;          // = sender pid (so BS can filter)
    uint32_t req_id;
    int16_t status;      // 0 ok, <0 fail
    pid_t child_pid;     // spawned squadron pid on success
} mq_spawn_rep_t;

int mq_try_recv_spawn(int qreq, mq_spawn_req_t *out);
int mq_send_spawn(int qreq, const mq_spawn_req_t *req);

int mq_send_reply(int qrep, const mq_spawn_rep_t *rep);
int mq_try_recv_reply(int qrep, mq_spawn_rep_t *out);