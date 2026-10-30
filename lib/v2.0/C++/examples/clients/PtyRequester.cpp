// SPDX-License-Identifier: LGPL-3.0-or-later
// Côté sol de l'atelier : demande une altitude à l'avion 42 et affiche sa réponse.
#include <pprzlink/BoostSerialPortDevice.h>
#include <pprzlink/PprzTransport.h>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>

int main(int argc, char **argv)
{
  // Chaque processus ouvre un port différent ; les deux ports doivent être reliés.
  if (argc != 3) {
    std::cerr << "Usage: pty_requester messages.xml serial-port\n";
    return 2;
  }
  try {
    using namespace std::chrono_literals;
    // Le transport emprunte le dictionnaire et le contexte, déclarés avant lui pour lui survivre.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    boost::asio::io_context context;
    using Serial = pprzlink::BoostSerialPortDevice;
    // Ouvre le port physique ou le pseudo-terminal et prépare sa propriété exclusive.
    auto device = std::make_unique<Serial>(context, argv[2]);
    // Configure 57600 bauds, huit bits, aucune parité, un bit d'arrêt et aucun contrôle de flux.
    device->setBaudrate(Serial::Baudrate(57600));
    device->setDataBits(Serial::DataBits(8));
    device->setParity(Serial::Parity(Serial::Parity::none));
    device->setStopBits(Serial::StopBits(Serial::StopBits::one));
    device->setFlowcontrol(Serial::Flowcontrol(Serial::Flowcontrol::none));
    // Le transport prend la propriété du port et transforme les octets en trames PPRZLINK v2.
    pprzlink::PprzTransport transport(std::move(device), dictionary);

    // GUIDE_ALTITUDE_REQ n'a pas de champs : les identifiants sont ceux de l'en-tête binaire.
    pprzlink::Message request(dictionary.getDefinition("GUIDE_ALTITUDE_REQ"));
    request.setSenderId(0);
    request.setReceiverId(42);
    // std::endl vide la sortie pour que le second terminal voie immédiatement l'annonce.
    std::cout << "Requester ready on " << argv[2] << std::endl;
    bool received = false;
    // Deux attentes distinctes : limite globale de 60 s et relance périodique des demandes.
    boost::asio::steady_timer timeout(context, 60s), retry(context);
    timeout.async_wait([&](const boost::system::error_code &error) {
      // Une expiration arrête lecture et relances ; les callbacks d'annulation sont ignorés.
      if (!error) { transport.stop(); retry.cancel(); }
    });
    // std::function permet au callback du timer de rappeler cette fonction par référence.
    // Les objets capturés restent vivants pendant toute l'exécution de context.run().
    std::function<void()> sendRequest = [&] {
      // L'envoi écrit immédiatement la trame ; il ne confirme pas sa réception distante.
      transport.sendMessage(request);
      // La répétition permet de démarrer le répondeur après le demandeur, sans boucle de sommeil.
      retry.expires_after(500ms);
      retry.async_wait([&](const boost::system::error_code &error) { if (!error) sendRequest(); });
    };
    // Accepte uniquement une altitude de l'avion 42 adressée à la station sol 0.
    transport.bind("GUIDE_ALTITUDE", {.senderId = 42, .receiverId = 0},
      [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
        // Les arguments sont empruntés pendant cet appel ; la lecture SI restitue des mètres.
        std::cout << "Aircraft 42: " << message.getFieldSI("altitude")
                  << " m (" << info.frameSize << " received bytes)\n";
        received = true;
        // Annule toutes les opérations restantes pour que run() puisse rendre la main.
        transport.stop(); timeout.cancel(); retry.cancel();
      });
    // Installe la réception avant le premier envoi ; bind() seul n'arme aucune lecture.
    transport.start();
    sendRequest();
    // Attend et traite la réception et les timers sur ce thread, sans thread supplémentaire.
    context.run();
    if (received) return 0;
    throw std::runtime_error("No altitude reply within 60 seconds");
  } catch (const std::exception &error) {
    // Les erreurs de catalogue, de port ou l'absence de réponse donnent un code d'échec.
    std::cerr << argv[0] << ": " << error.what() << '\n';
    return 1;
  }
}
