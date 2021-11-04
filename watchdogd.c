#include "debug.h"
#include "iterate_executables.h"
#include "utils.h"
#include "watchdog_fd.h"

#include <libubox/runqueue.h>
#include <libubox/uloop.h>
#include <ubus_utils/ubus_utils.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct test_state_st
{
    struct timespec test_run_start_time;
    unsigned num_tests;
    bool run_succeeded;
};

struct watchdog_context_st
{
    char const * exe_dir;
    int watchdog_fd;
    int watchdog_timeout_secs;
    int watchdog_keep_alive_msecs;
    struct uloop_timeout interval_timer;
    struct runqueue task_q;

    struct test_state_st state;
};

static char const
default_test_exe_dir[] = "/etc/watchdog.d";

static int const
default_watchdog_timeout_secs = 90;

static int const
default_keepalive_timer_secs = (default_watchdog_timeout_secs / 2) - 3;

static void
usage(FILE * const stream, char const * const program_name)
{
    fprintf(
        stream,
        "%s -e <test_directory. [-w <sec>] [-k <sec>] [-d] [-h]\n"
        "A basic watchdog daemon that strokes the watchdog after ensuring\n"
        "applications in the test directory all return success (0).\n"
        "Options:\n"
        "\t-e <test_directory> - the directory tests are found in (default: \"/etc/watchdog.d\")\n"
        "\t-w <secs>           - set the watchdog counter to <sec> seconds (default: %u secs)\n"
        "\t-k <secs>           - set the keepalive time to <sec> seconds (default: %u secs)\n"
        "\t-d                  - disable the watchdog when this program exits\n"
        "\t-h                  - write this help message and exit\n",
        program_name,
        default_watchdog_timeout_secs,
        default_keepalive_timer_secs);
}

static void
cleanup_unneeded_resources(void)
{
    uloop_done();
}

static void
schedule_next_test_run(
    struct watchdog_context_st * const context, int const when_msecs)
{
    debug("next test run in %u msecs\n", when_msecs);

    uloop_timeout_set(&context->interval_timer, when_msecs);
}

static void
all_tasks_complete_cb(struct runqueue * const q)
{
    struct watchdog_context_st * context =
        container_of(q, struct watchdog_context_st, task_q);
    struct test_state_st * const state = &context->state;

    debug("all tasks complete\n");

    if (state->run_succeeded)
    {
        debug("stroke watchdog\n");

#if WITHOUT_WATCHDOG == 0
        stroke_watchdog(context->watchdog_fd);
#endif
    }

    schedule_next_test_run(
        context,
        calculate_next_interval_msecs(
            &state->test_run_start_time,
            context->watchdog_keep_alive_msecs));
}

static void
task_q_init(struct runqueue * const task_q)
{
    runqueue_init(task_q);
    task_q->max_running_tasks = 1;
    task_q->empty_cb = all_tasks_complete_cb;
}

static void disable_output(void)
{
    int const output_fd = open("/dev/null", O_WRONLY);

    if (output_fd >= 0)
    {
        dup2(output_fd, STDOUT_FILENO);
        dup2(output_fd, STDERR_FILENO);
        close(output_fd);
    }
}

struct test_context_st
{
    struct uloop_process process;
    struct runqueue_task task;
    char const * executable_filename;
    int exit_status;
    /*
     * A back pointer to the test context is needed because the process ended
     * callback only provides a pointer to the process context in the test, but
     * state info in the watchdog context needs to be updated.
     */
    struct watchdog_context_st * context;
};

static void
run_test_child_process(struct test_context_st * const test)
{
    /*
     * This is a fork of the watchdog process and the child process doesn't
     * need access to some of the watchdog resources, so clean them up.
     */
    cleanup_unneeded_resources();
    disable_output();

    execle(test->executable_filename, test->executable_filename, NULL, NULL);

    /* Shouldn't get here. */
    exit(0);
}

static void
test_process_complete(struct uloop_process * const c, int const ret)
{
    struct test_context_st * const test =
        container_of(c, struct test_context_st, process);
    struct watchdog_context_st * const context = test->context;
    struct test_state_st * const state = &context->state;

    debug("process '%s' ended and result %d\n", test->executable_filename, ret);

    test->exit_status = WEXITSTATUS(ret);

    debug("exit status was %d\n", test->exit_status);

    if (test->exit_status != 0)
    {
        state->run_succeeded = false;
    }

    runqueue_task_complete(&test->task);
}


/*
 * Called for each task. The task starts the process that performs the test.
 * When that process completes the task is completed and the next task will run.
 */
static void
run_task(struct runqueue * const task_q, struct runqueue_task * const task)
{
    struct watchdog_context_st * const context =
        container_of(task_q, struct watchdog_context_st, task_q);
    struct test_context_st * const test =
        container_of(task, struct test_context_st, task);

    test->process.pid = fork();
    if (test->process.pid > 0)
    {
        /* Parent process. */
        test->process.cb = test_process_complete;
        uloop_process_add(&test->process);
    }
    else if (test->process.pid == 0)
    {
        /* Child process. */
        run_test_child_process(test);
    }
    else
    {
        /* Failed to start the child process. */
        struct test_state_st * const state = &context->state;

        state->run_succeeded = false;

        debug("failed to start test process\n");

        runqueue_task_complete(&test->task);
    }
}

static void
generic_event_free(struct test_context_st * const test)
{
    if (test != NULL)
    {
        free(UNCONST(test->executable_filename));
        free(test);
    }
}

static void
task_complete(
    struct runqueue * const task_q, struct runqueue_task * const task)
{
    UNUSED_ARG(task_q);
    struct test_context_st * const test =
        container_of(task, struct test_context_st, task);

    debug("%s: test task ended\n", test->executable_filename);

    generic_event_free(test);
}

static struct test_context_st *
test_new(
    struct watchdog_context_st * const context,
    char const * const executable_filename)
{
    static const struct runqueue_task_type generic_task_type =
    {
        .name = "Watchdog test",
        .run = run_task,
        .kill = runqueue_process_kill_cb
        /* Is 'cancel' support required? */
    };
    struct test_context_st * const test = calloc(1, sizeof *test);

    if (test == NULL)
    {
        goto done;
    }

    test->context = context;
    test->executable_filename = strdup(executable_filename);
    test->task.type = &generic_task_type;
    test->task.complete = task_complete;

done:
    return test;
}

static void
add_test_task(
    struct watchdog_context_st * const context,
    char const * const executable_filename)
{
    struct runqueue * const task_q = &context->task_q;
    struct test_context_st * const test = test_new(context, executable_filename);

    if (test != NULL)
    {
        struct test_state_st * const state = &context->state;

        runqueue_task_add(task_q, &test->task, false);
        state->num_tests++;
    }
}

static bool
iterate_cb(char const * const filename, void * const user_ctx)
{
    struct watchdog_context_st * const context = user_ctx;

    debug("run %s\n", filename);

    add_test_task(context, filename);

    bool const continue_iteration = true;

    return continue_iteration;
}

static bool
initialise_test_state(struct test_state_st * const state)
{
    state->run_succeeded = true;
    state->num_tests = 0;

    bool const success = get_timestamp(&state->test_run_start_time);

    return success;
}

static void
enqueue_functionality_tests(
    struct watchdog_context_st * const context, char const * const executable_path)
{
    bool should_stroke_watchdog;
    int next_interval_msecs;
    struct test_state_st * const state = &context->state;

    if (!initialise_test_state(state))
    {
        debug("failed to initialise test state before enqueuing tests\n");

        should_stroke_watchdog = false;
        next_interval_msecs = context->watchdog_keep_alive_msecs;
    }
    else
    {
        iterate_executable_files(executable_path, iterate_cb, context);
        should_stroke_watchdog = state->num_tests == 0;
        /*
         * If any tests are scheduled then the next interval is calculated at
         * the end of the run.
         */
        next_interval_msecs =
            (state->num_tests == 0) ? context->watchdog_keep_alive_msecs : 0;
    }

    if (should_stroke_watchdog)
    {
        /*
         * No tests to run, so just stroke the watchdog and do it all again
         * soon.
         */
#if WITHOUT_WATCHDOG == 0
        stroke_watchdog(context->watchdog_fd);
#endif
    }

    if (next_interval_msecs > 0)
    {
        schedule_next_test_run(context, next_interval_msecs);
    }
}

static void
watchdog_interval_timeout_cb(struct uloop_timeout * const t)
{
    struct watchdog_context_st * const context =
        container_of(t, struct watchdog_context_st, interval_timer);

    debug("timeout\n");

    char * exe_path;

    if (asprintf(&exe_path, "%s/*", context->exe_dir) < 0)
    {
        debug("memory error\n");
        goto done;
    }

    enqueue_functionality_tests(context, exe_path);

    free(exe_path);

done:
    return;
}

static void
run(struct watchdog_context_st * const context)
{
    uloop_init();

    task_q_init(&context->task_q);
    context->interval_timer.cb = watchdog_interval_timeout_cb;

    /* Do the first test run right away. */
    int const msecs_til_next_run = 0;

    schedule_next_test_run(context, msecs_til_next_run);

    uloop_run();
}

#if WITHOUT_WATCHDOG == 0
static bool
open_and_configure_watchdog(struct watchdog_context_st * const context)
{
    bool success;

    context->watchdog_fd = watchdog_open();
    if (context->watchdog_fd == -1)
    {
        success = false;
        goto done;
    }

    if (set_watchdog_secs(context->watchdog_fd, context->watchdog_timeout_secs))
    {
        fprintf(stderr, "Unable to set watchdog timeout. Check the kernel log for more details.\n");
        fprintf(stderr, "Continuing with the original value\n");
    }

    int const real_wd_timeout_msecs = get_watchdog_secs(context->watchdog_fd) * 1000;

    if (real_wd_timeout_msecs < 0)
    {
        perror("Error while getting watchdog timeout");
    }
    else
    {
        if (real_wd_timeout_msecs <= context->watchdog_keep_alive_msecs)
        {
            fprintf(
                stderr,
                "Warning: Watchdog counter (%d) <= keepalive interval (%d).\n",
                real_wd_timeout_msecs,
                context->watchdog_keep_alive_msecs);
            fprintf(stderr, "The watchdog will probably trigger\n");
        }
    }

    debug("keepalive msecs %u\n", context->watchdog_keep_alive_msecs);

    success = true;

done:
    return success;
}
#endif

int
main(int const argc, char * * const argv)
{
    int exit_code;
    struct watchdog_context_st context =
    {
        .exe_dir = default_test_exe_dir,
        .watchdog_fd = -1,
        .watchdog_timeout_secs = default_watchdog_timeout_secs,
        .watchdog_keep_alive_msecs = default_keepalive_timer_secs * 1000
    };
#if WITHOUT_WATCHDOG == 0
    bool disable_on_exit = false;
#endif
    int opt;

    while ((opt = getopt(argc, argv, "dhw:k:e:")) != -1)
    {
        switch (opt)
        {
        case 'e':
            context.exe_dir = optarg;
            break;

        case 'd':
#if WITHOUT_WATCHDOG == 0
            disable_on_exit = true;
#endif
            break;

        case 'w':
            context.watchdog_timeout_secs = atoi(optarg);
            break;

        case 'k':
            context.watchdog_keep_alive_msecs = atoi(optarg) * 1000;
            break;

        case 'h':
            usage(stdout, argv[0]);
            exit_code = EXIT_SUCCESS;
            goto done;

        default:
            usage(stderr, argv[0]);
            exit_code = EXIT_FAILURE;
            goto done;
        }
    }

#if WITHOUT_WATCHDOG == 0
    if (!open_and_configure_watchdog(&context))
    {
        perror("Watchdog device not enabled");
        fflush(stderr);
        exit_code = EXIT_FAILURE;
        goto done;
    }
#endif

    run(&context);

    exit_code = EXIT_SUCCESS;

done:
#if WITHOUT_WATCHDOG == 0
    if (context.watchdog_fd >= 0)
    {
        watchdog_close(context.watchdog_fd, disable_on_exit);
    }
#endif

    return exit_code;
}

