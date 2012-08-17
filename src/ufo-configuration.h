#ifndef __UFO_CONFIGURATION_H
#define __UFO_CONFIGURATION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_CONFIGURATION             (ufo_configuration_get_type())
#define UFO_CONFIGURATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_CONFIGURATION, UfoConfiguration))
#define UFO_IS_CONFIGURATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_CONFIGURATION))
#define UFO_CONFIGURATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_CONFIGURATION, UfoConfigurationClass))
#define UFO_IS_CONFIGURATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_CONFIGURATION))
#define UFO_CONFIGURATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_CONFIGURATION, UfoConfigurationClass))

typedef struct _UfoConfiguration           UfoConfiguration;
typedef struct _UfoConfigurationClass      UfoConfigurationClass;
typedef struct _UfoConfigurationPrivate    UfoConfigurationPrivate;

/**
 * UfoConfiguration:
 *
 * The #UfoConfiguration collects and records OpenCL events and stores them in a
 * convenient format on disk or prints summaries on screen.
 */
struct _UfoConfiguration {
    /*< private >*/
    GObject parent_instance;

    UfoConfigurationPrivate *priv;
};

/**
 * UfoConfigurationClass:
 *
 * #UfoConfiguration class
 */
struct _UfoConfigurationClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoConfiguration *  ufo_configuration_new         (void);
gchar **            ufo_configuration_get_paths   (UfoConfiguration *config);
GType               ufo_configuration_get_type    (void);

G_END_DECLS

#endif
