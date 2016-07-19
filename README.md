# PPRZLINK

## Overview

PPRZLINK is a messages toolkit (message definition, code generators, libraries) to be used with [Paparazzi UAV System] (https://paparazziuav.org) and compatible systems. One tool that uses PPRZLINK is the [Flying Robot Commander](https://github.com/paparazzi/flyingrobotcommander), a web based, RESTful application for controlling multiple aircraft.

## Documentation

Documentation is in the [doc](doc) subdirectory and can be viewed on https://pprzlink.readthedocs.org

To build the docs locally:

    sudo pip install sphinx sphinx-autobuild phinx_rtd_theme
    cd doc && make html

## Libraries

PPRZLINK libraries are available for the following programming languages: 

- [C](lib/v1.0/C)
- [ocaml](lib/v1.0/ocaml)
- [python](lib/v1.0/python)

## Messages

Messages are defined in the [messages.xml](message_definitions/v1.0/messages.xml) file and are grouped into the following message classes:

- telemetry
- datalink
- ground
- alert
- intermcu

Please reference [Paparazzi Messages Document](http://docs.paparazziuav.org/latest/paparazzi_messages.html) for a more detailed overview. 

### Message Classes

**Telemetry**

Telemetry messages are sent from the aircraft to the ground and are defined in the `telemetry` class of the [messages.xml](message_definitions/v1.0/messages.xml) file.

**Datalink**

Datalink messages are sent from the ground to the aircraft and are defined in the `datalink` class of the [messages.xml](master/message_definitions/v1.0/messages.xml) file.

**Ground**

Ground messages are sent to the ground network agents(GCS, server, link, etc...) and are defined in the `ground` class of the [messages.xml](master/message_definitions/v1.0/messages.xml) file.

**Alert**

TBD

**InterMCU**

TBD

## Python Library

The [python](https://www.python.org/) PPRZLINK ivy interface is defined in [ivy.py](lib/v1.0/python/pprzlink/ivy.py). There is also a serial version of the interface in [serial.py](lib/v1.0/python/pprzlink/serial.py).

### Ivy Message Call Sequence

Add Libraries to the Search Path

    # if PAPARAZZI_SRC not set, then assume the tree containing this file is a reasonable substitute
    PPRZ_SRC = getenv("PAPARAZZI_SRC", path.normpath(path.join(path.dirname(path.abspath(__file__)), '~/paparazzi/')))

    sys.path.append(PPRZ_SRC + "/sw/ext/pprzlink/lib/v1.0/python")

    from pprzlink.ivy  import IvyMessagesInterface
    from pprzlink.message   import PprzMessage
    ...

Create an Interface Instance

    ivy_interface = IvyMessagesInterface("FlyingRobotCommander", start_ivy=False)
    ...

Subscribe to a Set of Messages

    ivy_interface.subscribe(callback_aircraft_messages)
    ...

Start the Interface

    ivy_interface.start()
    ...

Send Messages

    # Main Loop
    ivy_interface.send(msg)
    ivy_interface.send_raw_datalink(msg)
    ...

Stop the Interface

    ivy_interface.shutdown()
    ...

### Ivy Message Construction

It's easy to construct messages to send over the ivy message bus. Here are a few example `python` functions for reference.

**Datalink Message**

    def guidance(ac_id, flag, x, y, z, yaw):
        msg = PprzMessage("datalink", "GUIDED_SETPOINT_NED")
        msg['ac_id'] = ac_id
        msg['flags'] = flag
        msg['x']     = x
        msg['y']     = y
        msg['z']     = z
        msg['yaw']   = yaw

        ivy_interface.send_raw_datalink(msg)



**Ground Message**

    def flightblock(ac_id, fb_id):
        msg = PprzMessage("ground", "JUMP_TO_BLOCK")
        msg['ac_id']    = ac_id
        msg['block_id'] = fb_id

        ivy_interface.send(msg)

### Subscribing to Ivy Messages

TBD - Describe how to subscribe to message classes 


