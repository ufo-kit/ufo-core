#ifndef __UFO_PROFILER_H
#define __UFO_PROFILER_H

#include <glib-object.h>
#include "ufo-resource-manager.h"

G_BEGIN_DECLS

#define UFO_TYPE_PROFILER             (ufo_profiler_get_type())
#define UFO_PROFILER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_PROFILER, UfoProfiler))
#define UFO_IS_PROFILER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_PROFILER))
#define UFO_PROFILER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_PROFILER, UfoProfilerClass))
#define UFO_IS_PROFILER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_PROFILER))
#define UFO_PROFILER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_PROFILER, UfoProfilerClass))

typedef struct _UfoProfiler           UfoProfiler;
typedef struct _UfoProfilerClass      UfoProfilerClass;
typedef struct _UfoProfilerPrivate    UfoProfilerPrivate;

/**
 * UfoProfiler:
 *
 * The #UfoProfiler collects and records OpenCL events and stores them in a
 * convenient format on disk or prints summaries on screen.
 */
struct _UfoProfiler {
    /*< private >*/
    GObject parent_instance;

    UfoProfilerPrivate *priv;
};

typedef void (*UfoProfilerFunc) (const gchar *row, gpointer user_data);

/**
 * UfoProfilerClass:
 *
 * #UfoProfiler class
 */
struct _UfoProfilerClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoProfiler *ufo_profiler_new       (void);
void         ufo_profiler_call      (UfoProfiler    *profiler,
                                     gpointer        command_queue,
                                     gpointer        kernel,
                                     guint           work_dim,
                                     const gsize    *global_work_size,
                                     const gsize    *local_work_size);
void         ufo_profiler_foreach   (UfoProfiler    *profiler,
                                     UfoProfilerFunc func,
                                     gpointer        user_data);
GType        ufo_profiler_get_type  (void);

G_END_DECLS

#endif
