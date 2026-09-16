// SPDX-License-Identifier: MIT
//
// The eleven original Block Dude levels, by Brandon Sterner.
//
// Each level is plain text, one string per row, so a level can be edited in any
// text editor and read at a glance. The legend is:
//
//     '#' wall      'o' block      'D' door
//     ' ' empty     '<' dude facing left      '>' dude facing right
//
// Rows may be ragged: anything past the end of a short row is treated as empty,
// and levels_get() reports the widest row as the level width. Exactly one dude
// marker must appear in a level. The row count in the table below is derived
// from each array with ARRAY_ROWS so it can never drift from the text, and
// test/host/test_game.c checks every bundled level for a dude, a door and at
// least one block.

#include "levels.h"

//! Row count of a level array, so the table can never disagree with the text.
#define ARRAY_ROWS(a) ((int)(sizeof(a) / sizeof((a)[0])))


static const char *const LEVEL_1[] = {
  "##    #            #",
  "# #   #            #",
  "#  ###             #",
  "#                  #",
  "#   #       #      #",
  "#D  #   # o # o <  #",
  "####################",
};

static const char *const LEVEL_2[] = {
  " #    ##        ##",
  " #                #",
  "##                 #",
  "#D                  #",
  "##                   #",
  " #           #  o    #",
  " #           #o oo<  #",
  " #####   #############",
  "     #  o#",
  "     #####",
};

static const char *const LEVEL_3[] = {
  " #",
  " #   #############",
  "# # #             #",
  "#  #              #",
  "#                o#",
  "#               oo#",
  "# ###    <   #o ##",
  "# # #    #  #####",
  "# # #oo ##  #",
  "#D# ###### ##",
  "### ##   ###",
};

static const char *const LEVEL_4[] = {
  "                  #",
  "                 # #",
  "       #        #   #",
  "      # #      #     #",
  "   ###   #    #       #",
  "  #       #  #         #",
  " #         ##          #",
  " #                    o#",
  " #                   oo#",
  " #               <   ###",
  "##    #          #   #",
  "#D    # o        #####",
  "##### # o   o  ###",
  "    # # o # #o #",
  "    # ##########",
  "    ###",
};

static const char *const LEVEL_5[] = {
  "     ###    #########",
  " ####   ####         #",
  "#                    #",
  "#                    #",
  "#                    #",
  "#     #              #",
  "#     #              #",
  "#     #oooo          #",
  "#D   #######<        #",
  "## ###     ## #     o#",
  " # #        # ##   oo#",
  " # #        # ##  ooo#",
  " ###        # ########",
  "            ###",
};

static const char *const LEVEL_6[] = {
  " ###             ####",
  " #  #############   #",
  "##                  #",
  "#D                  #",
  "##                  #",
  " #                oo#",
  " #oo        #  o  ###",
  " #ooo       #<ooo #",
  " #oooo      ##### #",
  " #####    ###   ###",
  "     #   o#",
  "     ## ###",
  "      ###",
};

static const char *const LEVEL_7[] = {
  "  #   #####   ##   ###",
  " # # #     # #  # #   #",
  " #  ##      ##   ##    #",
  " #   #       #    #    #",
  " #                    o#",
  " #                    o#",
  "##                   oo#",
  "#D   o               ###",
  "##   # o     #    ## #",
  " #   # o    ## o <####",
  " ##  # ooo  ## ooo#",
  "  #  ###### #######",
  "  ## #    ###",
  "   ###",
};

static const char *const LEVEL_8[] = {
  " ###       ####   #######",
  "#   #     #    # #       #",
  "#    #   #     ##         #",
  "#o    ###    # #     ###  #",
  "#oo         ##      ## #  #",
  "####       ##          #D #",
  "   ##            ##    ## #",
  "  #    o #      #  #      #",
  "  #    o# #    #   #      #",
  " #   ###   #    #  #     o#",
  " #      # #      ##     oo#",
  "#        #           ######",
  "#            o            #",
  "#    o      ###          o#",
  "#   ###                 oo#",
  "#        o       o  <  ooo#",
  "###########################",
};

static const char *const LEVEL_9[] = {
  "        ###",
  "       #   #",
  "      #     #  #####",
  "     #       ##    #",
  "    #     o        #",
  "   #      oo      o#",
  "  #       ###    oo#",
  " #            < ####",
  "#             o    #",
  "#D           ###   #",
  "##    ##   #      o#",
  " #    ##o  ##   ####",
  " #    #######  ##",
  " ###  #     # ##",
  "   # ##     ###",
  "   ###",
};

static const char *const LEVEL_10[] = {
  "   #####################",
  " ##           #         #",
  "####o       oo#o   ooo o##",
  "#  ##  #   #####  o### ## #",
  "#   #  ##        ### ###  #",
  "#   ##  ##oooo            #",
  "#D       #######          #",
  "##        #   ###        ##",
  " #     o   # #  ##        #",
  " #     #    #    ##       #",
  " ####  ##             #####",
  "   #####      <           #",
  "   #          #           #",
  "   #         ##    ########",
  "   #        ##           #",
  "   #          o         o#",
  "   #o    ###########   oo#",
  "   #oo  ##         ## ooo#",
  "   ######           ######",
};

static const char *const LEVEL_11[] = {
  "#############################",
  "#  #   #                    #",
  "#     o#oo            ##### #",
  "#o   ### o##     o  ##  D # #",
  "#oo    ###   <  o       # # #",
  "###  oo#     # o          # #",
  "#   ####      #  ###   ###  #",
  "#o            # #      #  o #",
  "#oo       ### # #o    #  ####",
  "#### o   ###  # ##o  # o #  #",
  "#           o ###  o#   #   #",
  "#   o     oo #   ####       #",
  "#    #########        ##### #",
  "#              o   o##    # #",
  "####           o   #    oo# #",
  "#o##   #    #          #### #",
  "##o### #    #   ooo o       #",
  "#o#o#o##    #        ooo    #",
  "#############################",
};

static const Level LEVELS[] = {
  { LEVEL_1, ARRAY_ROWS(LEVEL_1) },
  { LEVEL_2, ARRAY_ROWS(LEVEL_2) },
  { LEVEL_3, ARRAY_ROWS(LEVEL_3) },
  { LEVEL_4, ARRAY_ROWS(LEVEL_4) },
  { LEVEL_5, ARRAY_ROWS(LEVEL_5) },
  { LEVEL_6, ARRAY_ROWS(LEVEL_6) },
  { LEVEL_7, ARRAY_ROWS(LEVEL_7) },
  { LEVEL_8, ARRAY_ROWS(LEVEL_8) },
  { LEVEL_9, ARRAY_ROWS(LEVEL_9) },
  { LEVEL_10, ARRAY_ROWS(LEVEL_10) },
  { LEVEL_11, ARRAY_ROWS(LEVEL_11) },
};

int levels_count(void) { return (int)(sizeof(LEVELS) / sizeof(LEVELS[0])); }

const Level *levels_get(int index) {
  if (index < 0 || index >= levels_count()) return 0;
  return &LEVELS[index];
}
