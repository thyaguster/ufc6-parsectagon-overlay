#ifndef UFC4_CHEATS_CONFIG_H
#define UFC4_CHEATS_CONFIG_H
#include <windows.h>
/* The port links its selected cheats at launch. These are saved choices for
   the next launch, not live edits to the game's memory. */
int ufc4_cheats_read(unsigned *enabled);
int ufc4_cheats_toggle(int fighter,unsigned *enabled);
#ifdef UFC4_TEST
void ufc4_cheats_test_path(const wchar_t *path);
#endif
#endif
