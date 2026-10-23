#include "session.h"

#include <stdio.h>

#include "platform.h"

static void say(shell *s, const char *msg)
{
    snprintf(s->status, sizeof s->status, "[%s] %s", plat_name(), msg);
}

int session_dispatch(shell *s, shell_action a)
{
    char buf[sizeof s->status];

    switch (a.act) {
    case ACT_OPEN_BROADCAST:
        say(s, plat_open_broadcast() == PLAT_OK ? "Live TV: stock broadcast view opened"
                                                : "Live TV could not be opened");
        break;
    case ACT_TUNE_FAVOURITE: {
        const char *name = s->cfg.favs[a.arg].name;
        if (plat_tune_service(name) == PLAT_OK) {
            snprintf(buf, sizeof buf, "Tuned %s", name);
        } else if (plat_open_channel_list() == PLAT_OK) {
            snprintf(buf, sizeof buf, "Direct tuning not verified: TV channel list opened, choose %s", name);
        } else {
            snprintf(buf, sizeof buf, "%s unavailable; use the TV remote's channel list", name);
        }
        say(s, buf);
        break;
    }
    case ACT_OPEN_CHANNEL_LIST:
        say(s, plat_open_channel_list() == PLAT_OK ? "TV channel list opened"
                                                   : "Channel list unavailable");
        break;
    case ACT_OPEN_YOUTUBE:
    case ACT_OPEN_NETFLIX:
        say(s, plat_open_app(a.act == ACT_OPEN_YOUTUBE ? "youtube" : "netflix") == PLAT_OK
                   ? "App opened"
                   : "This app is not available on this TV yet");
        break;
    case ACT_LAUNCH_GAME:
        return a.arg;
    case ACT_WEB_SEARCH:
        say(s, plat_web_search() == PLAT_OK ? "Web search opened"
                                            : "Web search not yet verified on this TV");
        break;
    case ACT_SWITCH_PROFILE:
        if (s->cfg.profile == PROFILE_PARENT) {
            shell_set_profile(s, PROFILE_CHILD);
            say(s, "Child profile active");
        } else {
            say(s, "Parent PIN required (PIN entry not built yet)");
        }
        break;
    case ACT_SHOW_SUPPORT:
        snprintf(buf, sizeof buf, "YouTube: %s | Netflix: %s | Direct tuning: unverified",
                 s->cfg.youtube_supported ? "tested" : "untested",
                 s->cfg.netflix_supported ? "tested" : "untested");
        say(s, buf);
        break;
    case ACT_NONE:
        break;
    }
    return -1;
}
