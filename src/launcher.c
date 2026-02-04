#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include "error_handler.h"

/* Space Skirmish Launcher
 *
 * This launcher spawns both Command Center (CC) and Console Manager (CM)
 * processes at the same level, creating the structure:
 *
 * ├─Launcher
 *   ├─CC (command_center)
 *   │  ├─BS (battleship)
 *   │  └─SQ (squadron)
 *   └─CM (console_manager)
 *
 * Both CC and CM communicate via message queues.
 */

static volatile sig_atomic_t g_stop = 0;
static pid_t cc_pid = -1;
static pid_t cm_pid = -1;

/* Signal handler */
static void on_term(int sig) {
    (void)sig;
    printf("\n[Launcher] Received signal, shutting down...\n");
    g_stop = 1;
    
    /* Forward signal to children */
    if (cc_pid > 0) kill(cc_pid, SIGTERM);
    if (cm_pid > 0) kill(cm_pid, SIGTERM);
}

int main(int argc, char **argv) {
    const char *cc_path = "./command_center";
    const char *cm_path = "./console_manager";
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cc") == 0 && i+1 < argc) {
            cc_path = argv[++i];
        } else if (strcmp(argv[i], "--cm") == 0 && i+1 < argc) {
            cm_path = argv[++i];
        }
    }
    
    /* Setup signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_term;
    if (CHECK_SYS_CALL_NONFATAL(sigaction(SIGINT, &sa, NULL), "launcher:sigaction_SIGINT") == -1) {
        return 1;
    }
    if (CHECK_SYS_CALL_NONFATAL(sigaction(SIGTERM, &sa, NULL), "launcher:sigaction_SIGTERM") == -1) {
        return 1;
    }
    
    printf("[Launcher] Starting Space Skirmish...\n");
    printf("[Launcher] CC path: %s\n", cc_path);
    printf("[Launcher] CM path: %s\n", cm_path);
    
    /* Spawn Command Center */
    cc_pid = fork();
    if (cc_pid == -1) {
        HANDLE_SYS_ERROR("launcher:fork_CC", "Failed to fork Command Center");
    }
    
    if (cc_pid == 0) {
        /* Child process - exec CC */
        execl(cc_path, cc_path, NULL);
        /* If execl returns, it failed */
        HANDLE_SYS_ERROR("launcher:execl_CC", "Failed to execute Command Center");
    }
    
    printf("[Launcher] Command Center started (pid=%d)\n", cc_pid);
    
    /* Give CC time to initialize IPC */
    sleep(1);
    
    /* Spawn Console Manager */
    cm_pid = fork();
    if (cm_pid == -1) {
        HANDLE_SYS_ERROR_NONFATAL("launcher:fork_CM", "Failed to fork Console Manager");
        /* Kill CC since CM failed */
        CHECK_SYS_CALL_NONFATAL(kill(cc_pid, SIGTERM), "launcher:kill_CC_after_CM_fail");
        CHECK_SYS_CALL_NONFATAL(waitpid(cc_pid, NULL, 0), "launcher:waitpid_CC");
        return 1;
    }
    
    if (cm_pid == 0) {
        /* Child process - exec CM */
        execl(cm_path, cm_path, NULL);
        /* If execl returns, it failed */
        HANDLE_SYS_ERROR("launcher:execl_CM", "Failed to execute Console Manager");
    }
    
    printf("[Launcher] Console Manager started (pid=%d)\n", cm_pid);
    printf("[Launcher] Both processes running. Press Ctrl+C to stop.\n");
    
    /* Wait for children */
    int status;
    pid_t exited_pid;
    int cc_done = 0, cm_done = 0;
    
    while (!g_stop && (!cc_done || !cm_done)) {
        exited_pid = waitpid(-1, &status, 0);
        
        if (exited_pid == -1) {
            if (errno == EINTR) {
                continue;  /* Interrupted by signal */
            }
            if (errno == ECHILD) {
                break;  /* No more children */
            }
            perror("[Launcher] waitpid");
            fprintf(stderr, "[Launcher] waitpid error: %s (errno=%d)\n", 
                    strerror(errno), errno);
            break;
        }
        
        if (exited_pid == cc_pid) {
            if (WIFEXITED(status)) {
                printf("[Launcher] Command Center exited with status %d\n", 
                       WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[Launcher] Command Center killed by signal %d\n", 
                       WTERMSIG(status));
            }
            cc_done = 1;
            cc_pid = -1;
            
            /* If CC dies, kill CM */
            if (cm_pid > 0) {
                printf("[Launcher] CC died, terminating CM...\n");
                kill(cm_pid, SIGTERM);
            }
        }
        else if (exited_pid == cm_pid) {
            if (WIFEXITED(status)) {
                printf("[Launcher] Console Manager exited with status %d\n", 
                       WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("[Launcher] Console Manager killed by signal %d\n", 
                       WTERMSIG(status));
            }
            cm_done = 1;
            cm_pid = -1;
            
            /* If CM exits normally, it's probably user choice, so continue CC */
            /* But if g_stop is set, we're shutting down anyway */
        }
    }
    
    /* Cleanup - ensure both children are terminated */
    if (cc_pid > 0) {
        printf("[Launcher] Terminating CC...\n");
        if (kill(cc_pid, SIGTERM) == -1) {
            perror("[Launcher] kill CC in cleanup");
        }
        if (waitpid(cc_pid, NULL, 0) == -1) {
            perror("[Launcher] waitpid CC in cleanup");
        }
    }
    
    if (cm_pid > 0) {
        printf("[Launcher] Terminating CM...\n");
        if (kill(cm_pid, SIGTERM) == -1) {
            perror("[Launcher] kill CM in cleanup");
        }
        if (waitpid(cm_pid, NULL, 0) == -1) {
            perror("[Launcher] waitpid CM in cleanup");
        }
    }
    
    printf("[Launcher] Shutdown complete.\n");
    return 0;
}
