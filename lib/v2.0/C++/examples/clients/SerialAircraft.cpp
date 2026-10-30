// SPDX-License-Identifier: LGPL-3.0-or-later
// Un même programme série : client sol (c) ou serveur avion (s).
#include <pprzlink/BoostSerialPortDevice.h>
#include <pprzlink/PprzTransport.h>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>

// Autorise les durées 10s et 500ms, utilisées par les timers Asio.
using namespace std::chrono_literals;

namespace {
  int server(boost::asio::io_context &context, pprzlink::PprzTransport &transport,
             const pprzlink::MessageDictionary &dictionary, const char *port)
  {
    bool answered = false;
    // Une attente bornée permet au serveur de terminer si aucun client ne se présente.
    boost::asio::steady_timer timeout(context, 10s);
    // Le callback s'exécute dans run() ; une annulation du timer fournit une erreur et est ignorée.
    timeout.async_wait([&](const boost::system::error_code &error) { if (!error) transport.stop(); });

    // Écoute uniquement PING adressé à l'avion 42, selon l'en-tête binaire PPRZLINK.
    // Les références au message et aux métadonnées sont empruntées pour cet appel.
    transport.bind("PING", {.receiverId = 42}, [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
      // Le décodeur binaire fournit un senderId numérique ; les réponses visent cet expéditeur.
      const auto sender = std::get<std::uint8_t>(message.getSenderId());
      pprzlink::Message altitude(dictionary.getDefinition("CLIENT_ALTITUDE"));
      altitude.setSenderId(42);
      altitude.setReceiverId(sender);
      // Fournit des mètres SI ; le XML choisit l'unité et le type réellement encodés.
      altitude.setFieldSI("altitude", 123.5);
      // Publie après le PING : le client a déjà ouvert son port et installé ses abonnements.
      transport.sendMessage(altitude);

      pprzlink::Message pong(dictionary.getDefinition("PONG"));
      pong.setSenderId(42);
      pong.setReceiverId(sender);
      // Les écritures sont synchrones : la trame d'altitude précède celle du PONG sur la liaison.
      transport.sendMessage(pong);
      std::cout << "Answered PING (" << info.frameSize << " received bytes)\n";
      answered = true;
      // Termine après un échange ; annule aussi le timer pour libérer la boucle Asio.
      transport.stop();
      timeout.cancel();
    });
    // bind() seul ne lit pas le port ; start() arme les lectures avant d'annoncer le serveur.
    transport.start();
    std::cout << "Server ready on " << port << " (aircraft 42, 57600 baud, 8N1)" << std::endl;
    // Traite les messages et le timer sur ce thread, jusqu'à la réponse ou à l'expiration.
    context.run();
    if (answered) return 0;
    std::cerr << "No PING received within 10 seconds\n";
    return 1;
  }

  int client(boost::asio::io_context &context, pprzlink::PprzTransport &transport,
             const pprzlink::MessageDictionary &dictionary, const char *port)
  {
    // Le client attend les deux messages ; un PONG seul ne valide pas la mesure d'altitude.
    bool receivedAltitude = false, receivedPong = false;
    boost::asio::steady_timer timeout(context, 10s), retry(context);
    // Arrête la réception et la relance périodique, puis laisse run() traiter les annulations.
    const auto stop = [&] { transport.stop(); timeout.cancel(); retry.cancel(); };
    timeout.async_wait([&](const boost::system::error_code &error) { if (!error) stop(); });
    const auto finish = [&] { if (receivedAltitude && receivedPong) stop(); };

    // Combine le nom du message, l'avion expéditeur 42 et le destinataire sol 0.
    transport.bind("CLIENT_ALTITUDE", {.senderId = 42, .receiverId = 0},
      [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
        std::cout << "Aircraft 42: " << message.getFieldSI("altitude")
                  << " m (" << info.frameSize << " received bytes)" << std::endl;
        receivedAltitude = true;
        // Accepte aussi les deux réponses dans l'ordre inverse : finish() vérifie l'état complet.
        finish();
      });
    transport.bind("PONG", {.senderId = 42, .receiverId = 0},
      [&](const pprzlink::Message &, const pprzlink::ReceiveInfo &info) {
        std::cout << "Received PONG (" << info.frameSize << " received bytes)" << std::endl;
        receivedPong = true;
        finish();
      });

    // PING n'a aucun champ XML ; seuls les identifiants de son en-tête sont renseignés.
    pprzlink::Message ping(dictionary.getDefinition("PING"));
    ping.setSenderId(0);
    ping.setReceiverId(42);
    // Cette fonction se reprogramme par timer ; elle ne crée ni thread ni boucle de sommeil.
    std::function<void()> sendPing = [&] {
      const auto sent = transport.sendMessage(ping);
      std::cout << "Sent PING (" << sent << " bytes)" << std::endl;
      // Réessaie si le serveur n'était pas encore prêt ; la réception complète annule ce timer.
      retry.expires_after(500ms);
      retry.async_wait([&](const boost::system::error_code &error) { if (!error) sendPing(); });
    };
    // Installe la réception avant le premier envoi pour traiter même une réponse immédiate.
    transport.start();
    std::cout << "Client ready on " << port << " (ground station 0, 57600 baud, 8N1)" << std::endl;
    sendPing();
    // Le même thread exécute les callbacks de réception, de relance et d'expiration.
    context.run();
    if (receivedAltitude && receivedPong) return 0;
    std::cerr << "No complete altitude/PONG reply within 10 seconds\n";
    return 1;
  }
}

int main(int argc, char **argv)
{
  // Le troisième argument choisit le rôle ; argc protège l'accès à argv[3].
  if (argc != 4 || (std::string_view(argv[3]) != "c" && std::string_view(argv[3]) != "s")) {
    std::cerr << "Usage: " << argv[0] << " messages.xml serial-port c|s\n"
              << "  c: client, send PING and receive altitude/PONG\n"
              << "  s: server, answer one PING with altitude/PONG\n";
    return 2;
  }
  try {
    // Le dictionnaire est chargé à l'exécution et doit survivre au transport qui l'emprunte.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    // Déclaré avant le périphérique, le contexte Asio leur survivra à la destruction.
    boost::asio::io_context context;
    using Serial = pprzlink::BoostSerialPortDevice;
    auto device = std::make_unique<Serial>(context, argv[2]);
    // Les deux processus ouvrent des ports distincts, configurés de la même façon : 57600, 8N1.
    device->setBaudrate(Serial::Baudrate(57600)); // Débit de la liaison.
    device->setDataBits(Serial::DataBits(8)); // Huit bits de données.
    device->setParity(Serial::Parity(Serial::Parity::none)); // N : aucune parité.
    device->setStopBits(Serial::StopBits(Serial::StopBits::one)); // Un bit d'arrêt.
    device->setFlowcontrol(Serial::Flowcontrol(Serial::Flowcontrol::none)); // Aucun contrôle de flux.
    // Le transport devient propriétaire du port ; device est vide après ce transfert.
    pprzlink::PprzTransport transport(std::move(device), dictionary);
    if (std::string_view(argv[3]) == "c") return client(context, transport, dictionary, argv[2]);
    return server(context, transport, dictionary, argv[2]);
  } catch (const std::exception &error) {
    // Les erreurs XML, de port série et de callbacks remontent jusqu'à ce point.
    std::cerr << error.what() << '\n';
    return 1;
  }
}
