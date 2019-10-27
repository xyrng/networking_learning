/*
 * Author: nonsense
 *
 * Created on Oct 19, 2019
 */
#pragma once


#include <stdio.h>
#include "adt.h"

int parse_packet(rdt_packet *packet, char *buff, ssize_t size);
int send_packet(int fd, rdt_packet *packet);
