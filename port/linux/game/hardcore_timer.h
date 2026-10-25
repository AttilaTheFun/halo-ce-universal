#ifndef HARDCORE_TIMER_H
#define HARDCORE_TIMER_H

/* The match clock, and a map announcer's talking_timer. Graphics' Match
   Timer chooses off, on, or on with voice. */

/* 0 off, 1 on, 2 on with voice. */
int hardcore_timer_mode(void);

/* After the map's scripts have run for this tick. On with Voice turns
   talking_timer on when that global exists. Off and On leave it as the
   map authored it, and this does not force power_call. */
void hardcore_timer_after_scripts(void);

/* The match clock, bottom right, as one time. Drawn in multiplayer when
   the client clock is on. Nothing is drawn over a menu. */
void hardcore_timer_render(void);

/* The top of the clock's digits, in the 480-tall screen. The random
   percent uses it so the two sit on one line. */
short hardcore_timer_top(void);

#endif
