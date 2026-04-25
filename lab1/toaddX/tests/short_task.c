/*
 * short_task — Test program that exits quickly.
 * Useful for testing: zombie state
 */
#include <stdio.h>
#include <unistd.h>

int main(void) {
    sleep(2);
    return 0;
}
