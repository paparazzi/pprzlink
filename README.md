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
- [Rust](https://github.com/paparazzi/pprzlink-rust)

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

## C standalone Library

The C standalone library can be used when only a few messages using the PPRZ transport are expected (in comparison with the full C library where all messages are generated and both device and transport can be selected independently).
This is usefull when implementing a program in C in an embedded computer talking to the autopilot through a serial port.
It is a header only library, so there is no file to compile.

### Generation of a C standalone messages parse

Assuming we only need the GPS and ATTITUDE messages, either sending or receiving, the generation of the library is as follow:

    ./tools/generator/gen_messages.py --protocol 2.0 --lang C_standalone -o build/pprzlink/my_pprz_messages.h message_definitions/v1.0/messages.xml telemetry --opt ATTITUDE,GPS

The generated files will be placed in the folder `build/pprzlink` in this case. The folder `pprzlink` and the files it contains should be copied to your project directory (or any library folder part of your include paths).

### Usage for sending

Sending a message is done as follow:

Include the library header

    #include "pprzlink/my_pprz_messages.h"

Implement some required callbacks (definitions can be found in `pprzlink/pprzlink_standalone.h`): `check_space`, `put_char`, `send_message` (or NULL if not needed)

    int check_space(uint8_t n) {
      // implement your check space function here
    }

    void put_char(uint8_t c) {
      // implement your put char function here
    }

    void send_message(void) {
      // implement your send message function here
    }

Init the TX structure (definitions can be found in `pprzlink/pprzlink_standalone.h`):

    // somewhere in your init section
    struct pprzlink_device_tx dev_tx = pprzlink_device_tx_init(check_space, put_char, send_message /* or NULL */);

Send messages (replace `...` with paramters, see definition in `pprzlink/my_pprz_messages.h`):

    // in the case of GPS message
    pprzlink_msg_send_GPS(&dev_tx, ...);

### Usage for receiving

Include the library header

    #include "pprzlink/my_pprz_messages.h"

Implement some required callbacks (definitions can be found in `pprzlink/pprzlink_standalone.h`): `char_available`, `get_char`, `new_message` as well as a buffer large enough to receive your messages (max 255 bytes):

    int char_available(void) {
      // implement your char available function here
    }

    uint8_t get_char(void) {
      // implement your get char function here
    }

    uint8_t rx_buffer[255];

    void new_message(uint8_t sender_id, uint8_t receiver_id, uint8_t class_id, uint8_t message_id, uint8_t *buf, void *user_data) {
      // check message/class IDs to before extracting data from the messages
      if (message_id == PPRZ_MSG_ID_GPS) {
        // get data from GPS
        int32_t east = pprzlink_get_GPS_utm_east(buf);
        int32_t north = pprzlink_get_GPS_utm_north(buf);
      }
      else if (message_id == PPRZ_MSG_ID_ATTITUDE) {
        // get data from ATTITUDE
      }
    }


Init the RX structure (definitions can be found in `pprzlink/pprzlink_standalone.h`):

    // somewhere in your init section
    struct pprzlink_device_rx dev_rx = pprzlink_device_rx_init(char_available, get_char, rx_buffer, (void *)&user_data);

Where `user_data` is a pointer to a structure that you may want to pass at init and use in the `new_message` callback. If no user data are needed, just pass `NULL` as argument value.
Parse messages by calling this function in your mainloop:

    pprzlink_check_and_parse(&dev_rx, new_message);

