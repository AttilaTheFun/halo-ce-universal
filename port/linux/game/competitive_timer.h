#ifndef COMPETITIVE_TIMER_H
#define COMPETITIVE_TIMER_H

/* A competitive map's match clock, and the globals its announcer waits on.
   Classic games are left alone. */

/* After the map's scripts have run for this tick. The startup script turns
   the announcer off, and the clock script sleeps for good if that stands. */
void competitive_timer_after_scripts(void);

/* The match clock, bottom right, as one time. Nothing is drawn over a menu
   or outside a competitive map. */
void competitive_timer_render(void);

#endif
