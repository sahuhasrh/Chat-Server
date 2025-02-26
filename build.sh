#!/bin/bash
gcc -o server server.c -pthread
gcc -o client client.c -pthread
