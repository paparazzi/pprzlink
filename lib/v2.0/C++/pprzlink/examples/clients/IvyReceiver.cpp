// SPDX-License-Identifier: LGPL-3.0-or-later
// Reçoit une altitude sur Ivy et termine après le premier message, ou après dix secondes.
#include <pprzlink/IvyLink.h>
#include <chrono>
#include <iostream>

int main(int argc, char **argv)
{
  // Le catalogue décrit les messages ; le domaine choisit le bus Ivy auquel se connecter.
  if (argc != 3) {
    std::cerr << "Usage: ivy_receiver messages.xml ivy-domain\n";
    return 2;
  }
  try {
    // Le dictionnaire doit survivre au lien, qui emprunte ses définitions.
    const pprzlink::MessageDictionary dictionary(argv[1]);
    // Sans argument true, la boucle Ivy sera exécutée par link.run() sur le thread principal.
    pprzlink::IvyLink link(dictionary, "ivy-receiver-example", argv[2]);
    bool received = false;

    // bind() conserve l'abonnement dans le lien ; le Message est emprunté pendant ce callback.
    // [&] permet de modifier received et d'arrêter link, tous deux vivants pendant run().
    link.bind("CLIENT_ALTITUDE", [&](const pprzlink::Message &message) {
      // Restitue des mètres en double, même si le XML stocke une autre unité ou un entier.
      const double metres = message.getFieldSI("altitude");
      // Sur Ivy, le senderId est textuel, y compris pour un expéditeur nommé "42".
      std::cout << "Aircraft " << std::get<std::string>(message.getSenderId()) << ": " << metres << " m\n";
      received = true;
      // Demande la sortie de la boucle Ivy après ce callback.
      link.stop();
    });
    // Conserve le jeton du timer : sa destruction annulerait l'attente immédiatement.
    // Ce timer à déclenchement unique borne aussi l'attente lorsqu'aucun message n'arrive.
    auto timeout = link.getBus().bind_event(
      [&](std::chrono::milliseconds) { link.stop(); }, ivy::after(std::chrono::seconds(10)));
    // Vérifie l'enregistrement avant de lancer la boucle d'événements.
    if (!timeout) throw std::system_error(timeout.error(), "Ivy timeout");
    // Traite les messages et le timer sur ce thread ; aucun thread supplémentaire n'est créé ici.
    link.run();
    if (!received) {
      std::cerr << "No altitude received within 10 seconds\n";
      return 1;
    }
  } catch (const std::exception &error) {
    // Remonte les erreurs de catalogue, de conversion SI ou de configuration du bus.
    std::cerr << error.what() << '\n';
    return 1;
  }
}
