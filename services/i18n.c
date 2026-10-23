#include "i18n.h"

static const char *const TABLE[STR_COUNT][LANG_COUNT] = {
    [STR_FAMILY_TV] = {"Family TV", "TV ya Familia"},
    [STR_LIVE_TV] = {"Live TV", "TV Mubashara"},
    [STR_LIVE_TV_HINT] = {"Aerial channels", "Chaneli za antena"},
    [STR_YOUTUBE] = {"YouTube", "YouTube"},
    [STR_NETFLIX] = {"Netflix", "Netflix"},
    [STR_APP_HINT] = {"Existing TV app", "Programu ya TV"},
    [STR_FOOTBALL] = {"Football", "Soka"},
    [STR_FOOTBALL_HINT] = {"Channels", "Chaneli"},
    [STR_GAMES] = {"Games", "Michezo"},
    [STR_GAMES_HINT] = {"Race, kick, build", "Mbio, piga, jenga"},
    [STR_MORE] = {"More", "Zaidi"},
    [STR_MORE_HINT] = {"Search and settings", "Tafuta na mipangilio"},
    [STR_ALL_CHANNELS] = {"All channels (TV channel list)", "Chaneli zote (orodha ya TV)"},
    [STR_WEB_SEARCH] = {"Web search", "Tafuta mtandaoni"},
    [STR_TO_CHILD] = {"Switch to child profile", "Badili kwa wasifu wa watoto"},
    [STR_TO_PARENT] = {"Switch to parent profile", "Badili kwa wasifu wa mzazi"},
    [STR_PARENT_SETTINGS] = {"Parent settings", "Mipangilio ya mzazi"},
    [STR_APP_STATUS] = {"App support status", "Hali ya programu"},
    [STR_GAME_RACER] = {"Family Racer", "Mbio za Familia"},
    [STR_GAME_PENALTY] = {"Penalty Cup", "Kombe la Penalti"},
    [STR_GAME_GARDEN] = {"Block Garden", "Bustani ya Vitalu"},
    [STR_HELP_HOME] = {"Arrows: move   OK: choose   Home: main menu",
                       "Mishale: sogeza   OK: chagua   Home: menyu kuu"},
    [STR_HELP_LIST] = {"Up/Down: move   OK: choose   Back: return",
                       "Juu/Chini: sogeza   OK: chagua   Back: rudi"},
    [STR_HELP_PIN] = {"Up/Down: change digit   Left/Right: move   OK: next   Back: cancel",
                      "Juu/Chini: badili tarakimu   Kushoto/Kulia: sogeza   OK: endelea   Back: ghairi"},
    [STR_PARENT] = {"Parent", "Mzazi"},
    [STR_CHILD] = {"Child", "Mtoto"},
    [STR_PIN_ENTER] = {"Enter parent PIN", "Weka PIN ya mzazi"},
    [STR_PIN_NEW] = {"Choose a new 4-digit parent PIN", "Chagua PIN mpya ya mzazi ya tarakimu 4"},
    [STR_PIN_CONFIRM] = {"Enter the new PIN again", "Weka PIN mpya tena"},
    [STR_PIN_WRONG] = {"Wrong PIN", "PIN si sahihi"},
    [STR_PIN_LOCKED] = {"Too many wrong tries. Seconds to wait:", "Majaribio mengi yasiyo sahihi. Sekunde za kusubiri:"},
    [STR_PIN_MISMATCH] = {"The PINs did not match. Try again.", "PIN hazilingani. Jaribu tena."},
    [STR_PIN_SAVED] = {"Parent PIN saved", "PIN ya mzazi imehifadhiwa"},
    [STR_NEED_PIN_FIRST] = {"Choose a parent PIN first", "Chagua PIN ya mzazi kwanza"},
    [STR_CHILD_ACTIVE] = {"Child profile active", "Wasifu wa watoto umewashwa"},
    [STR_PARENT_ACTIVE] = {"Parent profile active", "Wasifu wa mzazi umewashwa"},
    [STR_LANGUAGE] = {"Language / Lugha", "Language / Lugha"},
    [STR_CHILD_LIMIT] = {"Children's daily play time", "Muda wa kucheza wa watoto kwa siku"},
    [STR_LIMIT_OFF] = {"no limit", "hakuna kikomo"},
    [STR_MINUTES] = {"min", "dk"},
    [STR_GRANT_15] = {"Give 15 more minutes today", "Ongeza dakika 15 leo"},
    [STR_GRANTED] = {"15 more minutes given for today", "Dakika 15 zaidi zimeongezwa leo"},
    [STR_RESET_TODAY] = {"Reset today's play time", "Anza upya muda wa leo"},
    [STR_RESET_DONE] = {"Today's play time was reset", "Muda wa leo umeanzishwa upya"},
    [STR_CHANGE_PIN] = {"Change parent PIN", "Badilisha PIN ya mzazi"},
    [STR_ALLOWED] = {"allowed", "inaruhusiwa"},
    [STR_BLOCKED] = {"blocked", "imezuiwa"},
    [STR_SETTINGS_LOCKED] = {"Enter the parent PIN first", "Weka PIN ya mzazi kwanza"},
    [STR_SAVE_FAILED] = {"Settings could not be saved", "Mipangilio haikuweza kuhifadhiwa"},
    [STR_TIME_UP] = {"Play time is up for today", "Muda wa kucheza wa leo umekwisha"},
    [STR_TIME_LEFT] = {"Play time left today", "Muda wa kucheza uliobaki leo"},
    [STR_LIVE_OPENED] = {"Live TV: stock broadcast view opened", "TV Mubashara: skrini ya matangazo imefunguliwa"},
    [STR_LIVE_FAILED] = {"Live TV could not be opened", "TV Mubashara haikuweza kufunguliwa"},
    [STR_TUNE_FALLBACK] = {"Direct tuning not verified: TV channel list opened, choose",
                           "Kubadili chaneli moja kwa moja hakujathibitishwa: orodha ya chaneli imefunguliwa, chagua"},
    [STR_TUNED] = {"Tuned", "Chaneli imewekwa:"},
    [STR_LIST_OPENED] = {"TV channel list opened", "Orodha ya chaneli imefunguliwa"},
    [STR_LIST_FAILED] = {"Channel list unavailable", "Orodha ya chaneli haipatikani"},
    [STR_APP_UNAVAILABLE] = {"This app is not available on this TV yet", "Programu hii bado haipatikani kwenye TV hii"},
    [STR_SEARCH_UNAVAILABLE] = {"Web search not yet verified on this TV",
                                "Kutafuta mtandaoni bado hakujathibitishwa kwenye TV hii"},
    [STR_UNAVAILABLE] = {"Not available", "Haipatikani"},
};

const char *tr(ft_lang lang, ft_str id)
{
    const char *s;
    if (id >= STR_COUNT)
        return "?";
    if (lang >= LANG_COUNT)
        lang = LANG_EN;
    s = TABLE[id][lang];
    return s ? s : TABLE[id][LANG_EN];
}

const char *lang_name(ft_lang lang)
{
    return lang == LANG_SW ? "Kiswahili" : "English";
}
