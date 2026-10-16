// SPDX-License-Identifier: LGPL-3.0-or-later
// Un même programme UDP émet (e) une commande ou reçoit (r) un message avec son origine réseau.
#include <pprzlink/UdpTransport.h>
#include <pprzlink/IvyMessageCodec.h>
#include <boost/asio/steady_timer.hpp>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
  int receive(boost::asio::io_context &context, pprzlink::UdpTransport &transport)
  {
    bool received = false;
    // Limite l'attente si l'émetteur n'est pas lancé ; les callbacks annulés sont ignorés.
    boost::asio::steady_timer timeout(context, std::chrono::seconds(10));
    timeout.async_wait([&](const boost::system::error_code &error) { if (!error) transport.stop(); });
    // Le récepteur conserve ce binding ; les références ne sont valides que pendant le callback.
    transport.bind(pprzlink::ALL,
      [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
        // UDP fournit l'IP et le port source réels, indépendants des identifiants PPRZLINK.
        const auto &peer = info.udpPeer.value();
        std::cout << "Received from " << peer.address << ':' << peer.port
                  << " [" << info.frameSize << " bytes] "
                  << pprzlink::ivy_codec::serializeMessage(message) << std::endl;
        received = true;
        // Termine après un message et annule le timer pour libérer la boucle d'événements.
        transport.stop();
        timeout.cancel();
      });
    transport.start();
    // Le port 0 demande une allocation automatique ; expose le port effectivement choisi.
    std::cout << "Listening on " << transport.localEndpoint().port << std::endl;
    // Exécute lecture et timer sur ce thread, sans thread de réception supplémentaire.
    context.run();
    if (!received) std::cerr << "No UDP message received within 10 seconds\n";
    return received ? 0 : 1;
  }

  int emit(pprzlink::UdpTransport &transport, const pprzlink::MessageDictionary &dictionary,
           std::uint16_t port)
  {
    pprzlink::Message command(dictionary.getDefinition("CLIENT_SETTING"));
    // Ces identifiants appartiennent à l'en-tête ; la destination UDP est fournie séparément.
    command.setSenderId(0);
    command.setReceiverId(42);
    // Remplit tous les champs dans l'ordre des couples nom/valeur, avec conversion vers le XML.
    command.setField("ac_id", 42, "value", 12.5);
    // L'écriture est synchrone ; ce rôle n'a besoin ni de start() ni de context.run().
    const auto bytes = transport.sendMessage(command, {"127.0.0.1", port});
    std::cout << "Sent " << command.toString() << " (" << bytes << " bytes)" << std::endl;
    // Le retour confirme les octets écrits, sans accuser réception de la commande distante.
    return 0;
  }
}

int main(int argc, char **argv)
{
  // Le port désigne l'écoute locale en réception, ou le destinataire en émission.
  if (argc != 4 || (std::string_view(argv[3]) != "e" && std::string_view(argv[3]) != "r")) {
    std::cerr << "Usage: " << argv[0] << " messages.xml port e|r\n"
              << "  e: send one setting to 127.0.0.1:port\n"
              << "  r: receive one message; port 0 chooses a free port\n";
    return 2;
  }
  try {
    // from_chars valide le uint16_t et end refuse les caractères après le nombre.
    const std::string_view text(argv[2]);
    std::uint16_t port = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() ||
        (std::string_view(argv[3]) == "e" && port == 0)) {
      throw std::invalid_argument("Invalid UDP port (the destination must be in 1..65535)");
    }
    // Le dictionnaire et le contexte doivent survivre au socket possédé par le transport.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    boost::asio::io_context context;
    const bool receiving = std::string_view(argv[3]) == "r";
    // Le récepteur utilise le port demandé ; le port source de l'émetteur est choisi automatiquement.
    pprzlink::UdpTransport transport(context, dictionary,
      {.local = {"127.0.0.1", receiving ? port : std::uint16_t{0}}});
    if (receiving) return receive(context, transport);
    return emit(transport, dictionary, port);
  } catch (const std::exception &error) {
    // Les erreurs de saisie, de catalogue ou d'I/O donnent un code d'échec.
    std::cerr << error.what() << '\n';
    return 1;
  }
}
