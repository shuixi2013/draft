/********************************************************************
 * Copyright (c) 2017, TP-LINK TECHNOLOGIES CO., LTD.
 *
 * Brief        : Test the KCP and TCP
 * Author       : Li Zheng <lizheng_w5625@tp-link.com.cn>
 * Created Date : 2017-03-30
 ********************************************************************/

#include "common.h"
#include "kcp_test.h"

/*
 * Arguments:
 * - ip string (remote ip)
 * - s/c (server/client)
 * - k/t (kcp/tcp)
 */
int main(int argc, const char *argv[])
{
	char ip[IP_SIZE] = {0};
	char role = 0;
	char proto = 0;

	if (argc < 2)
	{
		LOG_ERROR("Need remote IP\n");
		return -1;
	}

	strncpy(ip, argv[1], IP_SIZE);

	if (argc < 3)
	{
		LOG_DEBUG("Manual test\n");
		kcp_manual_test(ip);
		return 0;
	}

	if (argc < 4)
	{
		LOG_ERROR("Arguments invalid\n");
		return -1;
	}


	role = argv[2][0];
	proto = argv[3][0];

	if (proto == 'k')
	{
		LOG_DEBUG("Auto KCP test\n");
		kcp_auto_test(ip, role);
	}
	else if (proto == 't')
	{
		LOG_DEBUG("Auto TCP test\n");
		tcp_auto_test(ip, role);
	}
	else
	{
		LOG_ERROR("Invalid proto\n");
		return -1;
	}

	return 0;
}
