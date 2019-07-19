#!/usr/bin/env python
# -*- coding: utf-8 -*-


import os


def count_file_lines(filename):
    """
    快速统计文件行数
    :param filename:
    :return:
    """
    count = 0
    fp = open(filename, "rb")
    byte_n = bytes("\n", encoding="utf-8")
    while 1:
        buffer = fp.read(16*1024*1024)
        if not buffer:
            count += 1  # 包含最后一行空行 ''
            break
        count += buffer.count(byte_n)
    fp.close()
    return count


g = os.walk("./abc/src")
totLen = 0
for path, dir_list, file_list in g:
    for file_name in file_list:
        extension = os.path.splitext(file_name)[-1]
        if extension == ".h" or extension == ".c":
            ss = str(os.path.join(path, file_name))
            ssLen = count_file_lines(ss)
            totLen += ssLen
            print(ss, ssLen)
print("total line = ", totLen)
