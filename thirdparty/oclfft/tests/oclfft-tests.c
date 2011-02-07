
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <check.h>
#include <CL/cl.h>
#include "clFFT.h"


START_TEST (test_simple_fft)
{
    cl_platform_id platform;
    cl_int err = clGetPlatformIDs(1, &platform, NULL);
    fail_if(err != CL_SUCCESS);

    cl_device_id device;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    fail_if(err != CL_SUCCESS);

    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    fail_if(err != CL_SUCCESS);

    cl_command_queue queue = clCreateCommandQueue(context, device, 0, &err);
    fail_if(err != CL_SUCCESS);

    clFFT_Dim3 dim;
    dim.x = 8; dim.y = dim.z = 1;

    clFFT_Plan fft_plan = clFFT_CreatePlan(context, dim, clFFT_1D, clFFT_InterleavedComplexFormat, &err);
    fail_if(err != CL_SUCCESS);

    float buffer[16], reference[16];
    float value = 1.0;
    for (int i = 0; i < 16; i += 2) {
        buffer[i] = reference[i] = value;
        buffer[i+1] = reference[i+1] = 0.0;
        value += 0.5;
    }

    cl_mem d_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, 16*sizeof(float), NULL, &err);
    fail_if(err != CL_SUCCESS);

    err = clEnqueueWriteBuffer(queue, d_buffer, CL_TRUE, 0, 16*sizeof(float), buffer, 0, NULL, NULL);
    fail_if(err != CL_SUCCESS);

    err = clFFT_ExecuteInterleaved(queue, fft_plan, 8, clFFT_Forward, d_buffer, d_buffer, 0, NULL, NULL);
    fail_if(err != CL_SUCCESS);

    err = clFFT_ExecuteInterleaved(queue, fft_plan, 8, clFFT_Inverse, d_buffer, d_buffer, 0, NULL, NULL);
    fail_if(err != CL_SUCCESS);

    err = clEnqueueReadBuffer(queue, d_buffer, CL_TRUE, 0, 16*sizeof(float), buffer, 0, NULL, NULL);
    fail_if(err != CL_SUCCESS);

    for (int i = 0; i < 16; i += 2) {
        fail_if((abs(buffer[i]/8.0) - abs(reference[i])) > 0.0001);
    }

    clReleaseMemObject(d_buffer);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
}
END_TEST


int main(int argc, char *argv[])
{
    printf("\n=== Using Check for Unit Tests =======================\n");
    /* Create test suite */
    Suite *s = suite_create("OpenCL FFT");

    /* Add test cases */
    TCase *tc = tcase_create("OpenCL FFT");
    tcase_add_test(tc, test_simple_fft);

    suite_add_tcase(s, tc);

    /* Run tests */
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int fails = srunner_ntests_failed(sr);
    srunner_free(sr);
    printf("\n=== Finished Check ===================================\n");

    return (fails == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
