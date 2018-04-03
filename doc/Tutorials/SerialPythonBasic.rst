.. _SerialTutorial:

==========================================================
Send and receive messages from a serial device with python
==========================================================

This tutorial explains how to send and receive messages to and from a serial device.

We will make a program that periodically sends *PING* messages to a serial device, while monitoring the device to print every paparazzi message received on it. When it receives a *PING* it will send back a *PONG*. This is ment to be tested with a loopback serial device (a FTDI cable with RX and TX linked).

The tutorial code will be written in the :class:`SerialTutorial` class so as to keep state information.

Creating the :class:`pprzlink.serial.SerialMessagesInterface`
=============================================================

First of all we need to create a :class:`pprzlink.serial.SerialMessagesInterface` object to handle sending and receiving data.

.. role:: python(code)
   :language: python

.. literalinclude:: serial_tutorial.py
    :language: python
    :lines: 23-28

Here we create an interface that will bind to a serial device (:code:`args.dev`) at the specified baud rate (:code:`args.baud`). The id of the local system is set to :code:`args.id` and messages arriving on the serial device for this id will be passed along with the source id to the :func:`self.process_incoming_message` callback function (see :ref:`serial_tutorial_receiving_msg`).

Note that the construction of :class:`pprzlink.serial.SerialMessagesInterface` can take additional parameters that we will ignore in this tutorial.

.. _serial_tutorial_receiving_msg:

Receiving messages
==================

When a message arrives on the serial device specified in the creation of the interface, the id of the destination of the message is checked. If the :code:`interface_id` specified in the interface creation is the same as the destination id of the message, then the callback function will be called. 

.. role:: python(code)
   :language: python

.. literalinclude:: serial_tutorial.py
    :language: python
    :lines: 57-59

Here the callback function just prints the message and the id of the source.

Sending messages
==================

To send a message, we just need to call the :func:`pprzlink.serial.SerialMessagesInterface.send` function. We send the message from our id to ourselves.

.. role:: python(code)
   :language: python

.. literalinclude:: serial_tutorial.py
    :language: python
    :lines: 46-51


Filtering messages on type
==========================

In order to filter the messages according to their type, we will use the :class:`pprzlink.message` API. It can be as simple as testing the :code:`name` attribute of the message. 

Here we use this so as to answer with a *PONG* message to any *PING* message sent to us. We send it from our id to the id of the source of the *PING* message.

.. role:: python(code)
   :language: python

.. literalinclude:: serial_tutorial.py
    :language: python
    :lines: 61-64

Complete file
=============

The complete file for this tutorial is :download:`here <serial_tutorial.py>`.


.. literalinclude:: serial_tutorial.py
    :language: python
    :lines: 1,7-


.. toctree::
    :maxdepth: 2
    :glob:
    
    

