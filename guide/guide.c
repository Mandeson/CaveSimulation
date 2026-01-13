#include "guide.h"
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"
#include "util/array.h"

GuideRes guide_init(Guide *guide) {
    array_create(&guide->trail_visitors, sizeof(int));

    SharedMemory *shared_memory = attach_shared_memory();
    if (shared_memory == NULL)
        return GUIDE_INIT_FAIL;
    guide->shared_memory = shared_memory;
    
    int message_queue = get_message_queue();
    if (message_queue == -1) {
        detach_shared_memory(shared_memory);
        return GUIDE_INIT_FAIL;
    }
    guide->message_queue = message_queue;

    int semaphores = get_semaphores();
    if (semaphores == -1) {
        detach_shared_memory(shared_memory);
        return GUIDE_INIT_FAIL;
    }
    guide->semaphores = semaphores;

    take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
    guide->number = (getpid() == guide->shared_memory->guide2_pid) ? 1 : 0;
    give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

    return GUIDE_SUCCESS;
}

GuideRes guide_destroy(Guide *guide) {
    if (detach_shared_memory(guide->shared_memory) == -1)
        return GUIDE_DESTROY_FAIL;

    array_destroy(&guide->trail_visitors);

    return GUIDE_SUCCESS;
}

static int receive_message(Guide *guide) {
    Message message;
    int res = msgrcv(guide->message_queue, (void *)&message, sizeof(message.mtext), getpid(),
            IPC_NOWAIT);
    if (res == -1) {
        if (errno != ENOMSG) {
            perror("receive_message: msgrcv");
            return -1;
        }
        return 0;
    } else if (res != sizeof(message.mtext)) {
        output_log(guide->semaphores, guide->shared_memory,
                "receive_message: wrong number of bytes received from message queue");
        return -1;
    } else {
        if (strcmp(message.mtext, "terminate") == 0) {
            take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
            guide->shared_memory->guides_finished++;
            give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        } else {
            VisitorGuideMessage visitor_guide_message;
            memcpy(&visitor_guide_message, message.mtext, sizeof(visitor_guide_message));

            output_log(guide->semaphores, guide->shared_memory,
                    "Guide of trail %d greets visitor (PID: %d, children: %d)", guide->number + 1,
                    visitor_guide_message.pid, visitor_guide_message.children_count);

            guide->trail_visitors_count += 1 + visitor_guide_message.children_count;
            int *new_element = array_add_empty(&guide->trail_visitors);
            *new_element = visitor_guide_message.pid;
        }

        return 1;
    }
}

void open_gate(Guide *guide) {
    int trail_semaphore = (guide->number == 1) ? TRAIL2_SEMAPHORE : TRAIL1_SEMAPHORE;
    give_semaphore_n(guide->semaphores, trail_semaphore, guide->shared_memory->N[guide->number]);
}

void guide_tour(Guide *guide, size_t person_space, void *buffer) {
    output_log(guide->semaphores, guide->shared_memory,
            "Guide of trail %d is starting the tour", guide->number + 1);

    take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
    guide->shared_memory->guides_using_catwalks--;
    if (guide->shared_memory->guides_using_catwalks == 0)
        give_semaphore(guide->semaphores, CATWALK_DIRECTION_SEMAPHORE);
    give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

    for (size_t i = 0; i < guide->trail_visitors.size; i++) {
        int visitor_pid = ((int *)guide->trail_visitors.ptr)[i];
        Message message = {0};
        message.mtype = visitor_pid;
        
        if (msgsnd(guide->message_queue, &message, sizeof(message.mtext), 0) == -1)
            perror("guide_run: msgsnd (Visitor)");
    }

    usleep(guide->shared_memory->T[guide->number] * 1000);

    take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

    // If there are no more visitors
    if (guide->shared_memory->guides_finished == 2) {
        give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        return;
    }

    if (guide->shared_memory->guides_using_catwalks != 0
            && guide->shared_memory->catwalk_direction == IN) {
        while (semctl(guide->semaphores, CATWALK_DIRECTION_SEMAPHORE, GETVAL) > 0)
            take_semaphore(guide->semaphores, CATWALK_DIRECTION_SEMAPHORE);
        output_log(guide->semaphores, guide->shared_memory,
            "Guide of trail %d wait1, sem: %d", guide->number + 1, 
        semctl(guide->semaphores, CATWALK_DIRECTION_SEMAPHORE, GETVAL));
        give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        take_semaphore(guide->semaphores, CATWALK_DIRECTION_SEMAPHORE);
        take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        output_log(guide->semaphores, guide->shared_memory,
            "Guide of trail %d end wait1", guide->number + 1);
    }
    guide->shared_memory->catwalk_direction = OUT;
    guide->shared_memory->guides_using_catwalks++;

    for (size_t i = 0; i < guide->trail_visitors.size; i++) {
        int visitor_pid = ((int *)guide->trail_visitors.ptr)[i];
        Message message = {0};
        message.mtype = visitor_pid;
        
        if (msgsnd(guide->message_queue, &message, sizeof(message.mtext), 0) == -1)
            perror("guide_run: msgsnd (Visitor)");
    }

    output_log(guide->semaphores, guide->shared_memory,
            "Guide of trail %d started reading from pipe", guide->number + 1);

    give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

    for (size_t i = 0; i < guide->trail_visitors.size; i++) {
        take_semaphore(guide->semaphores, PIPE_READ_SEMAPHORE);
        take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        volatile int *catwalk_visitors = guide->shared_memory->catwalk_visitors;
        int catwalk_number = (catwalk_visitors[1] > catwalk_visitors[0]) ? 1 : 0;
        int catwalk_pipe = guide->shared_memory->catwalk_pipe[catwalk_number][0];
        catwalk_visitors[catwalk_number]--;
        give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

        output_log(guide->semaphores, guide->shared_memory,
            "Reading catwalk %d", catwalk_number);

        int nbytes = 0;
        if (ioctl(catwalk_pipe, FIONREAD, &nbytes) == -1) {
            perror("ioctl(FIONREAD)");
        } else {
            output_log(guide->semaphores, guide->shared_memory,
            "Bytes available %d", nbytes);
        }

        int res = read(catwalk_pipe, buffer, person_space);
        output_log(guide->semaphores, guide->shared_memory,
            "Read %d", res);
        if (res == -1) {
            perror("guide_tour: read");
            break;
        }
        if ((size_t)res != person_space) {
            output_log(guide->semaphores, guide->shared_memory,
                    "Guide error: pipe read size mismatch");
        } else {
            VisitorOnCatwalk visitor_on_catwalk;
                memcpy(&visitor_on_catwalk, buffer, sizeof(VisitorOnCatwalk));
                output_log(guide->semaphores, guide->shared_memory,
                        "Guide %d has read PID: %d from the pipe", guide->number + 1, visitor_on_catwalk.pid);

                for (int i = 0; i < visitor_on_catwalk.children_count; i++) {
                    // give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
                    int res = read(catwalk_pipe, buffer, person_space);
                    // take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
                    if (res == -1) {
                        perror("guide_run: read");
                        give_semaphore(guide->semaphores, PIPE_READ_SEMAPHORE);
                        give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
                        return;
                    }
                    catwalk_visitors[catwalk_number] -= 1;
                    VisitorOnCatwalk tmp;
                    memcpy(&tmp, buffer, sizeof(VisitorOnCatwalk));
                    output_log(guide->semaphores, guide->shared_memory,
                        "Guide %d has read PID: %d (child) from the pipe", guide->number + 1, tmp.pid);
                }
        }

        give_semaphore(guide->semaphores, PIPE_READ_SEMAPHORE);
    }

    output_log(guide->semaphores, guide->shared_memory,
            "Guide of trail %d finished reading from pipe", guide->number + 1);

    guide->shared_memory->guides_using_catwalks--;
    if (guide->shared_memory->guides_using_catwalks == 0)
        give_semaphore(guide->semaphores, CATWALK_IN_SEMAPHORE);
    else {
        while (semctl(guide->semaphores, CATWALK_IN_SEMAPHORE, GETVAL) > 0)
            take_semaphore(guide->semaphores, CATWALK_IN_SEMAPHORE);
        output_log(guide->semaphores, guide->shared_memory,
            "Guide of trail %d wait2, sem: %d", guide->number + 1, 
        semctl(guide->semaphores, CATWALK_IN_SEMAPHORE, GETVAL));
        give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        take_semaphore(guide->semaphores, CATWALK_IN_SEMAPHORE);
        take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        output_log(guide->semaphores, guide->shared_memory,
            "Guide of trail %d end wait2", guide->number + 1);
    }
    guide->shared_memory->catwalk_direction = IN;
    guide->shared_memory->guides_using_catwalks++;
    give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

    array_clear(&guide->trail_visitors);
    guide->trail_visitors_count = 0;

    output_log(guide->semaphores, guide->shared_memory,
            "Guide of trail %d opening the gate", guide->number + 1);

    open_gate(guide);
}

GuideRes guide_run(Guide *guide) {
    pid_t pid = getpid();
    output_log(guide->semaphores, guide->shared_memory,
            "Running guide (PID: %d, guide nr: %d)", pid, guide->number + 1);
    
    size_t person_space = PIPE_BUF / guide->shared_memory->K;
    void *buffer = malloc(person_space);

    open_gate(guide);

    bool terminate = false;
    int timeout_counter = 0;
    do {
        int sleep_time = 1;
        usleep(sleep_time * 1000);
        timeout_counter += sleep_time;

        take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

        if (guide->shared_memory->guides_finished == 2)
            terminate = true;

        bool tour_start = false;
        int timeout = (guide->shared_memory->T[guide->number]);
        if (guide->trail_visitors_count == guide->shared_memory->N[guide->number]
                    || (guide->shared_memory->terminating && timeout_counter >= timeout)) {
            int trail_semaphore = (guide->number == 1) ? TRAIL2_SEMAPHORE : TRAIL1_SEMAPHORE;
            set_semaphore(guide->semaphores, trail_semaphore, 0);
            tour_start = true;
        }

        // int trail_semaphore = (guide->number == 1) ? TRAIL2_SEMAPHORE : TRAIL1_SEMAPHORE;

        // output_log(guide->semaphores, guide->shared_memory,
        //     "vv %d %d %d", guide->trail_visitors_count, guide->shared_memory->visitors_approaching_catwalk,
        // semctl(guide->semaphores, trail_semaphore, GETVAL));

        take_semaphore(guide->semaphores, PIPE_READ_SEMAPHORE);

        volatile int *catwalk_visitors = guide->shared_memory->catwalk_visitors;
        int catwalk_number = (catwalk_visitors[1] > catwalk_visitors[0]) ? 1 : 0;
        bool empty = (catwalk_visitors[0] == 0) && (catwalk_visitors[1] == 0);

        if(empty) {
            give_semaphore(guide->semaphores, PIPE_READ_SEMAPHORE);
            give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        } else {
            int catwalk_pipe = guide->shared_memory->catwalk_pipe[catwalk_number][0];
            output_log(guide->semaphores, guide->shared_memory,
                        "R %d %d %d %d", guide->number, catwalk_visitors[0], catwalk_visitors[1], catwalk_number);
            int res = read(catwalk_pipe, buffer, person_space);
            catwalk_visitors[catwalk_number] -= 1;
            output_log(guide->semaphores, guide->shared_memory,
                        "RF %d %d %d %d", guide->number, catwalk_visitors[0], catwalk_visitors[1], catwalk_number);

            if (res == -1) {
                perror("guide_run: read");
                give_semaphore(guide->semaphores, PIPE_READ_SEMAPHORE);
                give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
                free(buffer);
                return GUIDE_RUN_FAIL;
            }
            if ((size_t)res != person_space) {
                output_log(guide->semaphores, guide->shared_memory,
                        "Guide error: pipe read size mismatch");
                give_semaphore(guide->semaphores, PIPE_READ_SEMAPHORE);
                give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
            } else {
                VisitorOnCatwalk visitor_on_catwalk;
                memcpy(&visitor_on_catwalk, buffer, sizeof(VisitorOnCatwalk));
                output_log(guide->semaphores, guide->shared_memory,
                        "Guide %d has read PID: %d from the pipe", guide->number + 1, visitor_on_catwalk.pid);

                for (int i = 0; i < visitor_on_catwalk.children_count; i++) {
                    int res = read(catwalk_pipe, buffer, person_space);
                    if (res == -1) {
                        perror("guide_run: read");
                        give_semaphore(guide->semaphores, PIPE_READ_SEMAPHORE);
                        give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
                        free(buffer);
                        return GUIDE_RUN_FAIL;
                    }
                    catwalk_visitors[catwalk_number] -= 1;
                    VisitorOnCatwalk tmp;
                    memcpy(&tmp, buffer, sizeof(VisitorOnCatwalk));
                    output_log(guide->semaphores, guide->shared_memory,
                        "Guide %d has read PID: %d (child) from the pipe", guide->number + 1, tmp.pid);
                }

                give_semaphore(guide->semaphores, PIPE_READ_SEMAPHORE);

                // if (visitor_on_catwalk.pid != 0) {
                    // Send visitor pid to the specific trail guide
                    VisitorGuideMessage visitor_guide_message;
                    visitor_guide_message.pid = visitor_on_catwalk.pid;
                    visitor_guide_message.children_count = visitor_on_catwalk.children_count;

                    Message message = {0};
                    message.mtype = (visitor_on_catwalk.trail_nr == 2)
                            ? guide->shared_memory->guide2_pid : guide->shared_memory->guide1_pid;
                    memcpy(message.mtext, &visitor_guide_message, sizeof(visitor_guide_message));
                    
                    if (msgsnd(guide->message_queue, &message, sizeof(message.mtext), 0) == -1)
                        perror("guide_run: msgsnd");
                // }

                give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
            }
        }

        int res;
        do {
            res = receive_message(guide);
        } while (res == 1);

        if (tour_start) {
            timeout_counter = 0;
            guide_tour(guide, person_space, buffer);
        }
    } while (!terminate);

    free(buffer);

    output_log(guide->semaphores, guide->shared_memory,
            "Guide (PID: %d) finished", getpid());

    return GUIDE_SUCCESS;
}
