/********************************************************************
 * Copyright (c) 2017, TP-LINK TECHNOLOGIES CO., LTD.
 *
 * Brief        : A socket based on KCP
 * Author       : Li Zheng <lizheng_w5625@tp-link.com.cn>
 * Created Date : 2017-03-31
 ********************************************************************/

#ifndef ISOCKD_H
#define ISOCKD_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "ikcp.h"
#include "common.h"


void isockd_init(char *dest_ip);
void isockd_destroy(void);
int isockd_recv(char *buffer, int len);
int isockd_send(const char *buffer, int len);


#endif /* end of include guard: ISOCKD_H */
