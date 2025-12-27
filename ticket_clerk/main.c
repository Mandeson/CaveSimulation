#include "ticket_clerk.h"
#include <signal.h>

TicketClerk ticket_clerk;

int main(void) {
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
