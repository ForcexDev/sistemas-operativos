/*
 * crasher — Test program that crashes after 1 second.
 * Useful for testing: bonus auto-restart and FAILED state
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    sleep(1);
    abort();  /* Triggers SIGABRT — unexpected death */
    return 0;
}
