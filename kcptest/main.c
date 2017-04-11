/********************************************************************
 * Copyright (c) 2017, TP-LINK TECHNOLOGIES CO., LTD.
 *
 * Brief        : Test KCP
 * Author       : Li Zheng <lizheng_w5625@tp-link.com.cn>
 * Created Date : 2017-03-30
 ********************************************************************/

#include <pthread.h>

#include "common.h"
#include "test.h"

/*
 * arg:
 * dest ip
 * s/c
 */
int main(int argc, const char *argv[])
{
	if (argc <= 1)
	{
		LOG_ERROR("Need remote IP\n");
		return -1;
	}

	if (argc <= 2)
	{
		LOG_DEBUG("Manual test\n");
		manual_test(argv[1]);
		return 0;
	}

	LOG_DEBUG("Auto test\n");
	auto_test(argv[1], argv[2][0]);

	return 0;
}
