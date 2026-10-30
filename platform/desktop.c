/* Desktop adapter (macOS, Windows): randomness, clocks, files and app windows. */
#define _POSIX_C_SOURCE 200809L
#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <errno.h>
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#else
#include <errno.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

/* Only plain web addresses: nothing that could become an extra option. */
static int safe_url(const char *url)
{
    size_t i;
    if (strncmp(url, "https://", 8) != 0 || strlen(url) > 400)
        return 0;
    for (i = 0; url[i]; i++)
        if ((unsigned char)url[i] <= ' ' || url[i] == '"' || url[i] == '\\' || (unsigned char)url[i] >= 0x7F)
            return 0;
    return 1;
}

#ifdef _WIN32

plat_status plat_random(void *buf, unsigned n)
{
    return BCryptGenRandom(NULL, (PUCHAR)buf, n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 ? PLAT_OK : PLAT_FAILED;
}

uint64_t plat_monotonic_ms(void)
{
    return (uint64_t)GetTickCount64();
}

int plat_mkdir(const char *path)
{
    return _mkdir(path) == 0 || errno == EEXIST ? 0 : -1;
}

/* rename() refuses to overwrite on Windows; this replaces atomically. */
int plat_replace(const char *from, const char *to)
{
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
}

/* Edge comes with Windows 10 and 11; "msedge.exe" is registered by name. */
plat_status plat_open_app_window(const char *url)
{
    char args[512];
    if (!safe_url(url))
        return PLAT_FAILED;
    snprintf(args, sizeof args, "--app=\"%s\" --start-fullscreen", url);
    return (INT_PTR)ShellExecuteA(NULL, "open", "msedge.exe", args, NULL, SW_SHOWMAXIMIZED) > 32 ? PLAT_OK
                                                                                                : PLAT_UNSUPPORTED;
}

#else

plat_status plat_random(void *buf, unsigned n)
{
    size_t got;
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f)
        return PLAT_FAILED;
    got = fread(buf, 1, n, f);
    fclose(f);
    return got == n ? PLAT_OK : PLAT_FAILED;
}

uint64_t plat_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

int plat_mkdir(const char *path)
{
    return mkdir(path, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

int plat_replace(const char *from, const char *to)
{
    return rename(from, to);
}

extern char **environ;

/* macOS: the first Chromium browser found, through open(1). Safari has no
 * app window to ask for. */
plat_status plat_open_app_window(const char *url)
{
    static const char *const APPS[] = {"/Applications/Google Chrome.app", "/Applications/Microsoft Edge.app",
                                       "/Applications/Brave Browser.app"};
    char app_arg[480];
    size_t i;
    struct stat st;
    if (!safe_url(url))
        return PLAT_FAILED;
    snprintf(app_arg, sizeof app_arg, "--app=%s", url);
    for (i = 0; i < sizeof APPS / sizeof APPS[0]; i++) {
        char *argv[] = {"open", "-na", (char *)APPS[i], "--args", app_arg, "--start-fullscreen", NULL};
        pid_t pid;
        int status = 0;
        if (stat(APPS[i], &st) != 0)
            continue;
        if (posix_spawn(&pid, "/usr/bin/open", NULL, NULL, argv, environ) != 0)
            return PLAT_FAILED;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? PLAT_OK : PLAT_FAILED;
    }
    return PLAT_UNSUPPORTED;
}

#endif

int64_t plat_wall_utc(void)
{
    return (int64_t)time(NULL);
}
