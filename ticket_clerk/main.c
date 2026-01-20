#include "ticket_clerk.h"
#include <signal.h>
#include <stdlib.h>

TicketClerk ticket_clerk;

void sigusr1_handler(int);

int main(void) {
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGINT, SIG_IGN);

    TicketClerkRes res = ticket_clerk_init(&ticket_clerk);
    if (res != TICKET_CLERK_SUCCESS)
        return res;

    res = ticket_clerk_run(&ticket_clerk);

    TicketClerkRes destroy_res = ticket_clerk_destroy(&ticket_clerk);

    if (res != TICKET_CLERK_SUCCESS)
        return res;

    if (destroy_res != TICKET_CLERK_SUCCESS)
        return res;
}

void sigusr1_handler(int sig) {
    (void)sig;

    TicketClerkRes destroy_res = ticket_clerk_destroy(&ticket_clerk);
    exit(destroy_res);
}
