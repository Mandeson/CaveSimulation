#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"

SharedMemory *shared_memory;

void sigusr1_handler(int);

int main(void) {
    if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR
            || signal(SIGINT, SIG_IGN) == SIG_ERR)
        perror("Guard main: signal");

    shared_memory = attach_shared_memory();
    if (shared_memory == NULL)
        return -1;

    shared_memory->processes_starting--;

    printf("Guard: Enter '1' or '2' and press enter to send signal to Guide 1 or 2 "
            " (to cancel the tour)\n");

    while (1) {
        char c;
        read(0, &c, 1);
        if (c == '1') {
            printf("Guard: Sending signal to Guide 1\n");
            if (kill(shared_memory->guide1_pid, SIGUSR2) == -1)
                perror("Guard main: kill");
        } else if (c == '2') {
            printf("Guard: Sending signal to Guide 2\n");
            if (kill(shared_memory->guide2_pid, SIGUSR2) == -1)
                perror("Guard main: kill");
        }
    }

    if (shared_memory != NULL)
        detach_shared_memory(&shared_memory);
}

void sigusr1_handler(int sig) {
    (void)sig;

    if (shared_memory != NULL)
        detach_shared_memory(&shared_memory);

    exit(0);
}
