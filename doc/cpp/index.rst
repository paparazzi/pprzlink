C++ library and link++
======================

The C++23 library loads message definitions at runtime and exchanges PPRZLINK
messages over serial ports, UDP, Ivy and XBee radios. Start with the usage guide
for a complete workshop, or use the reference for build and API details.
The usage guide and architecture document are written in French.

.. toctree::
   :maxdepth: 1

   guide_d_utilisation
   reference
   API_USAGE
   architecture
   validation/index
   historique/index

Source and generated API documentation
--------------------------------------

The implementation remains in ``lib/v2.0/C++``. Header and source comments
are used by the ``pprzlink_docs`` CMake target to generate the Doxygen API
reference. Build commands in these manuals keep their stated working directory;
moving the manuals does not move the sources or examples.
