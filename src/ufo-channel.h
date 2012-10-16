#ifndef __UFO_CHANNEL_H
#define __UFO_CHANNEL_H

#include <glib-object.h>
#include "ufo-buffer.h"

G_BEGIN_DECLS

#define UFO_TYPE_CHANNEL             (ufo_channel_get_type())
#define UFO_CHANNEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_CHANNEL, UfoChannel))
#define UFO_IS_CHANNEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_CHANNEL))
#define UFO_CHANNEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_CHANNEL, UfoChannelClass))
#define UFO_IS_CHANNEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_CHANNEL))
#define UFO_CHANNEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_CHANNEL, UfoChannelClass))

typedef struct _UfoChannel           UfoChannel;
typedef struct _UfoChannelClass      UfoChannelClass;
typedef struct _UfoChannelPrivate    UfoChannelPrivate;

/**
 * UfoChannel:
 *
 * Data transport channel between two #UfoFilter objects. The contents of the
 * #UfoChannel structure are private and should only be accessed via the
 * provided API.
 */
struct _UfoChannel {
    /*< private >*/
    GObject parent_instance;

    UfoChannelPrivate *priv;
};

/**
 * UfoChannelClass:
 *
 * #UfoChannel class
 */
struct _UfoChannelClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoChannel *ufo_channel_new             (void);
void        ufo_channel_ref             (UfoChannel *channel);
void        ufo_channel_finish          (UfoChannel *channel);
void        ufo_channel_finish_next     (UfoChannel *channel);
void        ufo_channel_insert          (UfoChannel *channel,
                                         UfoBuffer  *buffer);
UfoBuffer * ufo_channel_fetch_input     (UfoChannel *channel);
void        ufo_channel_release_input   (UfoChannel *channel,
                                         UfoBuffer  *buffer);
UfoBuffer * ufo_channel_fetch_output    (UfoChannel *channel);
void        ufo_channel_release_output  (UfoChannel *channel,
                                         UfoBuffer  *buffer);
void        ufo_channel_daisy_chain     (UfoChannel *channel,
                                         UfoChannel *next);
GType       ufo_channel_get_type        (void);

G_END_DECLS

#endif
