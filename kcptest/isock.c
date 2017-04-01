/********************************************************************
 * Copyright (c) 2017, TP-LINK TECHNOLOGIES CO., LTD.
 *
 * Brief        : A socket based on KCP
 * Author       : Li Zheng <lizheng_w5625@tp-link.com.cn>
 * Created Date : 2017-03-31
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ikcp.h"
#include "isock.h"


static ikcpcb *g_kcp = NULL;
static pthread_t g_server_pid = -1;
static int g_server_sock = -1;
static int g_client_sock = -1;
static char g_dest_ip[IP_SIZE] = {0};

static int client_udp_init(void);
static void client_udp_destroy(void);
static int server_udp_init(void);
static void server_udp_destroy(void);
static void server_run(void);
static void *server_thread_func(void *arg);
static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user);

static void server_run(void)
{
	char recv_buf[BUF_SIZE] = {0};
	int recv_len = 0;
	int sock_flag = 0;
	IUINT32 current = 0;
	IUINT32 check_time = 0;

	sock_flag |= MSG_DONTWAIT;

	while (1)
	{
		isleep(20);
		current = iclock();
		if (current >= check_time)
		{
			ikcp_update(g_kcp, current);
			check_time = ikcp_check(g_kcp, current);
		}

		while (1)
		{
			recv_len = recv(g_server_sock, recv_buf, BUF_SIZE, sock_flag);
			if (recv_len < 0)
			{
				break;
			}
			ikcp_input(g_kcp, recv_buf, recv_len);
		}
	}
}

static void *server_thread_func(void *arg)
{
	pthread_detach(pthread_self());
	server_run();

	return NULL;
}

/******************************************************************************
 * FUNCTION    : server_udp_init
 * DESCRIPTION : handle remote -> local data
 * RETURN      : return 0 if success, else return -1
 *****************************************************************************/
static int server_udp_init(void)
{
	struct sockaddr_in addr;

	g_server_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == g_server_sock)
	{
		LOG_ERROR("Create server socket failed: %s\n", strerror(errno));
		goto failed;
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port        = htons(KCP_UDP_SRV_PORT);

	if (-1 == bind(g_server_sock, (struct sockaddr*)&addr, (socklen_t)sizeof(addr)))
	{
		LOG_ERROR("Bind server port failed\n");
		goto failed;
	}

	/* create a thread to handle message from client */
	if (0 != pthread_create(&g_server_pid, NULL, server_thread_func, NULL))
	{
		LOG_ERROR("Create server thread failed\n");
		goto failed;
	}

	LOG_ERROR("server ok\n");

	return 0;

failed:
	server_udp_destroy();

	return -1;
}

/******************************************************************************
 * FUNCTION    : client_udp_init
 * DESCRIPTION : handle local -> remote data
 * RETURN      : return 0 if success, else return -1
 *****************************************************************************/
static int client_udp_init(void)
{
	struct sockaddr_in addr;

	bzero(&addr, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = inet_addr(g_dest_ip);
	addr.sin_port        = htons(KCP_UDP_SRV_PORT);

	g_client_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == g_client_sock)
	{
		LOG_ERROR("Create NR socket failed: %s\n", strerror(errno));
		goto failed;
	}

	if (-1 == connect(g_client_sock, (struct sockaddr*)&addr,
				(socklen_t)sizeof(addr)))
	{
		LOG_ERROR("Connect to server failed\n");
		goto failed;
	}

	LOG_ERROR("client ok\n");
	return 0;

failed:
	client_udp_destroy();

	return -1;
}

/***************************************************************
 * FUNCTION     : udp_output
 * DESCRIPTION  : send data via UDP
 * RETURN       : On success, these calls return the number of
 *                bytes sent.  On error, -1 is returned
 ***************************************************************/
static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	int ret = send(g_client_sock, buf, len, 0);

	if (-1 == ret)
	{
		LOG_DEBUG("Send packet failed: %s\n", strerror(errno));
	}

	return ret;
}

static void server_udp_destroy(void)
{
	if (-1 != g_server_sock)
	{
		close(g_server_sock);
		g_server_sock = -1;
	}

	if (-1 != g_server_pid)
	{
		pthread_cancel(g_server_pid);
		g_server_pid = -1;
	}

}

static void client_udp_destroy(void)
{
	if (-1 != g_client_sock)
	{
		close(g_client_sock);
		g_client_sock = -1;
	}
}

void isock_init(char *dest_ip)
{
	if (dest_ip == NULL)
	{
		LOG_ERROR("Dest IP invalid: %s\n", dest_ip);
		goto failed;
	}

	strncpy(g_dest_ip, dest_ip, IP_SIZE);

	server_udp_init();
	client_udp_init();

	if (g_server_sock == -1 || g_client_sock == -1)
	{
		LOG_ERROR("Init socket failed.\n");
		goto failed;
	}

	g_kcp = ikcp_create(CONV, (void *)0);

	ikcp_setoutput(g_kcp, udp_output);

failed:
	isock_destroy();
}

void isock_destroy(void)
{
	client_udp_destroy();
	server_udp_destroy();

	if (g_kcp != NULL)
	{
		ikcp_release(g_kcp);
	}
}

int isock_recv(char *buffer, int len)
{
	return ikcp_recv(g_kcp, buffer, len);
}

int isock_send(const char *buffer, int len)
{
	LOG_ERROR("send: %s\n", buffer);
	//return send(g_client_sock, buffer, len, 0);
	return ikcp_send(g_kcp, buffer, len);
}
