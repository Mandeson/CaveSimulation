#include "visitor.h"
#include <signal.h>
#include <stdlib.h>

Visitor visitor;

void sigint_handler(int);

int main(void) {
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    VisitorRes res = visitor_init(&visitor);
    if (res != VISITOR_SUCCESS)
        return res;

    res = visitor_run(&visitor);

    VisitorRes destroy_res = visitor_destroy(&visitor);

    if (res != VISITOR_SUCCESS)
        return res;
    
    if (destroy_res != VISITOR_SUCCESS)
        return destroy_res;
}

void sigint_handler(int sig) {
    (void)sig;

    VisitorRes destroy_res = visitor_destroy(&visitor);
    exit(destroy_res);
}
