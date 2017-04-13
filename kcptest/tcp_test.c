#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "ikcp.h"
#include "isock.h"
#include "common.h"

/*
 * 0: 默认模式，类似 TCP：正常模式，无快速重传，常规流控
 * 1: 普通模式，关闭流控等
 * 2: 快速模式，所有开关都打开，且关闭流控
 */
#define AUTO_MODE (0)

static pthread_t g_server_pid = -1;
static char input[BUF_SIZE] = {0};

static int auto_test_srv(const char *ip, int mode)
{
	if (isock_init(ip) == ERR)
	{
		goto failed;
	}

	char buffer[BUF_SIZE];
	int hr;
	ikcpcb *kcp = isock_get_kcp();
	IUINT32 current = 0;

	// 配置窗口大小：平均延迟200ms，每20ms发送一个包，
	// 而考虑到丢包重发，设置最大收发窗口为128
	ikcp_wndsize(kcp, 128, 128);

	// 判断测试用例的模式
	if (mode == 0) {
		// 默认模式
		ikcp_nodelay(kcp, 0, 10, 0, 0);
	}
	else if (mode == 1) {
		// 普通模式，关闭流控等
		ikcp_nodelay(kcp, 0, 10, 0, 1);
	}	else {
		// 启动快速模式
		// 第二个参数 nodelay-启用以后若干常规加速将启动
		// 第三个参数 interval为内部处理时钟，默认设置为 10ms
		// 第四个参数 resend为快速重传指标，设置为2
		// 第五个参数 为是否禁用常规流控，这里禁止
		ikcp_nodelay(kcp, 1, 10, 2, 1);
	}

	IUINT32 ts1 = iclock();

	while (1) {
		isleep(1);
		current = iclock();

		// 接收到任何包都返回回去
		while (1) {
			hr = isock_recv(buffer, 10);
			// 没有收到包就退出
			if (hr < 0) break;
			// 如果收到包就回射
			isock_send(buffer, hr);
		}
	}

	ts1 = iclock() - ts1;

	isock_destroy();

	const char *names[3] = { "default", "normal", "fast" };
	printf("%s mode result (%dms):\n", names[mode], (int)ts1);
	printf("tx=%u\n", isock_get_tx());
	printf("press enter to next ...\n");
	char ch; scanf("%c", &ch);

	return OK;

failed:
	isock_destroy();

	return ERR;
}

static int auto_test_cli(const char *ip, int mode)
{
	if (isock_init(ip) == ERR)
	{
		goto failed;
	}

	ikcpcb *kcp = isock_get_kcp();
	IUINT32 current = iclock();
	IUINT32 slap = current + 20;
	IUINT32 index = 0;
	IUINT32 next = 0;
	IINT64 sumrtt = 0;
	int count = 0;
	int maxrtt = 0;
	char buffer[BUF_SIZE];
	int hr;

	// 配置窗口大小：平均延迟200ms，每20ms发送一个包，
	// 而考虑到丢包重发，设置最大收发窗口为128
	ikcp_wndsize(kcp, 128, 128);

	// 判断测试用例的模式
	if (mode == 0) {
		// 默认模式
		ikcp_nodelay(kcp, 0, 10, 0, 0);
	}
	else if (mode == 1) {
		// 普通模式，关闭流控等
		ikcp_nodelay(kcp, 0, 10, 0, 1);
	}	else {
		// 启动快速模式
		// 第二个参数 nodelay-启用以后若干常规加速将启动
		// 第三个参数 interval为内部处理时钟，默认设置为 10ms
		// 第四个参数 resend为快速重传指标，设置为2
		// 第五个参数 为是否禁用常规流控，这里禁止
		ikcp_nodelay(kcp, 1, 10, 2, 1);
		kcp->rx_minrto = 10;
		kcp->fastresend = 1;
	}

	IUINT32 ts1 = iclock();

	while (1) {
		isleep(1);
		current = iclock();

		// 每隔 20ms，发送数据
		for (; current >= slap; slap += 20) {
			((IUINT32*)buffer)[0] = index++;
			((IUINT32*)buffer)[1] = current;

			// 发送上层协议包
			isock_send(buffer, 8);
		}

		while (1) {
			hr = isock_recv(buffer, 10);
			// 没有收到包就退出
			if (hr < 0) break;
			IUINT32 sn = *(IUINT32*)(buffer + 0);
			IUINT32 ts = *(IUINT32*)(buffer + 4);
			IUINT32 rtt = current - ts;
			
			if (sn != next) {
				// 如果收到的包不连续
				//printf("ERROR sn %d<->%d\n", (int)count, (int)next);
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

	isock_destroy();

	const char *names[3] = { "default", "normal", "fast" };
	printf("%s mode result (%dms):\n", names[mode], (int)ts1);
	printf("avgrtt=%d maxrtt=%d tx=%u\n", (int)(sumrtt / count), (int)maxrtt, isock_get_tx());
	printf("press enter to next ...\n");
	char ch; scanf("%c", &ch);

	return OK;

failed:
	isock_destroy();

	return ERR;
}


void kcp_auto_test(const char *ip, char role)
{
	if (role == 'c')
	{
		auto_test_cli(ip, AUTO_MODE);
	}
	else if (role == 's')
	{
		auto_test_srv(ip, AUTO_MODE);
	}
	else
	{
		LOG_ERROR("Invalid role\n");
		return -1;
	}
}

static void *server_thread_func(void *arg)
{
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

	return NULL;
}


void kcp_manual_test(const char *ip)
{
	isock_init(ip);
	if (0 != pthread_create(&g_server_pid, NULL, server_thread_func, NULL))
	{
		LOG_ERROR("Create server thread failed\n");
	}

	while (1)
	{
		scanf("%s", input);
		isock_send(input, strlen(input));
	}

	isock_destroy();
	pthread_join(g_server_pid, NULL);
	LOG_DEBUG("server end\n");
}

/*
default mode result (20917ms):
avgrtt=740 maxrtt=1507
normal mode result (20131ms):
avgrtt=156 maxrtt=571
fast mode result (20207ms):
avgrtt=138 maxrtt=392
*/
