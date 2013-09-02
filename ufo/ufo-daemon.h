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

#ifndef __UFO_DAEMON_H
#define __UFO_DAEMON_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_DAEMON             (ufo_daemon_get_type())
#define UFO_DAEMON(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_DAEMON, UfoDaemon))
#define UFO_IS_DAEMON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_DAEMON))
#define UFO_DAEMON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_DAEMON, UfoDaemonClass))
#define UFO_IS_DAEMON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_DAEMON))
#define UFO_DAEMON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_DAEMON, UfoDaemonClass))

typedef struct _UfoDaemon           UfoDaemon;
typedef struct _UfoDaemonClass      UfoDaemonClass;
typedef struct _UfoDaemonPrivate    UfoDaemonPrivate;

/**
 * UfoDaemon:
 *
 * TODO: Add documentation
 */
struct _UfoDaemon {
    /*< private >*/
    GObject parent_instance;

    UfoDaemonPrivate *priv;
};

/**
 * UfoDaemonClass:
 *
 * #UfoDaemon class
 */
struct _UfoDaemonClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoDaemon *  ufo_daemon_new               (UfoConfig *config, gchar *listen_addr);
GThread *    ufo_daemon_start             (UfoDaemon *daemon);
void         ufo_daemon_stop              (UfoDaemon *daemon);
GType        ufo_daemon_get_type          (void);

G_END_DECLS

#endif
