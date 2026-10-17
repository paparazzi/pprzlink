// SPDX-License-Identifier: LGPL-3.0-or-later
// Atelier Ivy : le même programme publie (e) ou reçoit (r) une altitude.
#include <pprzlink/IvyLink.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace {
    void waitForReceiver(pprzlink::IvyLink &link)
    {
        // Ivy découvre les applications et leurs abonnements de façon asynchrone.
        // L'horloge monotone borne cette découverte à cinq secondes.
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            // Un résultat valide peut encore contenir un optional vide : aucun pair trouvé.
            const auto peer = link.getBus().find_application("guide-recepteur");
            if (!peer) throw std::system_error(peer.error(), "Recherche du récepteur Ivy");
            if (*peer) {
                // **peer fournit l'application ; ses regex décrivent les abonnements annoncés.
                const auto bindings = link.getBus().application_regexps(**peer);
                if (!bindings) throw std::system_error(bindings.error(), "Abonnements Ivy");
                if (!bindings->empty()) return;
            }
            // Le thread principal attend ; le thread Ivy continue à découvrir les pairs.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("Abonnement du récepteur introuvable après 5 secondes");
    }

    int receive(const pprzlink::MessageDictionary &dictionary, const std::string &domain)
    {
        bool received = false;
        // Sans argument true, la boucle Ivy sera exécutée par link.run() sur ce thread.
        pprzlink::IvyLink link(dictionary, "guide-recepteur", domain);
        // Conserve le jeton : sa destruction désabonne le callback.
        // [&] permet de modifier received et d'arrêter link pendant toute l'attente.
        auto subscription = link.subscribeMessage("GUIDE_ALTITUDE",
            [&](std::string sender, pprzlink::Message message) {
                // Le callback reçoit un message complet ; son expéditeur Ivy est textuel.
                std::cout << "Altitude de " << sender << " : "
                          << message.getFieldSI("altitude") << " m" << std::endl;
                received = true;
                link.close();
            });
        // Le résultat possède le jeton du timer ; sa durée de vie conserve l'attente active.
        auto timeout = link.getBus().bind_event(
            [&](std::chrono::milliseconds) { link.close(); },
            ivy::after(std::chrono::seconds(10)));
        if (!timeout) throw std::system_error(timeout.error(), "Temporisation Ivy");
        std::cout << "Récepteur prêt sur Ivy (" << domain
                  << ", attente de 10 secondes)" << std::endl;
        // Traite messages et timer sur ce thread, jusqu'à link.close().
        // subscription et timeout restent vivants pendant tout run().
        link.run();
        if (!received) std::cerr << "Aucune altitude reçue\n";
        return received ? 0 : 1;
    }

    int emit(const pprzlink::MessageDictionary &dictionary, const std::string &domain)
    {
        // Remplit une altitude en mètres SI ; le XML choisit l'unité et le type transmis.
        pprzlink::Message altitude(dictionary.getDefinition("GUIDE_ALTITUDE"));
        altitude.setSenderId(42);
        altitude.setFieldSI("altitude", 123.5);
        // true démarre un thread Ivy pendant que le thread principal attend la découverte.
        // Le destructeur du lien arrêtera et rejoindra ce thread.
        pprzlink::IvyLink link(dictionary, "guide-emetteur", domain, true);
        std::cout << "Émetteur prêt sur Ivy (" << domain << ')' << std::endl;
        waitForReceiver(link);
        // Publie après l'annonce de l'abonnement : Ivy ne conserve pas les messages pour plus tard.
        link.sendMessage(altitude);
        std::cout << "Altitude publiée sur Ivy." << std::endl;
        // Remonte sur ce thread les erreurs éventuelles des callbacks du thread Ivy.
        const auto status = link.getBus().take_callback_error();
        if (!status) throw std::system_error(status.error(), "Callback Ivy");
        return 0;
    }
}

int main(int argc, char **argv)
{
    // Le rôle suit le XML ; un domaine facultatif permet de choisir un autre bus Ivy.
    if (argc < 3 || argc > 4 ||
        (std::string_view(argv[2]) != "e" && std::string_view(argv[2]) != "r")) {
        std::cerr << "Usage: " << argv[0] << " messages.xml e|r [domaine-Ivy]\n"
                  << "  e : publier une altitude\n"
                  << "  r : recevoir une altitude\n";
        return 2;
    }
    try {
        // Le catalogue reste vivant jusqu'au retour du rôle choisi et à la destruction de son lien.
        const pprzlink::MessageDictionary dictionary(argv[1]);
        const std::string domain = argc == 4 ? argv[3] : "127.255.255.255:2010";
        if (std::string_view(argv[2]) == "e") return emit(dictionary, domain);
        return receive(dictionary, domain);
    } catch (const std::exception &error) {
        // Les erreurs XML, de découverte ou de callbacks donnent un code de sortie non nul.
        std::cerr << error.what() << '\n';
        return 1;
    }
}
