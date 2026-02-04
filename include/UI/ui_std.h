#pragma once

#include "UI/ui.h"

/* STD thread - displays standard output from command center via tee FIFO */
void* ui_std_thread(void* arg);
