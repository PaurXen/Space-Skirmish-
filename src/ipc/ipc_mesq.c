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
