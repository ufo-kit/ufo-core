#ifndef __UFO_SEQUENCE_H
#define __UFO_SEQUENCE_H

#include <glib-object.h>
#include "ufo-container.h"

G_BEGIN_DECLS

#define UFO_TYPE_SEQUENCE             (ufo_sequence_get_type())
#define UFO_SEQUENCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_SEQUENCE, UfoSequence))
#define UFO_IS_SEQUENCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_BUFFER))
#define UFO_SEQUENCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_SEQUENCE, UfoSequenceClass))
#define UFO_IS_SEQUENCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_BUFFER))
#define UFO_SEQUENCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_SEQUENCE, UfoSequenceClass))

typedef struct _UfoSequence           UfoSequence;
typedef struct _UfoSequenceClass      UfoSequenceClass;
typedef struct _UfoSequencePrivate    UfoSequencePrivate;

struct _UfoSequence {
    UfoContainer parent;

    /* public */

    /* private */
    UfoSequencePrivate *priv;
};

struct _UfoSequenceClass {
    UfoContainerClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* virtual public methods */

/* non-virtual public methods */
UfoSequence *ufo_sequence_new();

GType ufo_sequence_get_type(void);

G_END_DECLS

#endif
