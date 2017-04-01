/********************************************************************
 * Copyright (c) 2017, TP-LINK TECHNOLOGIES CO., LTD.
 *
 * Brief        : Test KCP
 * Author       : Li Zheng <lizheng_w5625@tp-link.com.cn>
 * Created Date : 2017-03-30
 ********************************************************************/

#include <pthread.h>

#include "common.h"
#include "isock.h"
#include "isockd.h"

static char g_dest_ip[IP_SIZE] = {0};
static char g_role;
static pthread_t g_server_pid = -1;

static void *server_thread_func(void *arg)
{
	char recv[BUF_SIZE];
	int recv_len;

	LOG_DEBUG("server_thread_func\n");

	while (1)
	{
		sleep(1);
		LOG_DEBUG("server\n");
		continue;
		recv_len = isockd_recv(recv, BUF_SIZE);
		if (recv_len > 0)
		{
			printf("recv: %s\n", recv);
		}
	}

	return NULL;
}

void server_process(void)
{
	isockd_init(g_dest_ip);
	LOG_DEBUG("server start\n");

	if (0 != pthread_create(&g_server_pid, NULL, server_thread_func, NULL))
	{
		LOG_ERROR("Create server thread failed\n");
	}
}

void client_process(void)
{
	char input[BUF_SIZE];

	isock_init(g_dest_ip);
	LOG_DEBUG("client start\n");

	while (1)
	{
		scanf("%s", input);
		isock_send(input, strlen(input));
	}

	LOG_DEBUG("client end\n");
	isock_destroy();
}

/*
 * arg:
 * dest ip,
 * s/c
 */
int main(int argc, const char *argv[])
{
	strncpy(g_dest_ip, argv[1], IP_SIZE);
	g_role = argv[2][0];

	if (g_role == 's')
	{
		server_process();
	}
	else if (g_role == 'c')
	{
		client_process();
	}

	LOG_DEBUG("server join\n");
	pthread_join(g_server_pid, NULL);
	LOG_DEBUG("server end\n");

	isockd_destroy();

	return 0;
}
