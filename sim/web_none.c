/* web.h where there is no web view: apps open in their own window. */
#include "web.h"

int web_open(void *native_window, const char *url, const char *data_dir, int top_permille)
{
    (void)native_window;
    (void)url;
    (void)data_dir;
    (void)top_permille;
    return -1;
}

void web_resize(int top_permille)
{
    (void)top_permille;
}

void web_close(void)
{
}
