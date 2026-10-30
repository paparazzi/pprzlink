// SPDX-License-Identifier: LGPL-3.0-or-later
// Le même agent publie et reçoit à chaque extrémité d'une liaison série physique ou virtuelle.
#include <pprzlink/BoostSerialPortDevice.h>
#include <pprzlink/PprzTransport.h>
#include <boost/system/system_error.hpp>
#include <charconv>
#include <chrono>
#include <csignal>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
  constexpr auto usage = "Usage: pty_agent messages.xml serial-port agent-id (0..255)\n"
                         "Publishes and receives GUIDE_ALTITUDE until Ctrl+C.\n";
}

int main(int argc, char **argv)
{
  // L'aide et les arguments sont traités avant toute ouverture de port ou lecture de XML.
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    std::cout << usage;
    return 0;
  }
  if (argc != 4) { std::cerr << usage; return 2; }
  // La vue emprunte argv pendant main() ; from_chars ne modifie ni le texte ni la locale.
  const std::string_view idText(argv[3]);
  unsigned int id = 0;
  const auto [end, error] = std::from_chars(idText.data(), idText.data() + idText.size(), id);
  // Refuse le texte vide, les suffixes, les débordements et les valeurs hors de l'en-tête uint8.
  if (idText.empty() || error != std::errc{} || end != idText.data() + idText.size() || id > 255) {
    std::cerr << "Agent id must be an integer in [0, 255].\n" << usage;
    return 2;
  }
  try {
    using namespace std::chrono_literals;
    // Le transport emprunte ce catalogue et ce contexte ; l'ordre garantit leur durée de vie.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    boost::asio::io_context context;
    using Serial = pprzlink::BoostSerialPortDevice;
    // Chaque agent ouvre sa propre extrémité et utilise 57600 bauds, 8N1, sans contrôle de flux.
    auto device = std::make_unique<Serial>(context, argv[2]);
    device->setBaudrate(Serial::Baudrate(57600));
    device->setDataBits(Serial::DataBits(8));
    device->setParity(Serial::Parity(Serial::Parity::none));
    device->setStopBits(Serial::StopBits(Serial::StopBits::one));
    device->setFlowcontrol(Serial::Flowcontrol(Serial::Flowcontrol::none));
    // Transfère la propriété du port au transport ; ne plus utiliser device après cet appel.
    pprzlink::PprzTransport transport(std::move(device), dictionary);

    // Une mesure dépendant de l'identifiant permet de distinguer les deux agents à l'affichage.
    const double metres = 123.5 + id;
    pprzlink::Message altitude(dictionary.getDefinition("GUIDE_ALTITUDE"));
    altitude.setSenderId(id);
    altitude.setReceiverId(255); // Diffusion : chaque agent peut recevoir, sans rôle sol/avion imposé.
    // Le setter reçoit des mètres SI ; le XML fixe le stockage et l'unité sur la liaison.
    altitude.setFieldSI("altitude", metres);
    // Le signal et la lecture gardent run() actif ; le timer assure les publications périodiques.
    boost::asio::signal_set signals(context, SIGINT, SIGTERM);
    boost::asio::steady_timer transmit(context);
    // Un signal termine sa propre attente et annule lecture et publication suivante.
    signals.async_wait([&](const boost::system::error_code &error, int) {
      if (!error) { transport.stop(); transmit.cancel(); }
    });
    // Le prédicat capture id par valeur et complète le filtre de nom du message.
    // Écarte les propres messages de l'agent et accepte son adresse ou la diffusion 255.
    transport.bind("GUIDE_ALTITUDE", {.where = [id](const pprzlink::Message &message) {
      return std::get<uint8_t>(message.getSenderId()) != id &&
             (message.getReceiverId() == id || message.getReceiverId() == 255);
    }}, [&](const pprzlink::Message &message) {
      // Le message est emprunté pour cet appel ; le senderId binaire est un uint8_t.
      const auto sender = std::get<uint8_t>(message.getSenderId());
      // Le + unaire promeut l'octet en entier et évite son affichage comme caractère.
      std::cout << "Agent " << id << " RX from " << +sender << ": GUIDE_ALTITUDE "
                << message.getFieldSI("altitude") << " m" << std::endl;
    });
    // Réutilise le message rempli et reprogramme un envoi, sans thread ni boucle de sommeil.
    std::function<void()> send = [&] {
      transport.sendMessage(altitude);
      std::cout << "Agent " << id << " TX GUIDE_ALTITUDE " << metres << " m" << std::endl;
      // Le callback annulé reçoit operation_aborted ; il ne doit pas relancer cette fonction.
      transmit.expires_after(1s);
      transmit.async_wait([&](const boost::system::error_code &error) { if (!error) send(); });
    };
    // Arme la réception avant la première publication et annonce le port effectivement ouvert.
    transport.start();
    std::cout << "Agent " << id << " ready on " << argv[2] << std::endl;
    send();
    // Exécute réception, timer et signal sur ce thread jusqu'à l'arrêt et à leurs annulations.
    context.run();
    return 0;
  } catch (const boost::system::system_error &error) {
    // Le nom du port aide à diagnostiquer une ouverture ou une opération série refusée.
    std::cerr << argv[0] << ": serial port '" << argv[2] << "': " << error.code().message() << '\n';
    return 1;
  } catch (const std::exception &error) {
    // Les autres erreurs comprennent le catalogue XML et les conversions de champs.
    std::cerr << argv[0] << ": " << error.what() << '\n';
    return 1;
  }
}
