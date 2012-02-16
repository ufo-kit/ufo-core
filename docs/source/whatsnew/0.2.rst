==========================
What's New in ufo-core 0.2
==========================

:Author: Matthias Vogelgesang

Major breakage
==============

Filters are now prefixed again with ``libfilter`` to allow introspected
documentation. Thus, any filter built for 0.1 cannot be used because they are
simply not found.


General improvements
====================

Instead of the ``json/`` directory a new ``scripts/`` directory has been added
to give an idea how to use the UFO framework for computing the tomographic
reconstruction.
