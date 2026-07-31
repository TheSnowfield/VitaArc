#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <libc/poll.h>

static atomic_bool __stop_signals;

static void* __send_later(void* argument)
{
    int fd = *(int*)argument;
    struct timespec wait = {.tv_nsec = 10 * 1000 * 1000};

    assert(nanosleep(&wait, NULL) == 0 && write(fd, "x", 1) == 1);
    return NULL;
}

static void __signal_handler(int signal)
{
    assert(signal == SIGUSR1);
}

static void* __interrupt_later(void* argument)
{
    pthread_t target = *(pthread_t*)argument;
    struct timespec wait = {.tv_nsec = 10 * 1000 * 1000};

    do {
        assert(nanosleep(&wait, NULL) == 0 && pthread_kill(target, SIGUSR1) == 0);
        wait.tv_nsec = 1000 * 1000;
    } while(!atomic_load(&__stop_signals));

    return NULL;
}

static void __check_streams(void)
{
    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    bridge_pollfd_t entries[] = {{.fd = sockets[0], .events = BIONIC_POLLIN},
        {.fd = sockets[0], .events = BIONIC_POLLOUT},
        {.fd = -1, .events = BIONIC_POLLIN, .returned = 123}};

    assert(bridge_poll(entries, 3, 0) == 1 && entries[0].returned == 0 && entries[1].returned == BIONIC_POLLOUT &&
        entries[2].returned == 0);
    pthread_t writer;
    assert(!pthread_create(&writer, NULL, __send_later, &sockets[1]));
    assert(bridge_poll(entries, 1, -1) == 1 && entries[0].returned == BIONIC_POLLIN);
    assert(!pthread_join(writer, NULL));
    entries[1].events = BIONIC_POLLRDNORM;
    assert(bridge_poll(entries, 2, 0) == 2 && entries[0].returned == BIONIC_POLLIN &&
        entries[1].returned == BIONIC_POLLRDNORM);
    char byte;
    assert(read(sockets[0], &byte, 1) == 1 && byte == 'x');
    errno = EBUSY;
    entries[0].returned = 123;
    assert(bridge_poll(entries, 1, 10) == 0 && entries[0].returned == 0 && errno == EBUSY);
    assert(close(sockets[1]) == 0);
    entries[0].events = 0;
    assert(bridge_poll(entries, 1, 0) == 1 && (entries[0].returned & BIONIC_POLLHUP));
    assert(close(sockets[0]) == 0);
    assert(bridge_poll(entries, 1, -1) == 1 && entries[0].returned == BIONIC_POLLNVAL);

    int pipes[2];
    assert(pipe(pipes) == 0);
    entries[0] = (bridge_pollfd_t){.fd = pipes[0], .events = BIONIC_POLLIN};

    assert(write(pipes[1], "data", 4) == 4 && close(pipes[1]) == 0);
    assert(bridge_poll(entries, 1, 0) == 1 &&
        (entries[0].returned & (BIONIC_POLLIN | BIONIC_POLLHUP)) == (BIONIC_POLLIN | BIONIC_POLLHUP));
    assert(close(pipes[0]) == 0);
}

static void __check_files_and_validation(void)
{
    FILE* file = tmpfile();
    assert(file);
    bridge_pollfd_t entry = {.fd = fileno(file), .events = BIONIC_POLLIN | BIONIC_POLLOUT};

    assert(bridge_poll(&entry, 1, -1) == 1 && entry.returned == entry.events);
    assert(fclose(file) == 0);
    assert(bridge_poll(NULL, 1, 0) == -1 && errno == EFAULT);
    assert(bridge_poll(NULL, BRIDGE_POLL_MAX + 1, 0) == -1 && errno == EINVAL);
    bridge_pollfd_t ignored[BRIDGE_POLL_MAX];
    for(unsigned index = 0; index < BRIDGE_POLL_MAX; ++index) {
        ignored[index] = (bridge_pollfd_t){.fd = -1, .returned = 123};
    }

    assert(bridge_poll(ignored, BRIDGE_POLL_MAX, 0) == 0);
    for(unsigned index = 0; index < BRIDGE_POLL_MAX; ++index) {
        assert(ignored[index].returned == 0);
    }

    assert(bridge_poll(NULL, 0, 1) == 0);
}

int main(void)
{
    __check_streams();
    __check_files_and_validation();
    struct sigaction action = {.sa_handler = __signal_handler};

    assert(sigemptyset(&action.sa_mask) == 0 && sigaction(SIGUSR1, &action, NULL) == 0);
    pthread_t current = pthread_self(), interrupt;
    assert(!pthread_create(&interrupt, NULL, __interrupt_later, &current));
    assert(bridge_poll(NULL, 0, -1) == -1 && errno == EINTR);
    atomic_store(&__stop_signals, true);
    assert(!pthread_join(interrupt, NULL));
    puts(
        "Poll passed: real sockets/pipes/files, duplicate entries, readiness/HUP/NVAL, delayed/infinite waits, timeout, signals and validation.");
}
