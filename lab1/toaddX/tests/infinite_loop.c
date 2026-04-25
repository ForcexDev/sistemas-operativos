/*
 * infinite_loop — Test program that runs forever.
 * Useful for testing: ps, stop, kill, status
 */
#include <stdio.h>
#include <unistd.h>

int main(void) {
    int counter = 0;
    while (1) {
        counter++;
        sleep(1);
    }
    return 0;
}
