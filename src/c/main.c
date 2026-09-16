// SPDX-License-Identifier: MIT

#include <pebble.h>

#include "app.h"
#include "play.h"
#include "screens.h"
#include "store.h"

Settings g_settings = {
  .zoom = 1,          // 24px
  .auto_climb = true,
  .vibrate = true,
};

int main(void) {
  store_init();
  // Opt into the firmware's list navigation for the menus. The play screen holds
  // a raw touch subscription while it is up, which suppresses this bridge
  // exactly where a rightward swipe has to mean "walk right".
  app_touch_navigation_enable(true);

  screens_push_main();

#ifdef CD_SHOT
  // Screenshot harness. Jumps straight to one screen so each can be captured
  // without driving the emulator by hand. Never compiled into a normal build.
  switch (CD_SHOT) {
    case 1: screens_push_levels(); break;
    case 2: screens_push_settings(); break;
    case 3: screens_push_help(); break;
    case 4: play_push(2); break;
    case 5: play_push(2); screens_push_pause(); break;
    case 6: play_push(2); screens_push_win(2, 37, 94, true); break;
    case 7: play_push(0); play_shot_walk(-1, 5); break;
    case 8: g_settings.zoom = 0; play_push(10); break;
    case 9: g_settings.zoom = 2; play_push(10); break;
    default: break;
  }
#endif

  app_event_loop();

  // Empty the stack first. Destroying a window that is still stacked re-exposes
  // the one beneath it and runs its appear handler, which for the board means
  // re-subscribing touch and restarting the clock on a window being freed.
  window_stack_pop_all(false);
  play_deinit();
  screens_deinit();
}
