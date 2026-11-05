// SPDX-License-Identifier: LGPL-3.0-or-later
// Reçoit trois messages UDP et affiche l'origine réseau propre à chaque réception.
#include <pprzlink/UdpTransport.h>
#include <pprzlink/IvyMessageCodec.h>
#include <charconv>
#include <chrono>
#include <iostream>
#include <boost/asio/steady_timer.hpp>

int main(int argc, char **argv)
{
  // Le port est celui de l'écoute locale ; 0 demande au système d'en choisir un libre.
  if (argc != 3) {
    std::cerr << "Usage: udp_recorder messages.xml local-port (0 chooses a free port)\n";
    return 2;
  }
  try {
    using namespace std::chrono_literals;
    // from_chars contrôle la plage du uint16_t ; end vérifie que tout le texte a été consommé.
    const std::string_view text(argv[2]);
    uint16_t port = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
    if (error != std::errc{} || end != text.data() + text.size())
      throw std::invalid_argument("Invalid UDP port");
    // Dictionnaire et contexte sont déclarés avant le transport pour lui survivre.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    boost::asio::io_context context;
    // L'adresse de loopback limite l'écoute à cette machine.
    pprzlink::UdpTransport transport(context, dictionary,
      {.local = {"127.0.0.1", port}});
    // Affiche le port réellement attribué ; std::endl vide la sortie avant d'attendre les données.
    std::cout << "Listening on " << transport.localEndpoint().port << std::endl;

    // Ce timer borne l'attente même si moins de trois messages arrivent.
    boost::asio::steady_timer timeout(context, 10s);
    int count = 0;
    // Une expiration normale arrête la lecture ; une annulation du timer est ignorée.
    timeout.async_wait([&](const boost::system::error_code &error) { if (!error) transport.stop(); });
    // ALL accepte tous les messages décodés ; les deux arguments sont empruntés pour cet appel.
    transport.bind(pprzlink::ALL, [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
      // udpPeer est présent en UDP et désigne la source réelle, distincte du senderId PPRZLINK.
      // Le codec produit du texte Ivy pour l'affichage, sans ouvrir de bus Ivy.
      std::cout << info.udpPeer->address << ':' << info.udpPeer->port
                << " [" << info.frameSize << " bytes] "
                << pprzlink::ivy_codec::serializeMessage(message) << std::endl;
      // Compte les messages, même si un datagramme en contient plusieurs, puis annule les deux attentes.
      if (++count == 3) { transport.stop(); timeout.cancel(); }
    });
    // Arme la réception après avoir installé le callback.
    transport.start();
    // Exécute les callbacks sur ce thread jusqu'à l'arrêt et au traitement des annulations.
    context.run();
    return count == 3 ? 0 : 1;
  } catch (const std::exception &error) {
    // Signale les erreurs de port, de catalogue ou d'I/O avec un code de sortie non nul.
    std::cerr << error.what() << '\n';
    return 1;
  }
}
