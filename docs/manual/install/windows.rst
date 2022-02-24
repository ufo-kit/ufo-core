.. _installation-windows:

#################################
Installation on Windows with WSL2
#################################

With Windows 11 and WSL2 it is easy to get a Linux distribution with Intel
OpenCL support. First, install WSL2 and Ubuntu 20.04 from Microsoft store and
then follow `Intel`_ instructions. Long story short, you need to install the
Intel `driver`_ inside Windows and then `OpenCL runtime`_ inside Ubuntu 20.04
running in the WSL.


.. _Intel: https://www.intel.com/content/www/us/en/artificial-intelligence/harness-the-power-of-intel-igpu-on-your-machine.html
.. _driver: https://www.intel.com/content/www/us/en/download/19344/30579/intel-graphics-windows-dch-drivers.html?
.. _OpenCL runtime: https://github.com/intel/compute-runtime/releases/tag/21.35.20826
