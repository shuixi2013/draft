#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "common.h"
#include "tcp_test.h"

#define SRV_PORT		(10527)
#define CLI_PORT		(10419)

static pthread_t g_server_pid = -1;
static int g_sockfd = -1;

static void cleanup(void)
{
	if (g_sockfd > 0)
	{
		close(g_sockfd);
		g_sockfd = -1;
	}
}

static int auto_test_srv(const char *ip)
{
	char buffer[BUF_SIZE];
	int hr;

	g_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket < 0)
	{
		LOG_ERROR("Create socket failed\n");
		goto failed;
	}

	struct sockaddr_in srvaddr;
	bzero(&srvaddr, sizeof(srvaddr));
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	srvaddr.sin_port = htons(SRV_PORT);

	bind(g_sockfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr));
	listen(g_sockfd, 128);

	while (1)
	{
		struct sockaddr_in cliaddr;
		bzero(&cliaddr, sizeof(cliaddr));
		socklen_t cliaddr_len = sizeof(cliaddr);
		int connfd = accept(g_sockfd, (struct sockaddr *)&cliaddr, &cliaddr_len);

		int flag = 1;
		setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

		char str[INET_ADDRSTRLEN];
		printf("connected from %s at PORT %d\n",
				inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
				ntohs(cliaddr.sin_port));

		while(1)
		{
			isleep(1);

			hr = recv(connfd, buffer, 10, 0);
			if (hr < 0) continue;
			send(connfd, buffer, hr, 0);
		}

		close(connfd);
		printf("closed from %s at PORT %d\n",
				inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
				ntohs(cliaddr.sin_port));
	}

	cleanup();

	//TODO
	printf("tx=%u\n", 1);
	printf("press enter to next ...\n");
	char ch; scanf("%c", &ch);

	return OK;

failed:
	cleanup();

	return ERR;
}

static int auto_test_cli(const char *ip)
{
	g_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in srvaddr;
	bzero(&srvaddr, sizeof(srvaddr));
	srvaddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &srvaddr.sin_addr);
	srvaddr.sin_port = htons(SRV_PORT);

	if (0 != connect(g_sockfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr)))
	{
		LOG_ERROR("Connect failed\n");
		goto failed;
	}
	LOG_DEBUG("Connect !!\n");

	int flag = 1;
	setsockopt(g_sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

	IUINT32 current = iclock();
	IUINT32 slap = current + 20;
	IUINT32 index = 0;
	IUINT32 next = 0;
	IINT64 sumrtt = 0;
	int count = 0;
	int maxrtt = 0;
	char buffer[BUF_SIZE];
	int hr;

	IUINT32 ts1 = iclock();

	while (1) {
		isleep(1);
		current = iclock();

		// 每隔 20ms，发送数据
		for (; current >= slap; slap += 20) {
			((IUINT32*)buffer)[0] = index++;
			((IUINT32*)buffer)[1] = current;

			// 发送上层协议包
			send(g_sockfd, buffer, 8, 0);
		}

		while (1) {
			hr = recv(g_sockfd, buffer, 10, MSG_DONTWAIT);
			// 没有收到包就退出
			if (hr < 0) break;
			IUINT32 sn = *(IUINT32*)(buffer + 0);
			IUINT32 ts = *(IUINT32*)(buffer + 4);
			IUINT32 rtt = current - ts;
			
			if (sn != next) {
				// 如果收到的包不连续
				printf("ERROR sn %d<->%d\n", (int)count, (int)next);
				goto failed;
			}

			next++;
			sumrtt += rtt;
			count++;
			if (rtt > (IUINT32)maxrtt) maxrtt = rtt;

			//printf("[RECV] mode=%d sn=%d rtt=%d\n", mode, (int)sn, (int)rtt);
		}
		if (next > 1000) break;
	}

	ts1 = iclock() - ts1;

	cleanup();

	printf("result (%dms):\n", (int)ts1);
	//TODO
	printf("avgrtt=%d maxrtt=%d tx=%u\n", (int)(sumrtt / count), (int)maxrtt, 1);
	printf("press enter to next ...\n");
	char ch; scanf("%c", &ch);

	return OK;

failed:
	cleanup();

	return ERR;
}


void tcp_auto_test(const char *ip, char role)
{
	if (role == 'c')
	{
		auto_test_cli(ip);
	}
	else if (role == 's')
	{
		auto_test_srv(ip);
	}
	else
	{
		LOG_ERROR("Invalid role\n");
	}
}

static void *server_thread_func(void *arg)
{
/*
	char recv[BUF_SIZE];
	int recv_len;

	LOG_DEBUG("server_thread_func\n");

	while (1)
	{
		isleep(1000);
		recv_len = isock_recv(recv, BUF_SIZE);
		if (recv_len > 0)
		{
			LOG_DEBUG("recv: %s\n", recv);
			memset(recv, 0, BUF_SIZE);
		}
	}

*/
	return NULL;
}

void tcp_manual_test(const char *ip)
{
	/*
	char input[BUF_SIZE] = {0};

	isock_init(ip);
	if (0 != pthread_create(&g_server_pid, NULL, server_thread_func, NULL))
	{
		LOG_ERROR("Create server thread failed\n");
	}

	while (1)
	{
		memset(input, 0, BUF_SIZE);
		scanf("%s", input);
		isock_send(input, strlen(input));
	}

	isock_destroy();
	pthread_join(g_server_pid, NULL);
	LOG_DEBUG("server end\n");
	*/
}

/*
default mode result (20917ms):
avgrtt=740 maxrtt=1507
normal mode result (20131ms):
avgrtt=156 maxrtt=571
fast mode result (20207ms):
avgrtt=138 maxrtt=392
*/
