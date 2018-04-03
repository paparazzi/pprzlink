.. _UDPTutorial:

========================================================
Send and receive messages from an udp socket with python
========================================================

This tutorial explains how to send and receive messages to and from an udp socket.

We will make a program that wait for messages and print them. If the message received is a *PING* message it answers with a *PONG* message. Furthermore it will have the option to send *PING* messages every two seconds.

The tutorial code will be written in the :class:`UDPTutorial` class so as to keep state information.

Creating the :class:`pprzlink.udp.UdpMessagesInterface`
==============================================

First of all we need to create a :class:`pprzlink.udp.UdpMessagesInterface` object to handle sending and receiving data.

.. role:: python(code)
   :language: python

.. literalinclude:: udp_tutorial.py
    :language: python
    :lines: 11,46-50

Here, we create an interface that will receive message on port :code:`ping_port` and send its messages toward port :code:`pong_port`. It will call :func:`self.proccess_msg` when a message is received (see :ref:`udp_tutorial_receiving_msg`) and filter messages that are sent to id :code:`my_receiver_id`.

.. _udp_tutorial_receiving_msg:

Receiving messages
==================

When a message arrives on the udp port specified as :code:`downlink_port` in the creation of the interface, the id of the destination of the message is checked. If the :code:`interface_id` specified in the interface creation is the same as the destination id of the message or if it was specified as :code:`None`, then the callback function will be called. 

.. role:: python(code)
   :language: python

.. literalinclude:: udp_tutorial.py
    :language: python
    :lines: 53-54,62

Here the callback function just prints the message.

Sending messages
==================

To send a message, we just need to call the :func:`pprzlink.udp.UdpMessagesInterface.send` function.

.. role:: python(code)
   :language: python

.. literalinclude:: udp_tutorial.py
    :language: python
    :lines: 71-72,2,77,79-84


Filtering messages on type
==========================

In order to filter the messages according to their type, we will use the :class:`pprzlink.message` API. It can be as simple as testing the :code:`name` attribute of the message.

.. role:: python(code)
   :language: python

.. literalinclude:: udp_tutorial.py
    :language: python
    :lines: 56-57

Complete file
=============

The complete file for this tutorial including waiting for keyboard interuption and selecting who sends the *PING* is :download:`here <udp_tutorial.py>`.

It can be tested by running it twice, one time without the `-s` switch and one with it.

.. role:: python(code)
   :language: python

.. literalinclude:: udp_tutorial.py
    :language: python
    :lines: 1,6-

.. toctree::
    :maxdepth: 2
    :glob:
    
    

