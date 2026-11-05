// SPDX-License-Identifier: LGPL-3.0-or-later
// Un seul programme pour demander (c) ou fournir (s) une altitude sur une liaison série.
#include <pprzlink/BoostSerialPortDevice.h>
#include <pprzlink/PprzTransport.h>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace std::chrono_literals;

namespace {
  int request(boost::asio::io_context &context, pprzlink::PprzTransport &transport,
              const pprzlink::MessageDictionary &dictionary, const char *port)
  {
    // La requête n'a aucun champ ; son en-tête identifie le sol 0 et l'avion 42.
    pprzlink::Message message(dictionary.getDefinition("GUIDE_ALTITUDE_REQ"));
    message.setSenderId(0);
    message.setReceiverId(42);
    bool received = false;
    // La limite globale et la répétition des demandes ont des timers distincts.
    boost::asio::steady_timer timeout(context, 60s), retry(context);
    const auto stop = [&] { transport.stop(); timeout.cancel(); retry.cancel(); };
    timeout.async_wait([&](const boost::system::error_code &error) { if (!error) stop(); });

    // Accepte uniquement l'altitude envoyée par l'avion 42 à cette station sol.
    // Le callback emprunte message/info pendant son appel et lit l'altitude en mètres SI.
    transport.bind("GUIDE_ALTITUDE", {.senderId = 42, .receiverId = 0},
      [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
        std::cout << "Aircraft 42: " << message.getFieldSI("altitude")
                  << " m (" << info.frameSize << " received bytes)" << std::endl;
        received = true;
        // Annule lecture et timers pour que la boucle puisse rendre la main.
        stop();
      });
    // La fonction se reprogramme toutes les 500 ms, sans thread ni boucle de sommeil.
    // Les captures par référence restent valides jusqu'au retour de context.run().
    std::function<void()> sendRequest = [&] {
      transport.sendMessage(message);
      retry.expires_after(500ms);
      retry.async_wait([&](const boost::system::error_code &error) { if (!error) sendRequest(); });
    };
    // Arme la réception avant l'envoi ; le serveur peut alors démarrer dans l'autre terminal.
    transport.start();
    std::cout << "Requester ready on " << port << std::endl;
    sendRequest();
    context.run();
    if (!received) throw std::runtime_error("No altitude reply within 60 seconds");
    return 0;
  }

  int answer(boost::asio::io_context &context, pprzlink::PprzTransport &transport,
             const pprzlink::MessageDictionary &dictionary, const char *port)
  {
    bool answered = false;
    // Borne aussi l'attente du serveur ; une annulation du timer est ignorée par son callback.
    boost::asio::steady_timer timeout(context, 60s);
    timeout.async_wait([&](const boost::system::error_code &error) { if (!error) transport.stop(); });
    // L'application associe explicitement la demande et la réponse par leurs noms XML.
    transport.bind("GUIDE_ALTITUDE_REQ", {.senderId = 0, .receiverId = 42},
      [&](const pprzlink::Message &) {
        pprzlink::Message altitude(dictionary.getDefinition("GUIDE_ALTITUDE"));
        altitude.setSenderId(42);
        altitude.setReceiverId(0);
        // Fournit des mètres ; le XML peut choisir un float en m ou un entier en mm.
        altitude.setFieldSI("altitude", 123.5);
        transport.sendMessage(altitude);
        std::cout << "Answered GUIDE_ALTITUDE_REQ with 123.5 m" << std::endl;
        answered = true;
        // Termine après la première réponse et annule l'échéance restante.
        transport.stop();
        timeout.cancel();
      });
    transport.start();
    std::cout << "Responder ready on " << port << std::endl;
    // Exécute lecture et timer sur ce thread, jusqu'à la réponse ou l'expiration.
    context.run();
    if (!answered) throw std::runtime_error("No altitude request within 60 seconds");
    return 0;
  }
}

int main(int argc, char **argv)
{
  // Le troisième argument choisit le client demandeur ou le serveur répondeur.
  if (argc != 4 || (std::string_view(argv[3]) != "c" && std::string_view(argv[3]) != "s")) {
    std::cerr << "Usage: " << argv[0] << " messages.xml serial-port c|s\n"
              << "  c: request altitude from aircraft 42\n"
              << "  s: answer one altitude request from ground station 0\n";
    return 2;
  }
  try {
    // Catalogue et contexte survivent au transport et à son périphérique.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    boost::asio::io_context context;
    using Serial = pprzlink::BoostSerialPortDevice;
    auto device = std::make_unique<Serial>(context, argv[2]);
    // Configure les deux ports distincts de la liaison en 57600 bauds, 8N1, sans contrôle de flux.
    device->setBaudrate(Serial::Baudrate(57600));
    device->setDataBits(Serial::DataBits(8));
    device->setParity(Serial::Parity(Serial::Parity::none));
    device->setStopBits(Serial::StopBits(Serial::StopBits::one));
    device->setFlowcontrol(Serial::Flowcontrol(Serial::Flowcontrol::none));
    // Le transport prend la propriété exclusive du port ; device devient vide.
    pprzlink::PprzTransport transport(std::move(device), dictionary);
    if (std::string_view(argv[3]) == "c") return request(context, transport, dictionary, argv[2]);
    return answer(context, transport, dictionary, argv[2]);
  } catch (const std::exception &error) {
    // Signale les erreurs XML, d'I/O et les expirations avec un code de sortie non nul.
    std::cerr << argv[0] << ": " << error.what() << '\n';
    return 1;
  }
}
