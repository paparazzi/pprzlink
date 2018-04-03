.. _IvyTutorial:


==============================================
Send and receive messages from Ivy with python
==============================================

This tutorial explains how to send and receive messages to and from an Ivy bus. You should be familiar with basic `IVY <https://www.eei.cena.fr/products/ivy/>`_ concepts.

The majority of the work will be done through :class:`pprzlink.ivy.IvyMessagesInterface` class. You can have a look at the documentation :ref:`here <IvyInterface>`.

In this tutorial, we will build a simple application which receives *PING* messages and sends back *PONG* messages.

Creating an ivy bus
===================

The first step is to create a :class:`pprzlink.ivy.IvyMessagesInterface` object, which is straightforward:

.. role:: python(code)
   :language: python

.. literalinclude:: ivy_tutorial.py
    :language: python
    :lines: 7-9,21-25

Subscribe to messages
=====================

You can then issue subscriptions on the ivy bus with the :func:`pprzlink.ivy.IvyMessagesInterface.subscribe` function. You can subscribe both with a regexp in a string or a :class:`PprzMessage` specifying the type of message you want. Here we are subscribing to all PING messages. 

.. role:: python(code)
   :language: python

.. literalinclude:: ivy_tutorial.py
    :language: python
    :lines: 31-32

The parameter :code:`message.PprzMessage("datalink", "PING")` creates an empty *PING* message which serves as a prototype.

The function passed as first argument to :func:`pprzlink.ivy.IvyMessagesInterface.subscribe` (here :func:`recv_callback`) will be called when a matching message is received. It will be passed two parameters, the id of the sender of the message (`ac_id`) and the message itself (`pprzMsg`).

For now, we only print the message and the id of the sender. Note that since *PING* is a datalink message, the sender `ac_id` will be 0.

.. role:: python(code)
   :language: python

.. literalinclude:: ivy_tutorial.py
    :language: python
    :lines: 14-17
    
    
Sending messages
================

To send a message we use the :func:`pprzlink.ivy.IvyMessagesInterface.send`. We will use this to send a *PONG* message back when we receive a *PING* message. We identify ourselves as `ac_id` 2.

.. role:: python(code)
   :language: python

.. literalinclude:: ivy_tutorial.py
    :language: python
    :lines: 14-19


Complete file
=============

The complete file for this tutorial including waiting for keyboard interuption is :download:`here <ivy_tutorial.py>`.

.. role:: python(code)
   :language: python

.. literalinclude:: ivy_tutorial.py
    :language: python
    :lines: 1,6-

.. toctree::
    :maxdepth: 2
    :glob:


