#ifndef __UFO_PLUGIN_H
#define __UFO_PLUGIN_H

#include <glib.h>
#include <glib-object.h>
#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_PLUGIN             (ufo_plugin_get_type())
#define UFO_PLUGIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_PLUGIN, UfoPlugin))
#define UFO_IS_PLUGIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_PLUGIN))
#define UFO_PLUGIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_PLUGIN, UfoPluginClass))
#define UFO_IS_PLUGIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_PLUGIN))
#define UFO_PLUGIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_PLUGIN, UfoPluginClass))

typedef struct _UfoPlugin           UfoPlugin;
typedef struct _UfoPluginClass      UfoPluginClass;
typedef struct _UfoPluginPrivate    UfoPluginPrivate;

/* 
 * Plugin function prototypes. Compliant plugins must export all of them with
 * these names except for the filter calls.
 */

/*
 * \brief Initialize structures in the plugin.
 */
typedef void (*plugin_init) (void);

/*
 * \brief Destroy structures in the plugin.
 */
typedef void (*plugin_destroy) (void);

/*
 * \brief Get all filter names that are going to be called
 * \return Array of filter names
 */
typedef gchar **(*plugin_get_filter_names) (void);

/*
 * \brief Retrieve a brief description of a filter
 * \parameter[in] filter_name Name of the filter
 * \return Description string
 */
typedef gchar *(*plugin_get_filter_description) (const gchar *filter_name);

/*
 * \brief Call to the plugin filter
 * \parameter[in] data Buffer with image data
 * \parameter[in] width Image width
 * \parameter[in] height Image height
 * \parameter[in] bpp Bytes per pixel
 */
typedef void (*plugin_filter_call) (gchar *data, gint32 width, gint32 height, gint32 bpp);


struct _UfoPlugin {
    GObject parent_instance;

    /* public */
    /* private */
    UfoPluginPrivate *priv;
};

struct _UfoPluginClass {
    GObjectClass parent_class;

    /* class members */
    /* virtual public methods */
};

/* non-virtual public methods */
UfoPlugin *ufo_plugin_new(const gchar *file_name);
UfoFilter *ufo_get_filter(UfoPlugin *plugin, const gchar *name);

GType ufo_plugin_get_type(void);

G_END_DECLS

#endif
