#ifndef __COMMON_H__
#define __COMMON_H__

 #include <curses.h>

#define MSG(...)    printw(__VA_ARGS__)

#define SCREEN_HEIGHT   (50)
#define SCREEN_WIDTH    (50)

typedef struct _GAME
{
    char* (*brief)(void);
    void (*start)(void);
} GAME;


void registerGame(const GAME *game);
void startGame(void);

#endif /* end of include guard: __COMMON_H__ */
