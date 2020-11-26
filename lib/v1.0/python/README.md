PPRZLINK Python library
=======================

Available interfaces
--------------------

- serial
- ivy
- udp

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

License
-------

PPRZLINK Python library is released under LGPLv3 (or later)

