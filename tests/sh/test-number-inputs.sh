#!/bin/bash

ufo-launch [dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256 , \
	    dummy-data number=100 width=256 height=256] ! opencl filename=test-number-inputs.cl kernel=test_number_inputs dimensions=2 ! write filename=test.tif
exit 0
