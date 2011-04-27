#include <glib.h>
#include "ufo-split.h"

static void test_split_new(void)
{
    UfoSplit *split = ufo_split_new();
    g_assert(split != NULL);
    g_object_unref(split);
}

static void test_split_add_empty(void)
{
    UfoSplit *split = ufo_split_new();
    ufo_container_add_element(UFO_CONTAINER(split), NULL);
    g_object_unref(split);
}

/**
 * Tests that input queue of split is different from its added filters and both
 * filters' output queues are identical to that of the split.
 */
static void test_split_queues(void)
{
    UfoSplit *split = ufo_split_new();
    GAsyncQueue *input_queue = g_async_queue_new();
    ufo_element_set_input_queue(UFO_ELEMENT(split), input_queue);

    UfoElement *filter1 = g_object_new(UFO_TYPE_FILTER, NULL);
    UfoElement *filter2 = g_object_new(UFO_TYPE_FILTER, NULL);

    ufo_container_add_element(UFO_CONTAINER(split), filter1);
    ufo_container_add_element(UFO_CONTAINER(split), filter2);

    g_assert(input_queue != ufo_element_get_input_queue(filter1));
    g_assert(input_queue != ufo_element_get_input_queue(filter2));
    g_assert(ufo_element_get_output_queue(UFO_ELEMENT(split)) == ufo_element_get_output_queue(filter1));
    g_assert(ufo_element_get_output_queue(UFO_ELEMENT(split)) == ufo_element_get_output_queue(filter2));

    g_async_queue_unref(input_queue);
    g_object_unref(filter1);
    g_object_unref(filter2);
    g_object_unref(split);
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_test_add_func("/split/new", test_split_new);
    g_test_add_func("/split/add-empty", test_split_add_empty);
    g_test_add_func("/split/queues", test_split_queues);
    g_test_run();

    return 0;
}
