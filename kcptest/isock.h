/********************************************************************
 * Copyright (c) 2017, TP-LINK TECHNOLOGIES CO., LTD.
 *
 * Brief        : A socket based on KCP
 * Author       : Li Zheng <lizheng_w5625@tp-link.com.cn>
 * Created Date : 2017-03-31
 ********************************************************************/

#ifndef ISOCK_H
#define ISOCK_H

#include "ikcp.h"
#include "common.h"

void isock_init(char *dest_ip);
void isock_destroy(void);
int isock_recv(char *buffer, int len);
int isock_send(const char *buffer, int len);


#endif /* end of include guard: ISOCK_H */
