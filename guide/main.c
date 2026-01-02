#include "guide.h"
#include <signal.h>

Guide guide;

int main(void) {
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
