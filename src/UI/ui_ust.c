// UI UST thread - displays unit statistics
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>

#include "UI/ui.h"
#include "UI/ui_ust.h"
#include "ipc/shared.h"
#include "ipc/semaphores.h"
#include "log.h"

/* Color pairs for factions */
#define COLOR_REPUBLIC  1
#define COLOR_CIS       2

/* Helper to get unit type name */
static const char* get_type_name(uint8_t type) {
    switch (type) {
        case TYPE_FLAGSHIP:  return "Flagship";
        case TYPE_DESTROYER: return "Destroyer";
        case TYPE_CARRIER:   return "Carrier";
        case TYPE_FIGHTER:   return "Fighter";
        case TYPE_BOMBER:    return "Bomber";
        case TYPE_ELITE:     return "Elite";
        default:             return "Unknown";
    }
}

/* Helper to get faction name */
static const char* get_faction_name(uint8_t faction) {
    switch (faction) {
        case FACTION_REPUBLIC: return "Republic";
        case FACTION_CIS:      return "CIS";
        default:               return "None";
    }
}

/* Render unit statistics table */
static void render_ust(ui_context_t *ui_ctx) {
    pthread_mutex_lock(&ui_ctx->ui_lock);
    
    WINDOW *win = ui_ctx->ust_win;
    if (!win) {
        pthread_mutex_unlock(&ui_ctx->ui_lock);
        return;
    }
    
    int win_h, win_w;
    getmaxyx(win, win_h, win_w);
    
    /* Clear window content */
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " UNIT STATS ");
    
    /* Lock shared memory and read unit data */
    if (sem_lock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK) != 0) {
        mvwprintw(win, 1, 1, "Failed to lock shared memory");
        wrefresh(win);
        pthread_mutex_unlock(&ui_ctx->ui_lock);
        return;
    }
    
    uint16_t unit_count = ui_ctx->ctx->S->unit_count;
    uint32_t tick = ui_ctx->ctx->S->ticks;
    
    /* Copy unit data */
    unit_entity_t units[MAX_UNITS+1];
    memcpy(units, ui_ctx->ctx->S->units, sizeof(units));
    
    sem_unlock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
    
    /* Display header */
    mvwprintw(win, 0, win_w - 15, " Tick:%u ", tick);
    
    /* Table header */
    int row = 1;
    if (win_h > 2 && win_w > 40) {
        wattron(win, A_BOLD);
        mvwprintw(win, row++, 1, "ID Type       Faction   HP    Pos      PID");
        wattroff(win, A_BOLD);
    }
    
    /* Display units */
    int alive_count = 0;
    for (int i = 1; i <= MAX_UNITS && row < win_h - 1; i++) {
        if (units[i].alive) {
            alive_count++;
            
            /* Calculate HP percentage (assume max HP = some value, or just show damage) */
            int hp_pct = 100;  // TODO: calculate based on max HP
            
            /* Color based on faction */
            if (units[i].faction == FACTION_REPUBLIC) {
                wattron(win, COLOR_PAIR(COLOR_REPUBLIC));
            } else if (units[i].faction == FACTION_CIS) {
                wattron(win, COLOR_PAIR(COLOR_CIS));
            }
            
            /* Format: ID Type Faction HP Pos PID */
            mvwprintw(win, row++, 1, "%-2d %-10s %-9s %3d%% (%3d,%3d) %d",
                      i,
                      get_type_name(units[i].type),
                      get_faction_name(units[i].faction),
                      hp_pct,
                      units[i].position.x,
                      units[i].position.y,
                      units[i].pid);
            
            if (units[i].faction == FACTION_REPUBLIC || units[i].faction == FACTION_CIS) {
                wattroff(win, COLOR_PAIR(units[i].faction));
            }
        }
    }
    
    /* Show summary at bottom */
    if (row < win_h - 1) {
        mvwprintw(win, win_h - 2, 1, "Total: %d/%d alive", alive_count, unit_count);
    }
    
    wrefresh(win);
    pthread_mutex_unlock(&ui_ctx->ui_lock);
}

void* ui_ust_thread(void* arg) {
    ui_context_t *ui_ctx = (ui_context_t*)arg;
    
    LOGI("[UI-UST] Thread started");
    
    /* Display loop */
    while (!ui_ctx->stop) {
        render_ust(ui_ctx);
        
        /* Update every 500ms (2 Hz) */
        //usleep(500000);
    }
    
    LOGI("[UI-UST] Thread exiting");
    return NULL;
}
