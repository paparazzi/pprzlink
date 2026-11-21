// SPDX-License-Identifier: LGPL-3.0-or-later
// Côté avion de l'atelier : répond à une demande d'altitude de la station sol 0.
#include <pprzlink/BoostSerialPortDevice.h>
#include <pprzlink/PprzTransport.h>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>

int main(int argc, char **argv)
{
  // Utilise le même XML que le demandeur et l'autre extrémité de la liaison série.
  if (argc != 3) {
    std::cerr << "Usage: pty_responder messages.xml serial-port\n";
    return 2;
  }
  try {
    using namespace std::chrono_literals;
    // Le catalogue et le contexte doivent survivre au transport qui les emprunte.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    boost::asio::io_context context;
    using Serial = pprzlink::BoostSerialPortDevice;
    // Ouvre le port fourni, physique ou virtuel ; make_unique en prépare la propriété exclusive.
    auto device = std::make_unique<Serial>(context, argv[2]);
    // Les deux extrémités partagent la configuration 57600 bauds, 8N1, sans contrôle de flux.
    device->setBaudrate(Serial::Baudrate(57600));
    device->setDataBits(Serial::DataBits(8));
    device->setParity(Serial::Parity(Serial::Parity::none));
    device->setStopBits(Serial::StopBits(Serial::StopBits::one));
    device->setFlowcontrol(Serial::Flowcontrol(Serial::Flowcontrol::none));
    // Le transport devient propriétaire du port ; device est vide après std::move.
    pprzlink::PprzTransport transport(std::move(device), dictionary);

    // Vide la sortie pour permettre au demandeur de voir l'annonce sans attendre la fin du programme.
    std::cout << "Responder ready on " << argv[2] << std::endl;
    bool answered = false;
    // Ce timer borne l'attente ; son callback annulé ne doit pas provoquer un nouvel arrêt.
    boost::asio::steady_timer timeout(context, 60s);
    timeout.async_wait([&](const boost::system::error_code &error) { if (!error) transport.stop(); });
    // Combine le type de demande et les identifiants de l'en-tête, distincts des noms des ports.
    transport.bind("GUIDE_ALTITUDE_REQ", {.senderId = 0, .receiverId = 42}, [&](const pprzlink::Message &) {
      // La requête n'a pas de champs ; l'application construit explicitement le message de réponse.
      pprzlink::Message altitude(dictionary.getDefinition("GUIDE_ALTITUDE"));
      altitude.setSenderId(42);
      altitude.setReceiverId(0);
      // Fournit des mètres ; la bibliothèque convertit vers le type et l'unité du XML.
      altitude.setFieldSI("altitude", 123.5);
      // L'écriture est synchrone et peut être effectuée depuis le callback de réception.
      transport.sendMessage(altitude);
      std::cout << "Answered GUIDE_ALTITUDE_REQ with 123.5 m\n";
      // Termine après une réponse et libère toutes les attentes qui maintiennent run() actif.
      answered = true; transport.stop(); timeout.cancel();
    });
    // Commence la lecture après l'enregistrement ; run() exécutera ensuite les callbacks sur ce thread.
    transport.start();
    context.run();
    if (answered) return 0;
    throw std::runtime_error("No altitude request within 60 seconds");
  } catch (const std::exception &error) {
    // Les erreurs XML, d'I/O et l'expiration sont rapportées au terminal avec un code d'échec.
    std::cerr << argv[0] << ": " << error.what() << '\n';
    return 1;
  }
}
