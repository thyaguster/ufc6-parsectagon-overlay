#ifndef UFC6_MENU_FAVORITES_H
#define UFC6_MENU_FAVORITES_H
#include <stddef.h>
#include <wchar.h>

enum { MENU_MAX_FAVORITES = 5 };
enum {
    FAV_SCREENSHOT,
    FAV_FIGHT_REPORT,
    FAV_RESTART,
    FAV_QUIT,
    FAV_STAMINA,
    FAV_NO_DAMAGE,
    FAV_NEXT_HIT_KO,
    FAV_CAREER_CHEATS,
    FAV_SAVE_BACKUPS,
    FAV_PHOTO_ENTER,
    FAV_PHOTO_FREEZE,
    FAV_PHOTO_FX,
    FAV_PHOTO_WATERMARK,
    FAV_APPEARANCE,
    FAV_CONTROLS,
    FAV_GAME_SETTINGS,
    FAV_PERFORMANCE,
    FAV_TOOLS,
    FAV_CATALOG_COUNT
};
enum {
    FAV_CAT_ALL,
    FAV_CAT_ACTIONS,
    FAV_CAT_CHEATS,
    FAV_CAT_PHOTO,
    FAV_CAT_SETTINGS,
    FAV_CAT_COUNT
};

typedef struct {
    int id;
    int category;
    const char *key;
    const wchar_t *label;
    const wchar_t *category_label;
} FavoriteCatalogItem;

static const wchar_t *fav_category_names[FAV_CAT_COUNT] = {
    L"All", L"Actions", L"Cheats", L"Photo", L"Settings"
};

static inline const FavoriteCatalogItem *fav_catalog_item(int id) {
    static const FavoriteCatalogItem items[FAV_CATALOG_COUNT] = {
        { FAV_SCREENSHOT,      FAV_CAT_ACTIONS,  "screenshot",   L"Take screenshot",              L"Actions"  },
        { FAV_FIGHT_REPORT,    FAV_CAT_ACTIONS,  "fight_report", L"Post-fight report...",          L"Actions"  },
        { FAV_RESTART,         FAV_CAT_ACTIONS,  "restart",      L"Restart game...",               L"Actions"  },
        { FAV_QUIT,            FAV_CAT_ACTIONS,  "quit",         L"Quit game...",                  L"Actions"  },
        { FAV_STAMINA,         FAV_CAT_CHEATS,   "stamina",      L"Player 1 Infinite Stamina",     L"Cheats"   },
        { FAV_NO_DAMAGE,       FAV_CAT_CHEATS,   "no_damage",    L"Player 1 No Damage",            L"Cheats"   },
        { FAV_NEXT_HIT_KO,     FAV_CAT_CHEATS,   "next_hit_ko",  L"Next Hit KO (25s)",             L"Cheats"   },
        { FAV_CAREER_CHEATS,   FAV_CAT_CHEATS,   "career",       L"Career cheats...",              L"Cheats"   },
        { FAV_SAVE_BACKUPS,    FAV_CAT_CHEATS,   "backups",      L"Savegame backups...",           L"Cheats"   },
        { FAV_PHOTO_ENTER,     FAV_CAT_PHOTO,    "photo_mode",   L"Enter Freecam Photo Mode",      L"Photo"    },
        { FAV_PHOTO_FREEZE,    FAV_CAT_PHOTO,    "photo_freeze", L"Freeze Fight Action",           L"Photo"    },
        { FAV_PHOTO_FX,        FAV_CAT_PHOTO,    "photo_fx",     L"Visual Effects & Filters...",   L"Photo"    },
        { FAV_PHOTO_WATERMARK, FAV_CAT_PHOTO,    "watermark",    L"Custom Watermark & Brand...",   L"Photo"    },
        { FAV_APPEARANCE,      FAV_CAT_SETTINGS, "appearance",   L"Appearance & theme...",         L"Settings" },
        { FAV_CONTROLS,        FAV_CAT_SETTINGS, "controls",     L"Controls & keybinds...",        L"Settings" },
        { FAV_GAME_SETTINGS,   FAV_CAT_SETTINGS, "game",         L"Game & launcher settings...",   L"Settings" },
        { FAV_PERFORMANCE,     FAV_CAT_SETTINGS, "performance",  L"Performance & graphics...",     L"Settings" },
        { FAV_TOOLS,           FAV_CAT_SETTINGS, "tools",        L"Tools & diagnostics...",        L"Settings" }
    };
    return (id >= 0 && id < FAV_CATALOG_COUNT) ? &items[id] : NULL;
}

static inline int fav_ascii_lower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

static inline int fav_contains_ci(const wchar_t *hay, const char *needle) {
    if (!needle || !needle[0]) return 1;
    if (!hay) return 0;
    for (int i = 0; hay[i]; ++i) {
        int j = 0;
        while (needle[j] && hay[i + j] && fav_ascii_lower((int)hay[i + j]) == fav_ascii_lower((int)(unsigned char)needle[j])) ++j;
        if (!needle[j]) return 1;
    }
    return 0;
}

static inline int fav_contains_ascii_ci(const char *hay, const char *needle) {
    if (!needle || !needle[0]) return 1;
    if (!hay) return 0;
    for (int i = 0; hay[i]; ++i) {
        int j = 0;
        while (needle[j] && hay[i + j] && fav_ascii_lower((int)(unsigned char)hay[i + j]) == fav_ascii_lower((int)(unsigned char)needle[j])) ++j;
        if (!needle[j]) return 1;
    }
    return 0;
}

static inline int fav_is_pinned(const int *favs, int count, int id) {
    for (int i = 0; i < count; ++i) if (favs[i] == id) return 1;
    return 0;
}

/* Returns 1 if pinned, 0 if unpinned, -1 if already at MENU_MAX_FAVORITES. */
static inline int fav_toggle_pin(int *favs, int *count, int id) {
    if (id < 0 || id >= FAV_CATALOG_COUNT) return -1;
    for (int i = 0; i < *count; ++i) {
        if (favs[i] == id) {
            for (int j = i; j + 1 < *count; ++j) favs[j] = favs[j + 1];
            --(*count);
            return 0;
        }
    }
    if (*count >= MENU_MAX_FAVORITES) return -1;
    favs[(*count)++] = id;
    return 1;
}

static inline int fav_filter_items(int category, const char *query, int *out_ids, int max_out) {
    int n = 0;
    for (int id = 0; id < FAV_CATALOG_COUNT && n < max_out; ++id) {
        const FavoriteCatalogItem *it = fav_catalog_item(id);
        if (!it) continue;
        if (category != FAV_CAT_ALL && it->category != category) continue;
        if (query && query[0] && !fav_contains_ci(it->label, query) && !fav_contains_ascii_ci(it->key, query) && !fav_contains_ci(it->category_label, query)) continue;
        out_ids[n++] = id;
    }
    return n;
}

static inline void fav_serialize(const int *favs, int count, wchar_t *out, size_t cap) {
    if (!out || cap == 0) return;
    size_t pos = 0;
    out[0] = 0;
    for (int i = 0; i < count; ++i) {
        const FavoriteCatalogItem *it = fav_catalog_item(favs[i]);
        if (!it) continue;
        if (pos > 0 && pos + 1 < cap) out[pos++] = L',';
        for (int j = 0; it->key[j] && pos + 1 < cap; ++j) out[pos++] = (wchar_t)(unsigned char)it->key[j];
    }
    out[pos] = 0;
}

static inline void fav_deserialize(const wchar_t *in, int *favs, int *count) {
    *count = 0;
    if (!in || !in[0]) return;
    const wchar_t *p = in;
    while (*p && *count < MENU_MAX_FAVORITES) {
        while (*p == L',' || *p == L' ' || *p == L'\t') ++p;
        if (!*p) break;
        char token[32];
        int tlen = 0;
        while (*p && *p != L',' && *p != L' ' && *p != L'\t' && tlen + 1 < (int)sizeof(token)) {
            token[tlen++] = (char)(*p & 0x7f);
            ++p;
        }
        while (*p && *p != L',') ++p;
        token[tlen] = 0;
        for (int id = 0; id < FAV_CATALOG_COUNT; ++id) {
            const FavoriteCatalogItem *it = fav_catalog_item(id);
            if (it && fav_contains_ascii_ci(it->key, token) && fav_contains_ascii_ci(token, it->key)) {
                if (!fav_is_pinned(favs, *count, id)) favs[(*count)++] = id;
                break;
            }
        }
    }
}
enum {
    FAV_CAROUSEL_COUNT = 36
};

static inline char fav_carousel_char(int idx) {
    int i = ((idx % FAV_CAROUSEL_COUNT) + FAV_CAROUSEL_COUNT) % FAV_CAROUSEL_COUNT;
    return (i < 26) ? (char)('A' + i) : (char)('0' + (i - 26));
}

static inline void fav_carousel_label(int idx, wchar_t *out, size_t cap) {
    if (!out || cap < 2) return;
    out[0] = (wchar_t)(unsigned char)fav_carousel_char(idx);
    out[1] = 0;
}

static inline int fav_query_append(char ch, char *query, size_t cap) {
    if (!query || cap == 0) return 0;
    size_t len = 0;
    while (query[len] && len + 1 < cap) ++len;
    if (len + 1 >= cap) return 0;
    if (ch == ' ' && (len == 0 || query[len - 1] == ' ')) return 0;
    query[len] = (char)fav_ascii_lower((int)(unsigned char)ch);
    query[len + 1] = 0;
    return 1;
}

static inline int fav_carousel_apply(int idx, char *query, size_t cap) {
    return fav_query_append(fav_carousel_char(idx), query, cap);
}

static inline int fav_query_backspace(char *query) {
    if (!query || !query[0]) return 0;
    size_t len = 0;
    while (query[len]) ++len;
    query[len - 1] = 0;
    return 1;
}
#endif
