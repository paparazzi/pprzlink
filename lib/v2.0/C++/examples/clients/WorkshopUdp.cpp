// SPDX-License-Identifier: LGPL-3.0-or-later
// Atelier UDP : recevoir, émettre ou réunir les deux rôles dans un même programme.
#include <pprzlink/UdpTransport.h>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cctype>
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>

// Permet d'écrire les durées chrono sous la forme 2s ou 30s.
using namespace std::chrono_literals;

namespace {
  pprzlink::Message altitude(const pprzlink::MessageDictionary &dictionary, double metres)
  {
    // Crée un message indépendant à partir du schéma XML, puis renseigne son en-tête.
    pprzlink::Message message(dictionary.getDefinition("GUIDE_ALTITUDE"));
    message.setSenderId(42);
    message.setReceiverId(0);
    // Le setter convertit les mètres SI vers l'unité et le type définis dans le XML.
    message.setFieldSI("altitude", metres);
    return message;
  }
  void print(const pprzlink::Message &message, const pprzlink::ReceiveInfo &info)
  {
    // Ces références sont empruntées pendant l'affichage ; aucune donnée n'est conservée ici.
    // Une réception UDP fournit toujours l'adresse et le port source du datagramme.
    const auto &peer = info.udpPeer.value();
    std::cout << "Reçu de " << peer.address << ':' << peer.port
              << " : " << message.toString() << std::endl;
  }
  int both(const pprzlink::MessageDictionary &dictionary)
  {
    // Le contexte est déclaré avant les transports pour leur survivre à la destruction.
    boost::asio::io_context context;
    // Le port 0 laisse le système choisir un port libre pour chaque socket.
    pprzlink::UdpTransport receiver(context, dictionary, {.local = {"127.0.0.1", 0}});
    pprzlink::UdpTransport sender(context, dictionary, {.local = {"127.0.0.1", 0}});
    // Fixe une échéance dans deux secondes, sans bloquer ; run() traitera son expiration.
    boost::asio::steady_timer timeout(context, 2s);
    bool received = false;
    // [&] capture les objets locaux par référence ; ils vivent pendant toute l'exécution de run().
    timeout.async_wait([&](const boost::system::error_code &error) {
      // À l'échéance, error est vide : annule la lecture sans en programmer une autre.
      // Une annulation du timer donne operation_aborted et ne déclenche pas cet arrêt.
      if (!error) receiver.stop();
    });
    // Combine le senderId PPRZLINK et l'origine UDP réelle ; les deux identités sont distinctes.
    receiver.bind("GUIDE_ALTITUDE", {.senderId = 42, .udpPeer = sender.localEndpoint()},
      [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
        print(message, info);
        received = true;
        // Désactive la réception pour empêcher la lecture suivante, sans arrêter le contexte.
        receiver.stop();
        // Annule l'attente restante ; le callback d'une attente annulée reçoit operation_aborted.
        timeout.cancel();
      });
    // Arme la réception avant l'envoi ; le callback sera exécuté quand run() traitera les événements.
    receiver.start();
    // Retrouve le port attribué, à utiliser comme destination du datagramme.
    const auto destination = receiver.localEndpoint();
    // L'écriture est synchrone ; le callback de réception attend encore le traitement dans run().
    const auto sent = sender.sendMessage(altitude(dictionary, 123.5), destination);
    std::cout << "Envoyé : " << sent << " octets vers le port " << destination.port << std::endl;
    // Attend le message ou l'échéance. Après la fin du timer et l'annulation de la lecture,
    // run() traite les dernières annulations puis revient, sans opération encore en attente.
    context.run();
    if (!received) throw std::runtime_error("Aucun message UDP reçu dans le délai prévu");
    return 0;
  }
  int receive(const pprzlink::MessageDictionary &dictionary)
  {
    // Le port 4242 est fixe afin qu'un émetteur lancé dans un autre terminal puisse le viser.
    boost::asio::io_context context;
    pprzlink::UdpTransport receiver(context, dictionary, {.local = {"127.0.0.1", 4242}});
    // Le timer et l'attente de signal sont deux opérations qui maintiendront run() actif.
    boost::asio::steady_timer timeout(context, 30s);
    // Asio remettra Ctrl+C (SIGINT) ou SIGTERM à son callback, pendant run().
    boost::asio::signal_set signals(context, SIGINT, SIGTERM);
    unsigned int count = 0;
    // Les deux chemins d'arrêt annulent toutes les attentes, y compris celle du signal.
    // Annuler seulement la lecture et le timer laisserait run() attendre un signal.
    auto stop = [&] {
      receiver.stop();
      timeout.cancel();
      signals.cancel();
    };
    timeout.async_wait([&](const boost::system::error_code &error) {
      // Une expiration normale arrête la réception ; une annulation est ignorée.
      if (!error) stop();
    });
    signals.async_wait([&](const boost::system::error_code &error, int) {
      // Un signal reçu arrête le programme ; l'annulation de cette attente ne fait rien.
      if (!error) stop();
    });
    // ALL sélectionne tous les messages ; les deux références sont empruntées pour l'appel.
    receiver.bind(pprzlink::ALL, [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
      print(message, info);
      ++count;
    });
    // Commence les lectures asynchrones, qui seront traitées par le thread appelant run().
    receiver.start();
    std::cout << "Récepteur prêt sur 127.0.0.1:4242 (30 s, ou Ctrl+C)" << std::endl;
    // Continue à recevoir jusqu'à l'expiration ou au signal. stop() annule les trois attentes ;
    // run() traite leurs fins puis revient dès qu'il ne reste plus de travail.
    context.run();
    if (!count) throw std::runtime_error("Aucun message UDP reçu");
    return 0;
  }
  int emit(const pprzlink::MessageDictionary &dictionary)
  {
    // Le port source est libre ; seule la destination doit correspondre à l'écoute du récepteur.
    boost::asio::io_context context;
    pprzlink::UdpTransport sender(context, dictionary, {.local = {"127.0.0.1", 0}});
    const pprzlink::UdpEndpoint destination{"127.0.0.1", 4242};
    // Prépare et envoie trois messages distincts, chacun contenant sa propre mesure.
    for (const double metres : {100.0, 110.0, 123.5}) {
      // L'envoi UDP est synchrone : ce mode n'a pas besoin de run(), ni d'un abonnement de réception.
      const auto sent = sender.sendMessage(altitude(dictionary, metres), destination);
      std::cout << "Envoyé : " << sent << " octets vers le port " << destination.port << std::endl;
    }
    return 0;
  }
}

int main(int argc, char **argv)
{
  // Le rôle est fourni après le XML ; le contrôle évite de lire le premier caractère d'un texte vide.
  if (argc != 3 || !argv[2][0]) {
    std::cerr << "Usage: " << argv[0] << " messages.xml both|emitter|receiver\n";
    return 2;
  }
  try {
    // Le catalogue reste vivant jusqu'au retour du rôle choisi et à la destruction de ses transports.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    // toupper exige une valeur représentable en unsigned char ; évite un char signé négatif.
    switch (std::toupper(static_cast<unsigned char>(argv[2][0]))) {
      case 'B': return both(dictionary);
      case 'E': return emit(dictionary);
      case 'R': return receive(dictionary);
      default: throw std::invalid_argument("Mode attendu : both, emitter ou receiver");
    }
  } catch (const std::exception &error) {
    std::cerr << "Erreur : " << error.what() << '\n';
    return 1;
  }
}
