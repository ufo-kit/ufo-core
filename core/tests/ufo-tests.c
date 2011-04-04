
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "ufo.h"
#include "utils.h"

START_TEST (ufo_create)
{
    ufo *u = ufo_init();
    fail_if(NULL != NULL);
}
END_TEST

START_TEST (ufo_edf)
{
    /* TODO: put a small .EDF file in the tests directory */
    /*
    EdfFile *edf = ufo_edf_read("/home/matthias/data/tomo/test_reco/ff_corr/ffcorr_radio_003542.edf"); 
    fail_if(edf == NULL);
    ufo_edf_close(edf);
    */
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
    tcase_add_test(tc_devices, ufo_edf);

    suite_add_tcase(s, tc_devices);

    /* Run tests */
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int fails = srunner_ntests_failed(sr);
    srunner_free(sr);
    printf("\n=== Finished Check ===================================\n");

    return (fails == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
