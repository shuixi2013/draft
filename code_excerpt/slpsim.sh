#!/bin/bash
#
# Copyright (c) , TP-LINK TECHNOLOGIES CO., LTD.
# Brief:  Simulate SLP request
# Author: LiZheng <lizheng_w5625@tp-link.com.cn>


####################
# Custom settings
####################
http="http://192.168.1.240"
passwd="123456"

# eg. ipc, cpe, nvr
dev=nvr

# request, input a file in json format.
request=`cat $1`;


####################
# Send SLP request
####################
digest=`./auth $passwd`
stok=

if [ "x$request" == "x" ]; then
	echo "ERR: request not gotten!";
	exit;
fi

case $dev in
	ipc)
		stok=`curl -e "$http" $http -d "{\"method\":\"do\",\"login\":{\"username\":\"admin\",\"password\":\"$digest\"}}" 2>/dev/null | sed "s/{ \"stok\": \"//g" | sed "s/\", \"user_group\": \"root\", \"error_code\": 0 }//g"`;
		;;
	cpe)
		stok=`curl -e "$http" $http -d "{\"method\":\"do\",\"login\":{\"username\":\"admin\",\"password\":\"$digest\"}}" 2>/dev/null | sed "s/{\"stok\":\"//g" | sed "s/\",\"error_code\":0}//g"`;
		;;
	nvr)
		stok=`curl -e "$http" $http -d "{\"method\":\"do\",\"login\":{\"username\":\"admin\",\"password\":\"$digest\"}}" 2>/dev/null | sed "s/{ \"stok\": \"//g" | sed "s/\", \"user_group\": \"root\", \"error_code\": 0 }//g"`;
		;;
esac

if [ "x$stok" == "x" ]; then
	echo "ERR: stok not gotten!";
	exit;
fi

response=`curl -e "$http" "$http/stok=$stok/ds" -d "$request" 2>/dev/null`;

if [ "x$response" == "x" ]; then
	echo "ERR: response not gotten!";
	exit;
fi

####################
# Output
####################
echo request:
echo $request | python -m json.tool;
echo response:
echo $response | python -m json.tool;

# END
