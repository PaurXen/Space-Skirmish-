#include "ipc/ipc_mesq.h"
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

static inline int mq_open_or_create(key_t key) {
    return msgget(key, 0666 | IPC_CREAT);
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
