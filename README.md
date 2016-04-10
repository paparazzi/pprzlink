# PPRZLINK

## Overview

PPRZLINK is a messages toolkit (message definition, code generators, libraries) to be used with [Paparazzi UAV System] (https://paparazziuav.org) and compatible systems. One tool that uses PPRZLINK is the [Flying Robot Commander](https://github.com/paparazzi/flyingrobotcommander), a web based, RESTful application for controlling multiple aircraft.

## Libraries

PPRZLINK libraries are available for the following programming languages: 

- [C](https://github.com/hooperfly/pprzlink/tree/master/lib/v1.0/C)
- [ocaml](https://github.com/hooperfly/pprzlink/tree/master/lib/v1.0/ocaml)
- [python](https://github.com/hooperfly/pprzlink/tree/master/lib/v1.0/python)

### Python Library Call Sequence

Add Libraries to the Search Path

    # if PAPARAZZI_SRC not set, then assume the tree containing this file is a reasonable substitute
    PPRZ_SRC = getenv("PAPARAZZI_SRC", path.normpath(path.join(path.dirname(path.abspath(__file__)), '~/paparazzi/')))

    sys.path.append(PPRZ_SRC + "/sw/lib/python")
    sys.path.append(PPRZ_SRC + "/sw/ext/pprzlink/lib/v1.0/python")

    from ivy_msg_interface  import IvyMessagesInterface
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

    
## Message Definitions

TBD

