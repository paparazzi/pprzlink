// SPDX-License-Identifier: LGPL-3.0-or-later
// Prépare une commande complète, puis envoie un datagramme à un récepteur UDP local.
#include <pprzlink/UdpTransport.h>
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>

int main(int argc, char **argv)
{
  // Le dernier argument est le port distant ; un autre programme doit l'écouter.
  if (argc != 3) {
    std::cerr << "Usage: udp_setting_sender messages.xml destination-port\n";
    return 2;
  }
  try {
    // Valide le nombre entier, sa plage uint16_t et l'absence de caractères après le nombre.
    // Le port 0 est utile pour une écoute automatique, mais ne désigne pas une destination valide ici.
    const std::string_view portText(argv[2]);
    uint16_t port = 0;
    const auto [end, error] = std::from_chars(portText.data(), portText.data() + portText.size(), port);
    if (portText.empty() || error != std::errc{} || end != portText.data() + portText.size() || port == 0)
      throw std::invalid_argument("Destination port must be an integer in [1, 65535]");

    // Le catalogue et le contexte doivent survivre au socket détenu par le transport.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    boost::asio::io_context context;
    // Le constructeur choisit un port source libre ; la destination sera fournie lors de l'envoi.
    pprzlink::UdpTransport transport(context, dictionary);

    // Le nom XML sélectionne les champs de la commande ; le message commence sans valeurs.
    pprzlink::Message command(dictionary.getDefinition("CLIENT_SETTING"));
    // L'en-tête représente la station sol 0 et l'avion 42, indépendamment de l'adresse UDP.
    command.setSenderId(0);
    command.setReceiverId(42);

    // Les couples nom/valeur remplissent ac_id et value avec conversion vers leurs types XML.
    // setField() renvoie une référence constante au message rempli, utilisable directement à l'envoi.
    // sendMessage() écrit le datagramme de façon synchrone : aucune boucle run() n'est nécessaire.
    const auto bytes = transport.sendMessage(
      command.setField("ac_id", 42, "value", 12.5), {"127.0.0.1", port});
    // Le résultat compte les octets écrits ; il ne confirme pas l'application de la commande distante.
    std::cout << "Sent " << command.toString() << " (" << bytes << " bytes)\n";
    return 0;
  } catch (const std::exception &error) {
    // Une erreur de saisie, de champ XML ou d'envoi remonte jusqu'à ce point.
    std::cerr << error.what() << '\n';
    return 1;
  }
}
