#include <glib.h>
#include "ufo-sequence.h"

static void test_sequence_new(void)
{
    UfoSequence *sequence = ufo_sequence_new();
    g_assert(sequence != NULL);
    g_object_unref(sequence);
}

static void test_sequence_add_empty(void)
{
    UfoSequence *sequence = ufo_sequence_new();
    ufo_container_add_element(UFO_CONTAINER(sequence), NULL);
    g_object_unref(sequence);
}

/**
 * Tests that input queue of sequence is identical to first filter and output
 * queue is identical to second filter. Furthermore, test that output of filter
 * one is identical to input of filter 2. */
static void test_sequence_queues(void)
{
    UfoSequence *sequence = ufo_sequence_new();
    GAsyncQueue *input_queue = g_async_queue_new();
    ufo_element_set_input_queue(UFO_ELEMENT(sequence), input_queue);

    UfoElement *filter1 = g_object_new(UFO_TYPE_FILTER, NULL);
    UfoElement *filter2 = g_object_new(UFO_TYPE_FILTER, NULL);

    ufo_container_add_element(UFO_CONTAINER(sequence), filter1);
    ufo_container_add_element(UFO_CONTAINER(sequence), filter2);

    g_assert(input_queue == ufo_element_get_input_queue(filter1));
    g_assert(ufo_element_get_output_queue(UFO_ELEMENT(sequence)) != ufo_element_get_input_queue(filter2));
    g_assert(ufo_element_get_output_queue(filter1) == ufo_element_get_input_queue(filter2));

    g_async_queue_unref(input_queue);
    g_object_unref(filter1);
    g_object_unref(filter2);
    g_object_unref(sequence);
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_test_add_func("/sequence/new", test_sequence_new);
    g_test_add_func("/sequence/add-empty", test_sequence_add_empty);
    g_test_add_func("/sequence/queues", test_sequence_queues);

    g_test_run();
    return 0;
}
