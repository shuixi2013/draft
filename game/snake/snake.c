/*
 * Copyright © 2016 LiR <6007381@qq.com>. All Rights Reserved.
 *
 * Brief: The game: Marbles, for 1 player.
 */

/*
 * Headers
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "common.h"
#include "list.h"

/*
 * Definitions
 */
typedef struct _POINT
{
    int x;
    int y;
} POINT;

typedef struct _SnakeNode
{
    POINT position;
    struct list_head list;
} SnakeNode;

typedef struct _BAT
{
    POINT position;
    int len;
} BAT;


/*
 * Variables
 */
static int  g_level = 0;
static int  g_score = 0;
static BAT g_ball;
static BAT  g_bat;
static bool g_gameOver = FALSE;
static POINT g_food;

LIST_HEAD(g_snake);

/*
 * Declarations
 */
static void drawMap(int maxY, int maxX);
static void initMap(int maxY, int maxX);
static void calBall(int maxY, int maxX);
static void calBat(int maxY, int maxX, char key);
static void gameOver(int maxY, int maxX);


/*
 * Implementations
 */
static void eatFood(POINT food)
{
    SnakeNode *node;
    node = (SnakeNode *)malloc(sizeof(SnakeNode));
    if (!node) {
        return;
    }

    node->position.y = food.y;
    node->position.x = food.x;

    list_add(&(node->list), &g_snake);
}

static char* brief(void)
{
    static char str[] = "The Game of Marbles(for 1 player).";
    return str;
}

void* graphThread(void* unused)
{
    int y, x;
    getmaxyx(stdscr, y, x);

    while (!g_gameOver) {
        clear();
        drawMap(y, x);
        refresh();
        calBall(y, x);
        usleep(100000);
    }

    gameOver(y, x);

    return NULL;
}

static void start(void)
{
    int y, x;
    char key = 0;
    pthread_t graphThreadID;

    getmaxyx(stdscr, y, x);
    initMap(y, x);

    pthread_create(&graphThreadID, NULL, &graphThread, NULL);

    while (!g_gameOver) {
        if (halfdelay(5) != ERR) {
            key = getch();
            if (key == 'q') {
                cbreak();
                g_gameOver = TRUE;
                break;
            } else {
                calBat(y, x, key);
            }
        }
    }

    pthread_join(graphThreadID, NULL);
}

static void drawInfo(int maxY, int maxX)
{
    int y = 1;

    if (maxX < 15 || maxY < 3) {
        return;
    }
    mvprintw(y++, maxX - 15, "Level %d.", g_level);
    mvprintw(y++, maxX - 15, "Score %d.", g_score);
}

static void drawBall(int maxY, int maxX)
{
    char body = '*';
    mvaddch(g_ball.position.y, g_ball.position.x, body);
}

static void drawBat(int maxY, int maxX)
{
    char body = '#';
    int i;

    for (i = 0; i < g_bat.len; i++) {
        mvaddch(g_bat.position.y, g_bat.position.x + i, body);
    }
}

static void drawMap(int maxY, int maxX)
{
    drawInfo(maxY, maxX);
    drawBall(maxY, maxX);
    drawBat(maxY, maxX);
}

static void gameOver(int maxY, int maxX)
{
    int y = maxY / 2;
    int x = maxX / 2;

    g_gameOver = TRUE;
    clear();
    mvprintw(y++, x - 15, "Game Over");
    mvprintw(y++, x - 5, "Level %d.", g_level);
    mvprintw(y++, x - 5, "Score %d.", g_score);
    refresh();
    getch();
}

static void calBall(int maxY, int maxX)
{
    if (g_ball.speed.y > 0) {
        if (g_ball.position.y + g_ball.speed.y >= maxY - 1) {
            if (g_ball.position.x >= g_bat.position.x &&
                    g_ball.position.x <= g_bat.position.x + g_bat.len) {
                g_ball.position.y = maxY - 1;
                g_ball.speed.y *= -1;
                g_score++;
                if (g_bat.len > 5) {
                    g_level++;
                    g_bat.len--;
                }
            } else {
                g_gameOver = TRUE;
            }
        } else {
            g_ball.position.y += g_ball.speed.y;
        }
    } else {
        if (g_ball.position.y + g_ball.speed.y <= 1) {
            g_ball.position.y = 1;
            g_ball.speed.y *= -1;
        } else {
            g_ball.position.y += g_ball.speed.y;
        }
    }

    if (g_ball.speed.x > 0) {
        if (g_ball.position.x + g_ball.speed.x >= maxX - 1) {
            g_ball.position.x = maxX - 1;
            g_ball.speed.x *= -1;
        } else {
            g_ball.position.x += g_ball.speed.x;
        }
    } else {
        if (g_ball.position.x + g_ball.speed.x <= 1) {
            g_ball.position.x = 1;
            g_ball.speed.x *= -1;
        } else {
            g_ball.position.x += g_ball.speed.x;
        }
    }
}

static void calBat(int maxY, int maxX, char key)
{
    switch (key) {
        case 'a':
            if (g_bat.position.x > 1) {
                g_bat.position.x -= 4;
            }
            if (g_bat.position.x < 1) {
                g_bat.position.x = 1;
            }
            break;
        case 'd':
            if (g_bat.position.x < maxX - 1) {
                g_bat.position.x += 4;
            }
            if (g_bat.position.x > maxX - 1) {
                g_bat.position.x += maxX - 1;
            }
            break;
        default:
            break;
    }
}

static void initBat(int maxY, int maxX)
{
    g_bat.position.x = 0;
    g_bat.position.y = maxY - 1;
    g_bat.len = 30;
}

static void initBall(int maxY, int maxX)
{
    g_ball.position.x = g_bat.len / 2;
    g_ball.position.y = maxY - 1;

    srand(time(0));
    g_ball.speed.y = (rand() % 3 + 1) * (-1);
    g_ball.speed.x = (rand() % 3 + 1) * ((rand() % 2) ? 1 : -1);
}

static void initMap(int maxY, int maxX)
{
    initBat(maxY, maxX);
    initBall(maxY, maxX);

    g_level = 0;
    g_score = 0;
}

void initMarbles(void)
{
    GAME game = {
        .brief = brief,
        .start = start
    };

    registerGame(&game);
}
