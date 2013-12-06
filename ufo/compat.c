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

#include <glib.h>


#if !GLIB_CHECK_VERSION(2, 31, 0)
gpointer
g_async_queue_timeout_pop (GAsyncQueue *queue,
                           guint64 timeout)
{
    GTimeVal end_time;

    g_get_current_time (&end_time);
    g_time_val_add (&end_time, timeout);
    return g_async_queue_timed_pop (queue, &end_time);
}
#endif
