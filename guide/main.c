#include "guide.h"
#include <signal.h>
#include <stdlib.h>

Guide guide;

void sigusr1_handler(int);

int main(void) {
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGINT, SIG_IGN);

    GuideRes res = guide_init(&guide);
    if (res != GUIDE_SUCCESS)
        return res;

    res = guide_run(&guide);

    GuideRes destroy_res = guide_destroy(&guide);

    if (res != GUIDE_SUCCESS)
        return res;
    
    if (destroy_res != GUIDE_SUCCESS)
        return destroy_res;
}

void sigusr1_handler(int sig) {
    (void)sig;

    GuideRes destroy_res = guide_destroy(&guide);
    exit(destroy_res);
}
