/*
 * Copyright © 2016 LiR <6007381@qq.com>. All Rights Reserved.
 *
 * Brief: The game: Game of Life.
 */

/*
 * Headers
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "common.h"


/*
 * Variables
 */
static char g_map[SCREEN_WIDTH][SCREEN_HEIGHT] = {{0}};
static int g_round = 0;


/*
 * Declarations
 */
static void drawMap(int row, int col);
static void initMap(void);


/*
 * Implementations
 */
static char* brief(void)
{
    static char str[] = "The Game of Life.";
    return str;
}

static void start(void)
{
    int row, col;

    initMap();
    getmaxyx(stdscr, row, col);

    while (TRUE) {
        clear();

        drawMap(row, col);
        g_round++;
        //if (halfdelay(10) != ERR) {
            if (getch() == 'q') {
                break;
            }
        //}

        refresh();
    }

    cbreak();
}

static void calculateMap(void)
{
    int i, j;
    int map[SCREEN_WIDTH][SCREEN_HEIGHT];
    int count;

    for (i = 0; i < SCREEN_HEIGHT; i++) {
        for (j = 0; j < SCREEN_WIDTH; j++) {
            count = 0;
            /*
             * 1 2 3
             * 4 X 6
             * 7 8 9
             */
            /* 1 */
            if (j > 0 && i > 0) {
                if (g_map[i - 1][j - 1]) {
                    count++;
                }
            }
            /* 2 */
            if (i > 0) {
                if (g_map[i - 1][j]) {
                    count++;
                }
            }
            /* 3 */
            if (i > 0 && j < SCREEN_WIDTH - 1) {
                if (g_map[i - 1][j + 1]) {
                    count++;
                }
            }
            /* 4 */
            if (j > 0) {
                if (g_map[i][j - 1]) {
                    count++;
                }
            }
            /* 6 */
            if (j < SCREEN_WIDTH - 1) {
                if (g_map[i][j + 1]) {
                    count++;
                }
            }
            /* 7 */
            if (i < SCREEN_HEIGHT - 1 && j > 0) {
                if (g_map[i + 1][j - 1]) {
                    count++;
                }
            }
            /* 8 */
            if (i < SCREEN_HEIGHT - 1) {
                if (g_map[i + 1][j]) {
                    count++;
                }
            }
            /* 9 */
            if (i < SCREEN_HEIGHT - 1 && j < SCREEN_WIDTH - 1) {
                if (g_map[i + 1][j + 1]) {
                    count++;
                }
            }

            if (count == 2) {
                map[i][j] = g_map[i][j];
            } else if (count == 3) {
                map[i][j] = 1;
            } else {
                map[i][j] = 0;
            }
        }
    }

    for (i = 0; i < SCREEN_HEIGHT; i++) {
        for (j = 0; j < SCREEN_WIDTH; j++) {
            g_map[i][j] = map[i][j];
        }
    }
}

static void drawMap(int row, int col)
{
    int i, j;
    int x, y;

    x = 0;
    y = 0;
    mvprintw(y++, col /2 - 10, "Round %d.", g_round);

    y += 2;
    for (i = 0; i < SCREEN_HEIGHT; i++) {
        for (j = 0; j < SCREEN_WIDTH; j++) {
            if (g_map[i][j]) {
                mvaddch(y, x, '*');
            }
            x += 2;
        }
        y += 1;
        x = 0;
    }
    calculateMap();
}

static void initMap(void)
{
    int i, j;

    srand(time(0));
    for (i = 0; i < SCREEN_HEIGHT; i++) {
        for (j = 0; j < SCREEN_WIDTH; j++) {
            g_map[i][j] = rand() % 2;
        }
    }
    g_round = 0;
}

void initGOL(void)
{
    GAME game = {
        .brief = brief,
        .start = start
    };

    registerGame(&game);
}
