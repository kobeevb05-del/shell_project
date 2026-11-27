#include "signals.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

static int sig_error(const char *name)
{
    fprintf(stderr, "mysh: %s: %s\n", name, strerror(errno));
    return 1;
}

// подбирает завершившиеся фоновые процессы, чтобы не оставались зомби
static void reap_background(int sig)
{
    (void)sig;
    int saved_errno = errno;

    for (;;) {
        pid_t pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0) {
            continue;
        }
        if (pid == -1 && errno == EINTR) {
            continue;
        }
        break;
    }

    errno = saved_errno;
}

// SIGCHLD для автоматического удаления фоновых процессов
int signals_init_background(void)
{
    struct sigaction action = {0};
    action.sa_handler = reap_background;
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGCHLD, &action, NULL) == -1) {
        return sig_error("sigaction");
    }
    return 0;
}

// временно блокирует SIGCHLD, чтобы не перехватить статус нефоновой команды
int signals_block_chld(sigset_t *old_mask)
{
    sigset_t chld_mask;
    if (sigemptyset(&chld_mask) == -1 ||
        sigaddset(&chld_mask, SIGCHLD) == -1 ||
        sigprocmask(SIG_BLOCK, &chld_mask, old_mask) == -1) {
        return sig_error("sigprocmask");
    }
    return 0;
}

// возврат маски
int signals_restore_mask(const sigset_t *old_mask)
{
    if (sigprocmask(SIG_SETMASK, old_mask, NULL) == -1) {
        return sig_error("sigprocmask");
    }
    return 0;
}