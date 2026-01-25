#include "guide.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

Guide guide;
volatile atomic_bool guide_destroying = false;

void sigusr1_handler(int);
void sigusr2_handler(int);

int main(void) {
    if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR
            || signal(SIGUSR2, sigusr2_handler) == SIG_ERR
            || signal(SIGINT, SIG_IGN) == SIG_ERR)
        perror("main: signal");

    GuideRes res = guide_init(&guide);
    if (res != GUIDE_SUCCESS)
        return res;

    res = guide_run(&guide);

    guide_destroying = true;

    GuideRes destroy_res = guide_destroy(&guide);

    if (res != GUIDE_SUCCESS)
        return res;
    
    if (destroy_res != GUIDE_SUCCESS)
        return destroy_res;
}

void sigusr1_handler(int sig) {
    (void)sig;

    if (guide_destroying)
        exit(0);
    GuideRes destroy_res = guide_destroy(&guide);
    exit(destroy_res);
}

void sigusr2_handler(int sig) {
    (void)sig;

    guide_signal(&guide);
}
