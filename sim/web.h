#ifndef FT_WEB_H
#define FT_WEB_H

/* The real YouTube and Netflix sites inside the Family TV window, in the
 * system's own web view (WebKit on a Mac, WebView2 on Windows): it covers
 * the window below a strip that Family TV keeps for its Back button.
 * Sign-ins are kept between visits, as in a browser. native_window is the
 * NSWindow or HWND; top_permille is where the web view starts, in
 * thousandths of the window's height. */

/* 0 when the site is opening; -1 when there is no web view here (Windows
 * without the WebView2 runtime, or a build without one). */
int web_open(void *native_window, const char *url, const char *data_dir, int top_permille);
void web_resize(int top_permille);
void web_close(void);

#endif
