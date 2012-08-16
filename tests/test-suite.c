#include <glib-object.h>
#include "test-suite.h"

static void ignore_log (const gchar     *domain,
                        GLogLevelFlags   flags,
                        const gchar     *message,
                        gpointer         data)
{
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_log_set_handler ("Ufo", G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG, ignore_log, NULL);
    g_log_set_handler ("ocl", G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG, ignore_log, NULL);

    test_add_buffer ();
    test_add_channel ();
    test_add_filter ();
    test_add_graph ();
    test_add_relation ();

    g_test_run();

    return 0;
}
