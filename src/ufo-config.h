#ifndef __UFO_CONFIG_H
#define __UFO_CONFIG_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_CONFIG             (ufo_config_get_type())
#define UFO_CONFIG(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_CONFIG, UfoConfig))
#define UFO_IS_CONFIG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_CONFIG))
#define UFO_CONFIG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_CONFIG, UfoConfigClass))
#define UFO_IS_CONFIG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_CONFIG))
#define UFO_CONFIG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_CONFIG, UfoConfigClass))

typedef struct _UfoConfig           UfoConfig;
typedef struct _UfoConfigClass      UfoConfigClass;
typedef struct _UfoConfigPrivate    UfoConfigPrivate;

/**
 * UfoConfig:
 *
 * A #UfoConfig provides access to run-time specific settings.
 */
struct _UfoConfig {
    /*< private >*/
    GObject parent_instance;

    UfoConfigPrivate *priv;
};

/**
 * UfoConfigClass:
 *
 * #UfoConfig class
 */
struct _UfoConfigClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoConfig   *  ufo_config_new         (void);
gchar      **  ufo_config_get_paths   (UfoConfig *config);
GType          ufo_config_get_type    (void);

G_END_DECLS

#endif
