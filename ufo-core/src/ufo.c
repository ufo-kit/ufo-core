
#include <stdlib.h>

#include "config.h"
#include "ufo.h"

#ifdef HAVE_OCLFFT
#include "clFFT.h"
#endif


ufo *ufo_init(void)
{
    int err;
#ifdef HAVE_OCLFFT
    clFFT_Dim3 dim;
    clFFT_Plan *plan = clFFT_CreatePlan(0, dim, clFFT_1D, clFFT_SplitComplexFormat, &err);
#endif
    return NULL;
}

void ufo_destroy(ufo *context)
{
}
