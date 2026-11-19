#ifndef UFC4_LIVE_CHEATS_H
#define UFC4_LIVE_CHEATS_H

/* The mask is session-only: bit 0 is the left fighter, bit 1 the right. */
int ufc4_live_stamina_prepare(void);
unsigned ufc4_live_stamina_mask(void);
int ufc4_live_stamina_toggle(int fighter,unsigned *mask);

#endif
