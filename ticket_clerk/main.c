#include "ticket_clerk.h"

TicketClerk ticket_clerk;

int main(void) {
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
