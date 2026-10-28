#ifndef FT_CHECK_H
#define FT_CHECK_H

#include <stdio.h>

extern int failures, checks;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                    \
    } while (0)

#define TMP "out/testdata"

/* Removes a save file and its .prev/.bad/.tmp companions. */
void test_clean_save(const char *path);
/* Flips one byte of a file in place. */
void test_corrupt_byte(const char *path, long off);
void run_service_tests(void);
void run_view_tests(void);
void run_channel_tests(void);

#endif
