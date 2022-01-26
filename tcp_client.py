#!/usr/bin/env python3

import sys
import socket

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((sys.argv[1], int(sys.argv[2])))
    s.sendall(b'hello')
    data = s.recv(1024)
    print(data)

