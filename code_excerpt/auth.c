/********************************************************************
 * Copyright (c) 2017, TP-LINK TECHNOLOGIES CO., LTD.
 *
 * Brief        : Auth
 * Author       : Li Zheng <lizheng_w5625@tp-link.com.cn>
 * Created Date : 2017-04-05
 ********************************************************************/

#include <stdio.h>
#include <string.h>

#define BUF_LEN		(100)

char input[BUF_LEN] = {0};
char output[BUF_LEN] = {0};
char *pout = NULL;

char strDe[] = "RDpbLfCPsJZ7fiv";
char dic[] = "yLwVl0zKqws7LgKPRQ84Mdt708T1qQ3Ha7xv3H7NyU84p21BriUWBU43odz3iP4rBL3cD02KZciX"
		"TysVXiV8ngg6vL48rPJyAUw0HurW20xqxv9aYb4M9wK1Ae0wlro510qXeU07kV57fQMc8L6aLgML"
		"wygtc0F10a0Dg70TOoouyFhdysuRMO51yY5ZlOZZLEal1h0t9YQW0Ko7oBwmCAHoic4HYbUyVeU3"
		"sfQ1xtXcPcf1aT303wAQhv66qzW";

char* securityEncode(char *input1, char *input2, char *input3)
{
	char *dictionary = input3;
	int len, len1, len2, lenDict;
	int index;
	unsigned char cl = 0xBC, cr = 0xBB;

	len1 = strlen(input1);
	len2 = strlen(input2);
	lenDict = strlen(dictionary);
	len = len1 > len2 ? len1 : len2;

	for (index = 0; index < len; index++)
	{
		cl = 0xBB;
		cr = 0xBB;

		if (index >= len1)
		{
			cr = input2[index];
		}
		else if (index >= len2)
		{
			cl = input1[index];
		}
		else
		{
			cl = input1[index];
			cr = input2[index];
		}

		*pout = dictionary[(cl ^ cr)%lenDict];
		pout++;
	}

	return output;
};

char* orgAuthPwd(char *pwd)
{
	memset(output, 0, BUF_LEN);
	pout = output;
	return securityEncode(pwd, strDe, dic);
};

int main(int argc, const char *argv[])
{
	char pwd[BUF_LEN] = {0};
	strncpy(pwd, argv[1], BUF_LEN);

	printf("%s", orgAuthPwd(pwd));
	return 0;
}
