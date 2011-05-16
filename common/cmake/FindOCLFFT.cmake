# Try to find liboclfft and clFFT.h. Once found the following variables will be
# defined:
#
# OCLFFT_FOUND
# OCLFFT_INCLUDE_DIRS
# OCLFFT_LIBRARIES

find_path(OCLFFT_INCLUDE_DIRS clFFT.h)
find_library(OCLFFT_LIBRARIES oclfft)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OCLFFT DEFAULT_MSG OCLFFT_INCLUDE_DIRS OCLFFT_LIBRARIES)

mark_as_advanced(OCLFFT_INCLUDE_DIRS OCLFFT_LIBRARIES)
