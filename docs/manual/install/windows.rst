.. _installation-windows:

#################################
Installation on Windows with WSL2
#################################

With Windows 11 and WSL2 it is possible to use Intel OpenCL GPU support. Please
follow `Intel's documentation`_ for detailed installation procedure. Long story
short, you need to install WSL2 and Ubuntu 20.04 from Microsoft Store, then the
Intel `driver`_ *inside Windows* and then `OpenCL runtime`_ *inside Ubuntu 20.04
running in the WSL*. After that, follow the normal installation procedure
(without the nvidia packages) described in :ref:`ubuntu20.04`.


.. _Intel's documentation: https://www.intel.com/content/www/us/en/artificial-intelligence/harness-the-power-of-intel-igpu-on-your-machine.html
.. _driver: https://www.intel.com/content/www/us/en/download/19344/30579/intel-graphics-windows-dch-drivers.html?
.. _OpenCL runtime: https://github.com/intel/compute-runtime/releases/tag/21.35.20826
