// Échange une altitude entre deux liens Ivy du même programme, avec découverte du récepteur.
#include <pprzlink/IvyLink.h>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <unistd.h>

// Permet de noter les délais 5s et 10ms sans construire explicitement les durées.
using namespace std::chrono_literals;

namespace {
  // Attend l'annonce de l'application distante et de ses abonnements avant la publication.
  void waitForReceiver(pprzlink::IvyLink &sender, const std::string &receiverName)
  {
    // L'horloge monotone donne une échéance indépendante des corrections de l'heure système.
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
      // Le résultat distingue une erreur de bus d'une recherche réussie sans application trouvée.
      auto peer = sender.getBus().find_application(receiverName);
      if (!peer) throw std::system_error(peer.error(), "Find Ivy receiver");
      // *peer est un optional ; **peer fournit l'application seulement lorsqu'elle est présente.
      if (*peer) {
        auto bindings = sender.getBus().application_regexps(**peer);
        if (!bindings) throw std::system_error(bindings.error(), "Read Ivy subscriptions");
        // La connexion seule ne suffit pas : le récepteur doit avoir annoncé ses expressions d'abonnement.
        if (!bindings->empty()) return;
      }
      // Seul le thread principal attend ici ; les threads Ivy continuent la découverte du bus.
      std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("Timed out waiting for the Ivy receiver's subscription");
  }
}

int main(int argc, char **argv)
{
  // Le domaine peut être fourni pour rejoindre un bus existant, ou choisi automatiquement.
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " messages.xml [Ivy-domain]\n";
    return 1;
  }

  try {
    // Charge les schémas ; le dictionnaire doit survivre aux deux liens qui l'empruntent.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    const auto &definition = dictionary.getDefinition("EXAMPLE_ALTITUDE");
    // Le PID distingue les noms d'application et réduit les collisions de domaine entre essais locaux.
    const auto suffix = std::to_string(getpid());
    const auto domain = argc == 3 ? std::string(argv[2])
      : "127.255.255.255:" + std::to_string(40000 + getpid() % 10000);

    // La promesse transmet le message du thread Ivy au thread principal.
    // Déclarée avant les liens, elle restera vivante jusqu'à la fin de leurs callbacks.
    std::promise<pprzlink::Message> received;
    auto reception = received.get_future();
    const auto receiverName = "example-receiver-" + suffix;
    // true démarre une boucle Ivy sur un thread détenu par le lien ; aucun appel à run() n'est requis.
    pprzlink::IvyLink receiver(dictionary, receiverName, domain, true);
    // Conserve le jeton subscription : sa destruction désabonne le callback.
    auto subscription = receiver.subscribeMessage(definition, [&](std::string, pprzlink::Message message) {
      // Le callback reçoit son propre Message et en transfère les valeurs vers le future.
      received.set_value(std::move(message));
    });

    // Le second lien possède aussi son thread ; les deux applications utilisent le même domaine.
    pprzlink::IvyLink sender(dictionary, "example-sender-" + suffix, domain, true);
    pprzlink::Message outgoing(definition);
    // Le codec Ivy représentera cet identifiant numérique sous la forme textuelle "42".
    outgoing.setSenderId(uint8_t{42});

    // Attend l'abonnement, renseigne l'altitude en mètres SI et publie le message complet.
    waitForReceiver(sender, receiverName);
    sender.sendMessage(outgoing.setFieldSI("altitude", 123.5));
    std::cout << "Sent: " << outgoing.toString() << '\n';
    // Attend le résultat sur le thread principal, avec un délai indépendant de la découverte.
    if (reception.wait_for(5s) != std::future_status::ready) {
      throw std::runtime_error("Timed out waiting for the altitude message");
    }
    // Consomme le future une seule fois ; le message retourné possède ses valeurs.
    const auto incoming = reception.get();
    const double altitude = incoming.getFieldSI("altitude");
    // Sur Ivy, l'expéditeur est textuel ; getFieldSI() reste indépendant du stockage XML.
    const auto &source = std::get<std::string>(incoming.getSenderId());
    if (source != "42" || altitude != 123.5) {
      throw std::runtime_error("Received message differs from the sent message");
    }
    std::cout << "Received from " << source << ": altitude = " << altitude << " m\n";
    // La destruction des liens arrête et rejoint leurs threads avant celle de la promesse et du catalogue.
  } catch (const std::exception &error) {
    // Les erreurs de découverte, d'envoi ou de résultat sont signalées au processus appelant.
    std::cerr << error.what() << '\n';
    return 1;
  }
}
