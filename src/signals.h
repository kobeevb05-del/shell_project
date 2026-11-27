#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>

/* устанавливает SIGCHLD(завершение дочернего процесса) 
для сбора завершившихся фоновых процессов */
int signals_init_background(void);

//  временно блокирует SIGCHLD и сохраняет прежнюю маску в old_mask
int signals_block_chld(sigset_t *old_mask);

//  восстанавливает прежнюю маску сигналов из old_mask
int signals_restore_mask(const sigset_t *old_mask);

#endif