==========================
What's New in ufo-core 0.3
==========================

Major breakage
==============

- A graph is now a simple data structure and not a specific graph of task nodes.
  This is implemented by a TaskGraph.

- Filters are now called TaskNodes and connected with
  `ufo_task_graph_connect_nodes` and `ufo_task_graph_connect_nodes_full`
  respectively.


Graph expansion
===============

With 0.2, Using multiple GPUs was possible by manually splitting paths in the
graph and assigning GPUs. Now, task graphs are automatically expanded depending
on the number of available GPUs and remote processing slaves.


Minor improvements
==================

- A `deploy.sh` script has been added for easier deployment of the software
  stack. This is especially useful to install everything in the home directory
  of the user, who only needs to setup `LD_LIBRARY_PATH` and `GI_TYPELIB_PATH`
  correctly to run the software.
