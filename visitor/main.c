#include "visitor.h"
#include <signal.h>

Visitor visitor;

int main(void) {
    signal(SIGINT, SIG_IGN);

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
