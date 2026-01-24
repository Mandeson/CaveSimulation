#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"

SharedMemory *shared_memory;

void sigusr1_handler(int);

int main(void) {
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGINT, SIG_IGN);

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
            kill(shared_memory->guide1_pid, SIGUSR2);
        } else if (c == '2') {
            printf("Guard: Sending signal to Guide 2\n");
            kill(shared_memory->guide2_pid, SIGUSR2);
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
