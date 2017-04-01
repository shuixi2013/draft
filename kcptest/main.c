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
static char g_role = 0;
static pthread_t g_server_pid = -1;
static char input[BUF_SIZE] = {0};

static void (*sock_init)(char *dest_ip) = NULL;
static void (*sock_destroy)(void) = NULL;
static int (*sock_recv)(char *buffer, int len) = NULL;
static int (*sock_send)(const char *buffer, int len) = NULL;

static void *server_thread_func(void *arg)
{
	char recv[BUF_SIZE];
	int recv_len;

	LOG_DEBUG("server_thread_func\n");

	while (1)
	{
		isleep(1000);
		recv_len = sock_recv(recv, BUF_SIZE);
		if (recv_len > 0)
		{
			LOG_DEBUG("recv: %s\n", recv);
		}
	}

	return NULL;
}

void server_process(void)
{
	LOG_DEBUG("server start\n");

	sock_init = isockd_init;
	sock_send = isockd_send;
	sock_recv = isockd_recv;
	sock_destroy = isockd_destroy;
}

void client_process(void)
{
	LOG_DEBUG("client start\n");

	sock_init = isock_init;
	sock_send = isock_send;
	sock_recv = isock_recv;
	sock_destroy = isock_destroy;
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

	sock_init(g_dest_ip);
	if (0 != pthread_create(&g_server_pid, NULL, server_thread_func, NULL))
	{
		LOG_ERROR("Create server thread failed\n");
	}

	while (1)
	{
		scanf("%s", input);
		sock_send(input, strlen(input));
	}

	sock_destroy();
	pthread_join(g_server_pid, NULL);
	LOG_DEBUG("server end\n");

	return 0;
}
