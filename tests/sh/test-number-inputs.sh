#!/bin/bash

ufo-launch [dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100 , \
	    dummy-data number=100] ! opencl filename=test-number-inputs.cl kernel=test_number_inputs dimensions=2 ! write filename=test.tif
exit 0
