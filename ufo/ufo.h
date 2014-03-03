/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
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

#ifndef __UFO_H
#define __UFO_H

#define __UFO_H_INSIDE__

#include <ufo/ufo-arch-graph.h>
#include <ufo/ufo-buffer.h>
#include <ufo/ufo-buffer-pool.h>
#include <ufo/ufo-config.h>
#include <ufo/ufo-configurable.h>
#include <ufo/ufo-cpu-node.h>
#include <ufo/ufo-cpu-task-iface.h>
#include <ufo/ufo-dummy-task.h>
#include <ufo/ufo-daemon.h>
#include <ufo/ufo-enums.h>
#include <ufo/ufo-gpu-node.h>
#include <ufo/ufo-gpu-task-iface.h>
#include <ufo/ufo-graph.h>
#include <ufo/ufo-group.h>
#include <ufo/ufo-input-task.h>
#include <ufo/ufo-node.h>
#include <ufo/ufo-output-task.h>
#include <ufo/ufo-plugin-manager.h>
#include <ufo/ufo-profiler.h>
#include <ufo/ufo-remote-node.h>
#include <ufo/ufo-remote-task.h>
#include <ufo/ufo-resources.h>
#include <ufo/ufo-scheduler.h>
#include <ufo/ufo-task-graph.h>
#include <ufo/ufo-task-iface.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-two-way-queue.h>
#include <ufo/ufo-basic-ops.h>
#include <ufo/ufo-messenger-iface.h>

#undef __UFO_H_INSIDE__

#endif
