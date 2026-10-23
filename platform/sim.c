/* Host simulator adapter. Broadcast and channel-list views are simulated;
 * direct tuning and online apps are reported unsupported, matching what is
 * currently verified on the real TV (nothing). */
#include "platform.h"

const char *plat_name(void) { return "SIMULATOR"; }
plat_status plat_open_broadcast(void) { return PLAT_OK; }
plat_status plat_tune_service(const char *service_name) { (void)service_name; return PLAT_UNSUPPORTED; }
plat_status plat_open_channel_list(void) { return PLAT_OK; }
plat_status plat_open_app(const char *app_id) { (void)app_id; return PLAT_UNSUPPORTED; }
plat_status plat_web_search(void) { return PLAT_UNSUPPORTED; }
