#ifndef __UFO_FILTER_IFFT_H
#define __UFO_FILTER_IFFT_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_IFFT             (ufo_filter_ifft_get_type())
#define UFO_FILTER_IFFT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_IFFT, UfoFilterIFFT))
#define UFO_IS_FILTER_IFFT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_IFFT))
#define UFO_FILTER_IFFT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_IFFT, UfoFilterIFFTClass))
#define UFO_IS_FILTER_IFFT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_IFFT))
#define UFO_FILTER_IFFT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_IFFT, UfoFilterIFFTClass))

typedef struct _UfoFilterIFFT           UfoFilterIFFT;
typedef struct _UfoFilterIFFTClass      UfoFilterIFFTClass;
typedef struct _UfoFilterIFFTPrivate    UfoFilterIFFTPrivate;

struct _UfoFilterIFFT {
    UfoFilter parent_instance;

    /* private */
    UfoFilterIFFTPrivate *priv;
};

struct _UfoFilterIFFTClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_ifft_get_type(void);

#endif
