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

Source and generated API documentation
--------------------------------------

The implementation is in ``lib/v2.0/C++``. The ``pprzlink_docs`` CMake target
generates the Doxygen API reference from header and source comments.
