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
 * \class UfoChannel
 * \brief Abstract data container
 *
 * Abstract data container that holds either valid GPU data or CPU data.
 *
 * <b>Signals</b>
 *
 * <b>Properties</b>
 *
 *  - <tt>"finished" : gboolean</tt> — If TRUE, buffer does not contain any
 *      useful data but specifies that no more data is following.
 */
struct _UfoChannel {
    GObject parent_instance;

    /* private */
    UfoChannelPrivate *priv;
};

struct _UfoChannelClass {
    GObjectClass parent_class;
};

UfoChannel *ufo_channel_new(void);
void ufo_channel_ref(UfoChannel *channel);

void ufo_channel_finish(UfoChannel *channel);
UfoBuffer *ufo_channel_pop(UfoChannel *channel);
void ufo_channel_push(UfoChannel *channel, UfoBuffer *buffer);
gint ufo_channel_length(UfoChannel *channel);


GType ufo_channel_get_type(void);

G_END_DECLS

#endif
