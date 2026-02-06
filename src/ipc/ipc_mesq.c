#include "ipc/ipc_mesq.h"
#include "error_handler.h"
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>

static inline int mq_open_or_create(key_t key) {
    int qid = msgget(key, 0600 | IPC_CREAT);
    if (qid == -1) {
        perror("[IPC] msgget");
        fprintf(stderr, "[IPC] Failed to open/create message queue (key=0x%x): %s (errno=%d)\n",
                key, strerror(errno), errno);
    }
    return qid;
}

int mq_req_id(void) { return mq_open_or_create(MQ_KEY_REQ); }
int mq_rep_id(void) { return mq_open_or_create(MQ_KEY_REP); }

int mq_try_recv_spawn(int qreq, mq_spawn_req_t *out) {
    ssize_t n = msgrcv(qreq, out, sizeof(*out) - sizeof(long), MSG_SPAWN, IPC_NOWAIT);
    if (n < 0 && errno == ENOMSG) return 0;   // nothing
    return (n < 0) ? -1 : 1;                  // -1 error, 1 got msg
}

int mq_send_spawn(int qreq, const mq_spawn_req_t *req) {
    return msgsnd(qreq, req, sizeof(*req) - sizeof(long), IPC_NOWAIT);
}

int mq_send_reply(int qrep, const mq_spawn_rep_t *rep) {
    return msgsnd(qrep, rep, sizeof(*rep) - sizeof(long), IPC_NOWAIT);
}

int mq_try_recv_reply(int qrep, mq_spawn_rep_t *out) {
    pid_t me = getpid();
    ssize_t n = msgrcv(qrep, out, sizeof(*out) - sizeof(long), me, IPC_NOWAIT);
    if (n < 0 && errno == ENOMSG) return 0;
    return (n < 0) ? -1 : 1;
}

int mq_try_recv_commander_req(int qreq, mq_commander_req_t *out) {
    ssize_t n = msgrcv(qreq, out, sizeof(*out) - sizeof(long), MSG_COMMANDER_REQ, IPC_NOWAIT);
    if (n < 0 && errno == ENOMSG) return 0;
    return (n < 0) ? -1 : 1;
}

int mq_send_commander_req(int qreq, const mq_commander_req_t *req) {
    return msgsnd(qreq, req, sizeof(*req) - sizeof(long), IPC_NOWAIT);
}

int mq_send_commander_reply(int qrep, const mq_commander_rep_t *rep) {
    return msgsnd(qrep, rep, sizeof(*rep) - sizeof(long), IPC_NOWAIT);
}

int mq_try_recv_commander_reply(int qrep, mq_commander_rep_t *out) {
    pid_t me = getpid();
    ssize_t n = msgrcv(qrep, out, sizeof(*out) - sizeof(long), me, IPC_NOWAIT);
    if (n < 0 && errno == ENOMSG) return 0;
    return (n < 0) ? -1 : 1;
}

int mq_send_damage(int qreq, const mq_damage_t *dmg) {
    return msgsnd(qreq, dmg, sizeof(*dmg) - sizeof(long), IPC_NOWAIT);
}

int mq_try_recv_damage(int qreq, mq_damage_t *out) {
    pid_t me = getpid();
    ssize_t n = msgrcv(qreq, out, sizeof(*out) - sizeof(long), me, IPC_NOWAIT);
    if (n < 0 && errno == ENOMSG) return 0;
    return (n < 0) ? -1 : 1;
}

int mq_send_order(int qreq, const mq_order_t *order) {
    mq_order_t msg = *order;
    msg.mtype += MQ_ORDER_MTYPE_OFFSET;
    return msgsnd(qreq, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
}

int mq_try_recv_order(int qreq, mq_order_t *out) {
    pid_t me = getpid();
    ssize_t n = msgrcv(qreq, out, sizeof(*out) - sizeof(long), me + MQ_ORDER_MTYPE_OFFSET, IPC_NOWAIT);
    if (n < 0 && errno == ENOMSG) return 0;
    if (n > 0) {
        out->mtype -= MQ_ORDER_MTYPE_OFFSET;
    }
    return (n < 0) ? -1 : 1;
}

int mq_send_cm_cmd(int qreq, const mq_cm_cmd_t *cmd) {
    return msgsnd(qreq, cmd, sizeof(*cmd) - sizeof(long), IPC_NOWAIT);
}

int mq_try_recv_cm_cmd(int qreq, mq_cm_cmd_t *out) {
    ssize_t n = msgrcv(qreq, out, sizeof(*out) - sizeof(long), MSG_CM_CMD, IPC_NOWAIT);
    if (n < 0 && errno == ENOMSG) return 0;
    return (n < 0) ? -1 : 1;
}

int mq_send_cm_reply(int qrep, const mq_cm_rep_t *rep) {
    return msgsnd(qrep, rep, sizeof(*rep) - sizeof(long), IPC_NOWAIT);
}

int mq_try_recv_cm_reply(int qrep, mq_cm_rep_t *out) {
    pid_t me = getpid();
    ssize_t n = msgrcv(qrep, out, sizeof(*out) - sizeof(long), me, IPC_NOWAIT);
    if (n < 0 && errno == ENOMSG) return 0;
    return (n < 0) ? -1 : 1;
}

int mq_recv_cm_reply_blocking(int qrep, mq_cm_rep_t *out) {
    pid_t me = getpid();
    ssize_t n = msgrcv(qrep, out, sizeof(*out) - sizeof(long), me, 0);
    return (n < 0) ? -1 : 1;
}

/* UI Map snapshot request/response */
int mq_send_ui_map_req(int qreq, const mq_ui_map_req_t *req) {
    return msgsnd(qreq, req, sizeof(*req) - sizeof(long), IPC_NOWAIT);
}

int mq_try_recv_ui_map_req(int qreq, mq_ui_map_req_t *out) {
    ssize_t n = msgrcv(qreq, out, sizeof(*out) - sizeof(long), MSG_UI_MAP_REQ, IPC_NOWAIT);
    if (n < 0 && errno == ENOMSG) return 0;
    return (n < 0) ? -1 : 1;
}

int mq_send_ui_map_rep(int qrep, const mq_ui_map_rep_t *rep) {
    return msgsnd(qrep, rep, sizeof(*rep) - sizeof(long), IPC_NOWAIT);
}

int mq_recv_ui_map_rep_blocking(int qrep, mq_ui_map_rep_t *out) {
    pid_t me = getpid();
    ssize_t n = msgrcv(qrep, out, sizeof(*out) - sizeof(long), me, 0);
    return (n < 0) ? -1 : 1;
}
