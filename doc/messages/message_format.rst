===============
Message Format
===============

.. _`messages.xml`: https://github.com/paparazzi/pprzlink/blob/master/message_definitions/v1.0/messages.xml

PPRZ format
-----------

The Pprzlink v2.0 standard message is defined as such:

.. code-block::

    PPRZ-message: ABCxxxxxxxDE
        A PPRZ_STX (0x99)
        B LENGTH (A->E)
        C PPRZ_DATA
          0 SOURCE (~sender_ID)
          1 DESTINATION (can be a broadcast ID)
          2 CLASS/COMPONENT
            bits 0-3: 16 class ID
            bits 4-7: 16 component ID
          3 MSG_ID
          4 MSG_PAYLOAD
          . DATA (messages.xml)
        D PPRZ_CHECKSUM_A (sum[B->C])
        E PPRZ_CHECKSUM_B (sum[ck_a])

+ **PPRZ_STX** (1 bytes) start byte, defined as ``0x99``.
+ **LENGTH** (1 bytes) full length of the message, from *PPRZ_STX* to *PPRZ_CHECKSUM_B*.
+ **SOURCE** (1 bytes) ID of the sender.
+ **DESTINATION**: (1 bytes) ID of the receiver.
+ **CLASS**: (half a byte) The message class id, coded on the 4 least significant bits. See `messages.xml`_.
+ **COMPONENT**: (half a byte) The component id, coded on the 4 most significant bits. This is not yet broadly supported in paparazzi. Default value is 0.
+ **MSG_ID**: (1 bytes) The message id. See `messages.xml`_.
+ **MSG_PAYLOAD**: (LENGTH-8 bytes) message payload. See `messages.xml`_.
+ **PPRZ_CHECKSUM_***: (2 bytes) message checksum.


aircraft ID
___________

+ ID ``0x00`` is reserved for the ground.
+ ID ``0xFF`` is the broadcast ID.


Checksum
________
    
**PPRZ_CHECKSUM_A** is computed as the 1 byte wrapping sum of all bytes
from *LENGTH* to *MSG_PAYLOAD*.

**PPRZ_CHECKSUM_B** is computed as the 1 byte wrapping sum of all values
of *PPRZ_CHECKSUM_A*.

In this example code, ``data`` are the bytes from *LENGTH* to *MSG_PAYLOAD*.

.. code-block::

    def ck(data):
        cka=0
        ckb=0
        for b in data:
            cka += b
            ckb += cka
        return cka,ckb

Xbee API format
---------------

Pprz data are the same as in the PPRZ format, just the encapsulation differs.

.. code-block::

    XBee-message: ABCDxxxxxxxE
        A XBEE_START (0x7E)
        B LENGTH_MSB (D->D)
        C LENGTH_LSB
        D XBEE_PAYLOAD
          0 XBEE_TX16 (0x01) / XBEE_RX16 (0x81)
          1 FRAME_ID (0)     / SRC_ID_MSB
          2 DEST_ID_MSB      / SRC_ID_LSB
          3 DEST_ID_LSB      / XBEE_RSSI
          4 TX16_OPTIONS (0) / RX16_OPTIONS
          5 PPRZ_DATA
            0 SOURCE (~sender_ID)
            1 DESTINATION (can be a broadcast ID)
            2 CLASS/COMPONENT
                bits 0-3: 16 class ID
                bits 4-7: 16 component ID
            3 MSG_ID
            4 MSG_PAYLOAD
            . DATA (messages.xml)
        E XBEE_CHECKSUM (sum[D->D])

       ID is AC_ID for aircraft, 0x100 for ground station



XBee destination ID is 2 bytes long. Use the paparazzi AC_ID for the LSB and 0x00 for the MSB, with 2 exceptions:

+ The ground address is ``0x0100``
+ The broadcast address is ``0xFFFF``.



Example
_______

Lets take this "PPRZ" encoded message:

``99 0C 07 00 01 02 03 00 01 02 1C C4``

This message can be decomposed as :

+----+-----------+----------+---------------+----------------------+
|STX | LENGHT=12 | SOURCE=7 | DESTINATION=0 | CLASS=1, COMPONENT=0 |
+====+===========+==========+===============+======================+
| 99 |     0C    |     07   |     00        |       01             |
+----+-----------+----------+---------------+----------------------+

+----------+---------------+-----+----+
| MSG_ID=2 |  MSG_PAYLOAD  | CKA | CKB|
+==========+===============+=====+====+
|    02    |   03 00 01 02 | 1C  | C4 |
+----------+---------------+-----+----+




