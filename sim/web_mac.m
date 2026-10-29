/* web.h on a Mac: a WKWebView over the SDL window's content, Safari's own
 * engine, so YouTube and Netflix see Safari (Netflix plays through Apple's
 * FairPlay protection, as in Safari). */
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "web.h"

/* Links that open a new window open here instead. */
@interface FTWebDelegate : NSObject <WKUIDelegate>
@end

@implementation FTWebDelegate
- (WKWebView *)webView:(WKWebView *)web
    createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration
               forNavigationAction:(WKNavigationAction *)action
                    windowFeatures:(WKWindowFeatures *)features
{
    (void)configuration;
    (void)features;
    if (action.request.URL)
        [web loadRequest:action.request];
    return nil;
}
@end

static WKWebView *view;
static FTWebDelegate *delegate;

static NSRect below_strip(NSView *content, int top_permille)
{
    NSRect r = content.bounds; /* not flipped: y = 0 is the bottom */
    r.size.height -= r.size.height * top_permille / 1000;
    return r;
}

int web_open(void *native_window, const char *url, const char *data_dir, int top_permille)
{
    NSWindow *win = (NSWindow *)native_window;
    NSView *content = win.contentView;
    WKWebViewConfiguration *cfg;
    NSURL *u;
    (void)data_dir; /* WebKit keeps its own store, per app */
    if (!content || !url)
        return -1;
    web_close();
    cfg = [[WKWebViewConfiguration alloc] init];
    cfg.preferences.elementFullscreenEnabled = YES; /* a video's full-screen button */
    cfg.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    view = [[WKWebView alloc] initWithFrame:below_strip(content, top_permille) configuration:cfg];
    [cfg release];
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    view.customUserAgent = @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
                           @"(KHTML, like Gecko) Version/18.0 Safari/605.1.15";
    delegate = [[FTWebDelegate alloc] init];
    view.UIDelegate = delegate;
    [content addSubview:view];
    u = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
    if (u)
        [view loadRequest:[NSURLRequest requestWithURL:u]];
    [win makeFirstResponder:view];
    return 0;
}

void web_resize(int top_permille)
{
    if (view && view.superview)
        view.frame = below_strip(view.superview, top_permille);
}

void web_close(void)
{
    if (!view)
        return;
    [view stopLoading];
    [view removeFromSuperview];
    view.UIDelegate = nil;
    [view release];
    [delegate release];
    view = nil;
    delegate = nil;
}
