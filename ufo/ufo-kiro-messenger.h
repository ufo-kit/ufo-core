/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
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

#ifndef __UFO_KIRO_MESSENGER_H
#define __UFO_KIRO_MESSENGER_H

#include <ufo/ufo-remote-node.h>
#include <ufo/ufo-messenger-iface.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_KIRO_MESSENGER             (ufo_kiro_messenger_get_type())
#define UFO_KIRO_MESSENGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_KIRO_MESSENGER, UfoKiroMessenger))
#define UFO_IS_KIRO_MESSENGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_KIRO_MESSENGER))
#define UFO_KIRO_MESSENGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_KIRO_MESSENGER, UfoKiroMessengerClass))
#define UFO_IS_KIRO_MESSENGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_KIRO_MESSENGER))
#define UFO_KIRO_MESSENGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_KIRO_MESSENGER, UfoKiroMessengerClass))

typedef struct _UfoKiroMessenger           UfoKiroMessenger;
typedef struct _UfoKiroMessengerClass      UfoKiroMessengerClass;
typedef struct _UfoKiroMessengerPrivate    UfoKiroMessengerPrivate;

struct _UfoKiroMessenger {
    /*< private >*/
    GObject parent_instance;

    UfoKiroMessengerPrivate *priv;
};

struct _UfoKiroMessengerClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoKiroMessenger    *ufo_kiro_messenger_new           (void);
GType               ufo_kiro_messenger_get_type      (void);

G_END_DECLS

#endif
