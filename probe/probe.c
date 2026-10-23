/* Minimal execution probe (plan v2.0 p.42 step 4, p.44 "First execution proof").
 * Prints a build identity and exits 0. Opens no devices, writes no files,
 * starts no network listener. */
#include <stdio.h>

#ifndef BUILD_ID
#define BUILD_ID "unset"
#endif
#ifndef BUILD_TARGET
#define BUILD_TARGET "host"
#endif

int main(void)
{
    printf("family-tv-probe build=%s target=%s\n", BUILD_ID, BUILD_TARGET);
    return 0;
}
