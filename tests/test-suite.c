#include <glib-object.h>
#include "test-suite.h"

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    test_add_buffer ();
    test_add_filter ();
    test_add_relation ();

    g_test_run();

    return 0;
}
