/*
 * Copyright © 2016 LiR <6007381@qq.com>. All Rights Reserved.
 *
 * Brief: Common functions.
 */

/*
 * Headers
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "gol/gol.h"


/*
 * Definitions
 */
#define GAME_MAX    (10)


/*
 * Variables
 */
static GAME g_games[GAME_MAX] = {{0}};
static int g_gameCount = 0;

/*
 * Implementations
 */
static void init(void)
{
    initscr();
    cbreak();
    noecho();

    initGOL();
}

static void welcome(void)
{
    int i;
    int y = 0;

    clear();
    attron(A_BOLD);
    mvprintw(y++, 0, "Game List: (Choose a game, press 'q' to quit)");
    for (i = 0; i < g_gameCount; i++) {
        mvprintw(y++, 0, "%2d: %s", i, g_games[i].brief());
    }
    attroff(A_BOLD);
    mvprintw(y++, 0, "Input: ");

    refresh();
}

void startGame(void)
{
    char input;

    init();

    while (TRUE) {
        welcome();

        input = getch();
        if (input >= '0' && input <= '9') {
            if (input >= '0' + g_gameCount) {
                printw("There are only %d games.", g_gameCount);
            } else {
                g_games[input - '0'].start();
                continue;
            }
        } else {
            if (input == 'q') {
                break;
            } else {
                printw("Invalid input.");
            }
        }

        refresh();
        sleep(2);
    }

    endwin();
}

void registerGame(const GAME *game)
{
    if (g_gameCount >= GAME_MAX) {
        MSG("Too many games.\n");
        return;
    }

    memcpy((void *)(g_games + g_gameCount), (void *)game, sizeof(GAME));
    g_gameCount++;
}

