#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2017, TENCENT TECHNOLOGIES CO., LTD.
# Brief: A tool of data analyzing
# Author: zaynli

import os

data_path = "/tmp/statistic/"
data_window = data_path + "syn_collection"
local_ip = "192.168.36.129"

total = 0
result = {}

def handle_stream(filename):
    with open(filename, 'r') as f:
        syn_set = {}
        for line in f:
            lines_list = line.split()
            win  = lines_list[0]
            size = lines_list[1]

            # do not track control frame
            if int(size) <= 0:
                continue

            # skip the special line
            if not win.isdigit():
                continue

            syn  = lines_list[2]
            ack  = lines_list[3]

            syn = syn.split(':')[0]
            if syn not in syn_set:
                syn_set[syn] = [size]
            else:
                syn_set[syn].append(size)

        # frame missing rate (re-send frames/total frames)
        # data missing rate (re-send data/total data)
        total_frames = 0
        resend_frames = 0
        total_data = 0
        resend_data = 0
        for k, v in syn_set.items():
            send_times = len(v)
            if send_times > 1:
                resend_frames += send_times - 1
                for data_len in v[1:]:
                    resend_data += int(data_len)
                    total_data += int(data_len)

            total_frames += send_times
            total_data += int(v[0])

        if total_frames <= 0:
            return None

        frame_missing_rate = resend_frames * 100.0 / total_frames
        data_missing_rate = resend_data * 100.0 / total_data
        return [frame_missing_rate, data_missing_rate]
        print syn_set

# track one stream
print "============================"
print "part 1: track stream"
print "============================"
for parent,dirnames,filenames in os.walk(data_path):
    print("frame miss\tdata miss\tstream")
    for filename in filenames:
        # only track local->remote stream
        #sender = filename.split('_')[0]
        #if sender.find(local_ip) < 0:
        #    continue

        full_filename = os.path.join(parent,filename)
        ret = handle_stream(full_filename)
        if not ret:
            #print("bad file: " + full_filename)
            continue

        print("%.2f\t%.2f\t%s" % (ret[0], ret[1], full_filename))


# window statistic
print "============================"
print "part 2: window statistic"
print "============================"
with open(data_window, 'r') as f:
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


print("window\tcount\tratio")
for k, v in result.items():
    print("%s\t%s\t%.2f%%" % (k, v, (v*100.0/total)))
