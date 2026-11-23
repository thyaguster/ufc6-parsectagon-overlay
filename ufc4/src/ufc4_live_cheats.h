#ifndef UFC4_LIVE_CHEATS_H
#define UFC4_LIVE_CHEATS_H

/* The mask is session-only: bit 0 is the left fighter, bit 1 the right. */
int ufc4_live_stamina_prepare(void);
unsigned ufc4_live_stamina_mask(void);
int ufc4_live_stamina_toggle(int fighter,unsigned *mask);

/* Player 1 strike-damage protection is session-only; bit 1 is the tested actor. */
int ufc4_live_damage_prepare(void);
unsigned ufc4_live_damage_mask(void);
int ufc4_live_damage_toggle(unsigned *mask);

/* One-shot opponent strike: 0=Off, 1=armed, 2=triggered, 3=cancelled/ended. */
int ufc4_live_ko_prepare(void);
int ufc4_live_ko_toggle(unsigned *armed);
int ufc4_live_ko_poll(void);
void ufc4_live_ko_start_window(void);

#endif
