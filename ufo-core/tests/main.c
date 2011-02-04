
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "ufo.h"

START_TEST (ufo_create)
{
    ufo *u = ufo_init();
    fail_if(u == NULL);
}
END_TEST


int main(int argc, char *argv[])
{
    printf("\n=== Using Check for Unit Tests =======================\n");
    /* Create test suite */
    Suite *s = suite_create("UFO");

    /* Add test cases */
    TCase *tc_devices = tcase_create("Devices");
    tcase_add_test(tc_devices, ufo_create);

    suite_add_tcase(s, tc_devices);

    /* Run tests */
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int fails = srunner_ntests_failed(sr);
    srunner_free(sr);
    printf("\n=== Finished Check ===================================\n");

    return (fails == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
