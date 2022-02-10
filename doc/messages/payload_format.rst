==============
Payload format
==============

.. _`messages.xml`: https://github.com/paparazzi/pprzlink/blob/master/message_definitions/v1.0/messages.xml

The message payload is defined in the `messages.xml`_.

Fields base types are : 

+ int8
+ int16
+ int32
+ uint8
+ uint16
+ uint32
+ float
+ double
+ char

.. note:: values should be encoded as little endian.


Field type can be:

+ a base type: ``int32``
+ an variable size array : ``int32[]``
+ a fixed size array : ``int32[3]``


A variable length array is encoded as its lenght (on 1 byte), then all its values (from low to high indices).



Example
_______

Lets take this payload:

``03 00 01 02``

For this message definition :

.. code-block::

    <message name="ALIVE" id="2">
        <field name="md5sum" type="uint8[]"/>
    </message>

This message is defined as a single field, a variable length array.

We can then decode its content as the uint8 array [0, 1, 2].


