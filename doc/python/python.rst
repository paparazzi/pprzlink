==============
Python Library
==============

General description of python lib


Available interfaces
--------------------

The following interfaces are available :

- :doc:`Serial <serial>`
- :doc:`Ivy <ivy>`
- :doc:`UDP <udp>`

Supported protocols
-------------------

- pprz binary format
- ascii (ivy)

The XBee binary protocol is not supported at the moment.

Running the test programms
--------------------------

The PYTHONPATH environnement variable needs to be set to add the current folder:
On bask-like shells: `export PYTHONPATH=<path/to/this/directory>:$PYTHONPATH`

Then you can run test programs with `python -m pprzlink.serial` in the case of the serial interface.

.. toctree::
   :maxdepth: 2
   :glob:
	
   messages
   transport
   ivy
   serial
   udp

