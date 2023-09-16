/*
 * Copyright (C) 2011-2017 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __UFO_ZMQ_COMMON_H
#define __UFO_ZMQ_COMMON_H

#include <stdint.h>

#define ZMQ_MAX_DIMENSIONS                  16
#define ZMQ_MAX_DIMENSION_LENGTH            32768

#define ZMQ_REQUEST_REGISTER                0
#define ZMQ_REQUEST_DATA                    1

#define ZMQ_REPLY_ACK                       0
#define ZMQ_REPLY_STOP                      1

#define ZMQ_ERROR_OKAY                      0
#define ZMQ_ERROR_REGISTRATION_EXPECTED     1
#define ZMQ_ERROR_ALREADY_REGISTERED        2
#define ZMQ_ERROR_NOT_REGISTERED            3
#define ZMQ_ERROR_DATA_ALREADY_SENT         4

typedef struct {
    int32_t id;
    guint8 type;
} __attribute__((packed)) ZmqRequest;

typedef struct {
    guint8 error;
    guint8 type;
} __attribute__((packed)) ZmqReply;


#endif
