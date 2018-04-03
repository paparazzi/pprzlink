===============
Messages
===============

Messages are defined in the `messages.xml`_ file and are grouped into the following message classes:

.. _`messages.xml`: https://github.com/enacuavlab/pprzlink/blob/sphinx_doc/message_definitions/v1.0/messages.xml

    * telemetry
    * datalink
    * ground
    * alert
    * intermcu

The list of `Paparazzi Messages`_ can be found in the generated Doxygen documentation.
Details of the messages binary format can be found in the related `Wiki page <http://wiki.paparazziuav.org/wiki/Messages_Format>`_.

.. _`Paparazzi Messages`: http://docs.paparazziuav.org/latest/paparazzi_messages.html

Message Classes
---------------

Telemetry
~~~~~~~~~

Telemetry messages are sent from the aircraft to the ground and are defined in the telemetry class of the `messages.xml`_ file.

Datalink
~~~~~~~~~

Datalink messages are sent from the ground to the aircraft and are defined in the datalink class of the `messages.xml`_ file.

Ground
~~~~~~~~~

Ground messages are sent to the ground network agents(GCS, server, link, etc...) and are defined in the ground class of the `messages.xml`_ file.

Alert
~~~~~~~~~

TBD

InterMCU
~~~~~~~~~

InterMCU messages are used for communication between airborne MCU when supported and are defined in the intermcu class of the `messages.xml`_ file.

