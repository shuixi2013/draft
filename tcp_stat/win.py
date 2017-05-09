#!/usr/bin/python

file_name = "/tmp/capture_tool_output"
local_ip = "192.168.36.129"

total = 0
result = {}

with open(file_name, 'r') as f:
    for line in f:
        lines_list = line.split()

        line_tuple = lines_list[0]
        line_win = lines_list[1]

        # skip local window
        sender = line_tuple.split('>')[0]
        if sender.find(local_ip) >= 0:
            continue

        total += 1
        if line_win not in result:
            result[line_win] = 1
        else:
            result[line_win] += 1


for k, v in result.items():
    print("%s %s %.2f%%" % (k, v, (v*100.0/total)))
