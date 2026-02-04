#pragma once
#include <stdint.h>
#include <sys/types.h>
#include "ipc/shared.h"

#define MQ_KEY_REQ 0x12345
#define MQ_KEY_REP 0x12346
#define MQ_ORDER_MTYPE_OFFSET 100000

enum { MSG_SPAWN = 1, MSG_COMMANDER_REQ = 2, MSG_COMMANDER_REP = 3, MSG_DAMAGE = 4, MSG_ORDER = 5, MSG_CM_CMD = 6, MSG_UI_MAP_REQ = 7, MSG_UI_MAP_REP = 8 };

typedef enum {
    CM_CMD_FREEZE,
    CM_CMD_UNFREEZE,
    CM_CMD_TICKSPEED_GET,
    CM_CMD_TICKSPEED_SET,
    CM_CMD_SPAWN,
    CM_CMD_GRID,
    CM_CMD_END
} cm_command_type_t;

typedef struct {
    long mtype;          // MSG_SPAWN
    pid_t sender;        // BS/CM pid
    unit_id_t sender_id;   // BS unit_id (0 for CM)
    point_t pos;        // desired spawn coords
    unit_type_t utype;       // unit_type_t to spawn
    faction_t faction;   // faction for spawned unit (for CM requests)
    uint32_t req_id;     // optional: correlate replies
    unit_id_t commander_id;  // BS unit_id to assign as commander (0 for CM)
} mq_spawn_req_t;

typedef struct {
    long mtype;          // = sender pid (so BS can filter)
    uint32_t req_id;
    int16_t status;      // 0 ok, <0 fail
    pid_t child_pid;     // spawned squadron pid on success
    unit_id_t child_unit_id; // spawned squadron unit_id on success
} mq_spawn_rep_t;

typedef struct {
    long mtype;          // MSG_COMMANDER_REQ
    pid_t sender;        // squadron pid
    unit_id_t sender_id; // squadron unit_id
    uint32_t req_id;     // correlation id
} mq_commander_req_t;

typedef struct {
    long mtype;          // = sender pid (so squadron can filter)
    uint32_t req_id;
    int16_t status;      // 0 accepted, <0 rejected
    unit_id_t commander_id; // battleship unit_id on success
} mq_commander_rep_t;

typedef struct {
    long mtype;          // = target pid
    unit_id_t target_id; // unit receiving damage
    st_points_t damage;  // damage amount
} mq_damage_t;

typedef struct {
    long mtype;          // = target squadron pid
    unit_order_t order;  // order type (PATROL, ATTACK, GUARD, etc.)
    unit_id_t target_id; // target for ATTACK/GUARD orders
} mq_order_t;

typedef struct {
    long mtype;           // MSG_CM_CMD
    cm_command_type_t cmd; // command type
    pid_t sender;         // CM pid
    uint32_t req_id;      // correlation id
    int32_t tick_speed_ms; // for TICKSPEED_SET command
    int32_t grid_enabled;  // for GRID command: -1=query, 0=off, 1=on
    /* Spawn parameters */
    unit_type_t spawn_type;   // unit type to spawn
    faction_t spawn_faction;  // faction
    int16_t spawn_x;          // x coordinate
    int16_t spawn_y;          // y coordinate
} mq_cm_cmd_t;

typedef struct {
    long mtype;           // = sender pid (so CM can filter)
    uint32_t req_id;      // correlation id
    int16_t status;       // 0 ok, <0 fail
    char message[128];    // status message
    int32_t tick_speed_ms; // for TICKSPEED_GET response
    int32_t grid_enabled;  // for GRID query response
} mq_cm_rep_t;

int mq_try_recv_spawn(int qreq, mq_spawn_req_t *out);
int mq_send_spawn(int qreq, const mq_spawn_req_t *req);

int mq_send_reply(int qrep, const mq_spawn_rep_t *rep);
int mq_try_recv_reply(int qrep, mq_spawn_rep_t *out);

int mq_try_recv_commander_req(int qreq, mq_commander_req_t *out);
int mq_send_commander_req(int qreq, const mq_commander_req_t *req);
int mq_send_commander_reply(int qrep, const mq_commander_rep_t *rep);
int mq_try_recv_commander_reply(int qrep, mq_commander_rep_t *out);

int mq_send_damage(int qreq, const mq_damage_t *dmg);
int mq_try_recv_damage(int qreq, mq_damage_t *out);

int mq_send_order(int qreq, const mq_order_t *order);
int mq_try_recv_order(int qreq, mq_order_t *out);

int mq_send_cm_cmd(int qreq, const mq_cm_cmd_t *cmd);
int mq_try_recv_cm_cmd(int qreq, mq_cm_cmd_t *out);
int mq_send_cm_reply(int qrep, const mq_cm_rep_t *rep);
int mq_try_recv_cm_reply(int qrep, mq_cm_rep_t *out);
int mq_recv_cm_reply_blocking(int qrep, mq_cm_rep_t *out);

/* UI Map snapshot request/response */
typedef struct {
    long mtype;          // MSG_UI_MAP_REQ
    pid_t sender;        // UI pid
} mq_ui_map_req_t;

typedef struct {
    long mtype;          // MSG_UI_MAP_REP
    uint32_t tick;
    int ready;           // 1 = grid snapshot ready in shared memory
} mq_ui_map_rep_t;

int mq_send_ui_map_req(int qreq, const mq_ui_map_req_t *req);
int mq_try_recv_ui_map_req(int qreq, mq_ui_map_req_t *out);
int mq_send_ui_map_rep(int qrep, const mq_ui_map_rep_t *rep);
int mq_recv_ui_map_rep_blocking(int qrep, mq_ui_map_rep_t *out);