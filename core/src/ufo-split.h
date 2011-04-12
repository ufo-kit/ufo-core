#ifndef __UFO_SPLIT_H
#define __UFO_SPLIT_H

#include <glib-object.h>
#include "ufo-container.h"

G_BEGIN_DECLS

#define UFO_TYPE_SPLIT             (ufo_split_get_type())
#define UFO_SPLIT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_SPLIT, UfoSplit))
#define UFO_IS_SPLIT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_BUFFER))
#define UFO_SPLIT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_SPLIT, UfoSplitClass))
#define UFO_IS_SPLIT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_BUFFER))
#define UFO_SPLIT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_SPLIT, UfoSplitClass))

typedef struct _UfoSplit           UfoSplit;
typedef struct _UfoSplitClass      UfoSplitClass;
typedef struct _UfoSplitPrivate    UfoSplitPrivate;

struct _UfoSplit {
    UfoContainer parent;

    /* public */

    /* private */
    UfoSplitPrivate *priv;
};

struct _UfoSplitClass {
    UfoContainerClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* virtual public methods */

/* non-virtual public methods */
UfoSplit *ufo_split_new();

GType ufo_split_get_type(void);

G_END_DECLS

#endif
