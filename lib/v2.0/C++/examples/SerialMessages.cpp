// Diagnostic série : lit les messages et peut en envoyer un, avec framing PPRZ ou XBee.
#include <pprzlink/BoostSerialPortDevice.h>
#include <pprzlink/IvyMessageCodec.h>
#include <pprzlink/PprzTransport.h>
#include <pprzlink/XbeeTransport.h>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <charconv>
#include <chrono>
#include <concepts>
#include <csignal>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <optional>
#include <string_view>
#include <type_traits>

// Abrège les noms de la bibliothèque et permet les durées chrono comme 2000ms.
using namespace pprzlink;
using namespace std::chrono_literals;

namespace {
  // Le texte d'aide décrit les options ; aucune connexion Ivy n'est nécessaire pour ce programme.
  constexpr auto usage = R"(Usage: serial_messages -messages FILE -d PORT [options]
  -transport pprz|xbee   Serial framing (default: pprz)
  -s BAUD               Target baud rate (default: 57600), 8 data bits, no parity, 1 stop bit
  -hfc                  Enable paired host/XBee RTS/CTS (PPRZ: host only; default: none)
  -send "NAME fields"    Send one message, with XML fields in Ivy text order
  -sender ID            PPRZLINK sender, 0..255 (default: 0)
  -receiver ID          PPRZLINK receiver, 0..255 (default: 255 = broadcast)
  -xbeesan              Validate complete XBee frames before sending; errors go to stderr
  -xbee-dest ADDRESS    Override 16-bit radio destination (decimal or 0x...)
  -xbee-dest64 ADDRESS  Override 64-bit radio destination (decimal or 0x...)
  -xbee_addr ADDRESS   Local XBee MY address (default: 0x100)
  -ch CHANNEL         Set XBee channel, 0x0c..0x17 (default: unchanged)
  -xbee-no-init        Skip AT initialization for an already configured modem
  -xbee-no-autobaud    Use -s directly without detecting/changing the modem baud rate
  -xbee-guard-ms MS    Silence before/after +++ (default: 2000)
  -xbee-timeout-ms MS  Deadline for each AT reply (default: 2000)
  -duration SECONDS     Stop after this duration (default: 0 = until Ctrl-C)
  -h, --help            Show this help

Receive messages and radio statuses on stdout. No Ivy bus is started.
The xbee mode automatically detects the modem baud rate and changes it to -s.
A changed baud rate is verified and saved with ATWR before configuring MY/CH/AP.
ATWR also saves the modem's other existing settings. MY/CH/AP/D6/D7 changes made by
this program remain volatile. Legacy 802.15.4 RX16/RX64 API output is required.
Standard rates from 1200 to 115200 are probed, starting with the target rate.
Failed transmissions are not retried by this example. Payloads are limited to 100 bytes including
the PPRZLINK v2 header. XML defines messages, not the transport mode.
)";

  struct Options {
    // Les chaînes possèdent leurs caractères ; elles ne conservent aucune vue sur argv.
    std::string messages, device, transport = "pprz", send;
    unsigned int baud = 57600, duration = 0;
    // Identifiants de l'en-tête PPRZLINK, indépendants des adresses radio ci-dessous.
    uint8_t sender = 0, receiver = 255;
    // Un optional vide conserve l'adressage radio habituel déduit du destinataire PPRZLINK.
    std::optional<uint16_t> destination16;
    std::optional<uint64_t> destination64;
    // Configuration du dialogue AT : adresse locale, canal, débit cible et délais.
    XbeeConfiguration xbeeConfiguration;
    // Ces indicateurs distinguent une configuration explicite du choix de sauter l'initialisation.
    bool skipXbeeInitialization = false, hasXbeeConfiguration = false;
    bool autoBaud = true;
    bool hardwareFlowControl = false, help = false, xbeeSanity = false;
  };

  template<class T>
  T number(std::string_view text, std::string_view option)
  {
    // Accepte le décimal et le préfixe 0x ; la vue est locale et ne modifie pas le texte d'origine.
    int base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) { text.remove_prefix(2); base = 16; }
    uint64_t value = 0;
    // Lit d'abord un uint64_t, puis vérifie la plage du type demandé avant le cast.
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    // La consommation complète refuse aussi les suffixes après le nombre et les préfixes sans chiffres.
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() ||
        value > std::numeric_limits<T>::max()) {
      throw std::invalid_argument("Invalid value for " + std::string(option));
    }
    return static_cast<T>(value);
  }

  Options parseOptions(int argc, char **argv)
  {
    // Construit et valide toutes les options avant d'ouvrir le port série.
    Options options;
    for (int i = 1; i < argc; ++i) {
      const std::string_view option(argv[i]);
      // Ces drapeaux n'ont pas de valeur ; continue évite de consommer l'option suivante.
      if (option == "-h" || option == "--help") { options.help = true; return options; }
      if (option == "-hfc") { options.hardwareFlowControl = true; continue; }
      if (option == "-xbeesan") { options.xbeeSanity = true; continue; }
      if (option == "-xbee-no-init") { options.skipXbeeInitialization = true; continue; }
      if (option == "-xbee-no-autobaud") {
        options.autoBaud = false;
        options.hasXbeeConfiguration = true;
        continue;
      }
      // Les autres options consomment un argument supplémentaire, dont l'existence est vérifiée.
      if (i + 1 == argc) throw std::invalid_argument("Missing value for " + std::string(option));
      const std::string_view value(argv[++i]);
      if (option == "-messages") options.messages = value;
      else if (option == "-d") options.device = value;
      else if (option == "-transport") options.transport = value;
      else if (option == "-s") options.baud = number<unsigned int>(value, option);
      else if (option == "-send") options.send = value;
      else if (option == "-sender") options.sender = number<uint8_t>(value, option);
      else if (option == "-receiver") options.receiver = number<uint8_t>(value, option);
      else if (option == "-xbee-dest") options.destination16 = number<uint16_t>(value, option);
      else if (option == "-xbee-dest64") options.destination64 = number<uint64_t>(value, option);
      else if (option == "-xbee_addr") {
        options.xbeeConfiguration.localAddress = number<uint16_t>(value, option);
        options.hasXbeeConfiguration = true;
      } else if (option == "-ch") {
        options.xbeeConfiguration.channel = number<uint8_t>(value, option);
        options.hasXbeeConfiguration = true;
      } else if (option == "-xbee-guard-ms") {
        options.xbeeConfiguration.guardTime = std::chrono::milliseconds(number<unsigned int>(value, option));
        options.hasXbeeConfiguration = true;
      } else if (option == "-xbee-timeout-ms") {
        options.xbeeConfiguration.responseTimeout = std::chrono::milliseconds(number<unsigned int>(value, option));
        options.hasXbeeConfiguration = true;
      } else if (option == "-duration") options.duration = number<unsigned int>(value, option);
      else throw std::invalid_argument("Unknown option: " + std::string(option));
    }
    // Le catalogue XML définit les messages ; le framing est sélectionné séparément par -transport.
    if (options.transport != "pprz" && options.transport != "xbee") {
      throw std::invalid_argument("-transport must be pprz or xbee");
    }
    if (options.messages.empty() || options.device.empty() || options.baud == 0) {
      throw std::invalid_argument("Supply -messages FILE, -d PORT and a positive baud rate");
    }
    // Refuse les options radio avec PPRZ et les choix d'adressage ou d'initialisation contradictoires.
    if ((options.destination16 || options.destination64 || options.skipXbeeInitialization ||
         options.hasXbeeConfiguration || options.xbeeSanity) && options.transport != "xbee") {
      throw std::invalid_argument("XBee options require -transport xbee");
    }
    if (options.destination16 && options.destination64) {
      throw std::invalid_argument("Choose either -xbee-dest or -xbee-dest64");
    }
    if (options.skipXbeeInitialization && options.hasXbeeConfiguration) {
      throw std::invalid_argument("XBee configuration options conflict with -xbee-no-init");
    }
    if (options.transport == "xbee" && options.skipXbeeInitialization && options.hardwareFlowControl)
      throw std::invalid_argument("-hfc requires XBee initialization to configure both UART and modem");
    options.xbeeConfiguration.hardwareFlowControl = options.hardwareFlowControl;
    // nullopt conserve le débit du port ; un débit cible autorise sa détection et son changement par AT.
    options.xbeeConfiguration.targetBaudrate =
        options.autoBaud && !options.skipXbeeInitialization
        ? std::optional(options.baud) : std::nullopt;
    // Valide le débit cible, les délais et le canal sans encore dialoguer avec le modem.
    if (options.transport == "xbee") (void)XbeeModem(options.xbeeConfiguration);
    return options;
  }

  void printStatus(const XbeeTransport::RadioStatus &status)
  {
    // Le variant contient un statut de transmission, de modem ou de réponse AT.
    // visit choisit le type présent ; if constexpr ne compile que la branche qui lui correspond.
    std::visit([](const auto &event) {
      using T = std::decay_t<decltype(event)>;
      // Les conversions en unsigned affichent les octets comme des nombres plutôt que des caractères.
      if constexpr (std::same_as<T, XbeeTransport::TransmitStatus>) {
        std::cout << "XBee TX status: frame=" << unsigned(event.frameId)
                  << " status=" << unsigned(event.status) << std::endl;
      } else if constexpr (std::same_as<T, XbeeTransport::ModemStatus>) {
        std::cout << "XBee modem status: " << unsigned(event.status) << std::endl;
      } else {
        std::cout << "XBee AT response: " << event.command[0] << event.command[1]
                  << " frame=" << unsigned(event.frameId) << " status=" << unsigned(event.status) << std::endl;
      }
    }, status);
  }

  void run(const Options &options)
  {
    // Le dictionnaire doit survivre au transport et aux callbacks qui empruntent ses définitions.
    MessageDictionary dictionary(options.messages);
    // Prépare le message avant les I/O ; l'optional reste vide si aucun -send n'a été fourni.
    std::optional<Message> outgoing;
    if (!options.send.empty()) {
      const auto name = options.send.substr(0, options.send.find(' '));
      // Le codec de texte Ivy remplit les champs dans l'ordre XML, sans démarrer de bus Ivy.
      outgoing = ivy_codec::parseMessageBody(dictionary.getDefinition(name),
                                             std::to_string(options.sender), options.send);
      // Le format texte fournit l'expéditeur ; le destinataire binaire est renseigné séparément.
      outgoing->setReceiverId(options.receiver);
    }

    // Déclaré avant le port et le transport, le contexte reste vivant jusqu'à leur destruction.
    boost::asio::io_context context;
    auto device = std::make_unique<BoostSerialPortDevice>(context, options.device);
    // Configure les deux côtés en 8N1 ; le contrôle RTS/CTS est activé seulement avec -hfc.
    device->setBaudrate(BoostSerialPortDevice::Baudrate(options.baud));
    device->setDataBits(BoostSerialPortDevice::DataBits(8));
    device->setParity(BoostSerialPortDevice::Parity(BoostSerialPortDevice::Parity::none));
    device->setStopBits(BoostSerialPortDevice::StopBits(BoostSerialPortDevice::StopBits::one));
    // En XBee, le dialogue AT activera les deux côtés avant de notifier onReady.
    device->setFlowcontrol(BoostSerialPortDevice::Flowcontrol(options.transport == "pprz" && options.hardwareFlowControl
        ? BoostSerialPortDevice::Flowcontrol::hardware : BoostSerialPortDevice::Flowcontrol::none));

    // Le pointeur de base possède le transport choisi ; celui-ci possédera le port série.
    std::unique_ptr<Transport> transport;
    // Ce pointeur emprunté donne accès aux fonctions radio et reste valide tant que transport vit.
    XbeeTransport *xbee = nullptr;
    if (options.transport == "xbee") {
      auto radio = std::make_unique<XbeeTransport>(std::move(device), dictionary);
      // Installe l'observateur radio avant toute émission, pour recevoir aussi les statuts de livraison.
      radio->setStatusCallback(printStatus);
      radio->setSanityChecksEnabled(options.xbeeSanity);
      // Déplacer le unique_ptr transfère la propriété sans déplacer l'objet pointé.
      xbee = radio.get();
      transport = std::move(radio);
    } else {
      transport = std::make_unique<PprzTransport>(std::move(device), dictionary);
    }

    // Ctrl+C/SIGTERM et l'échéance facultative donnent deux chemins d'arrêt propre.
    boost::asio::signal_set signals(context, SIGINT, SIGTERM);
    boost::asio::steady_timer duration(context);
    // Annule toutes les attentes ; une attente de signal oubliée maintiendrait run() actif.
    auto stop = [&] { transport->stop(); signals.cancel(); duration.cancel(); };
    signals.async_wait([&](const boost::system::error_code &error, int) { if (!error) stop(); });
    // Le récepteur conserve le binding ; le message est emprunté uniquement pendant son affichage.
    transport->bind(ALL, [](const Message &message) {
      std::cout << ivy_codec::serializeMessage(message) << std::endl;
    });
    // Une trame malformée est signalée puis ignorée ; une panne d'I/O arrête l'exécution par exception.
    transport->onError([](const ReceiveError &error) {
      if (error.kind == ReceiveError::Kind::Decode) std::cerr << "Discarded message: " << error.message << '\n';
      else std::rethrow_exception(error.exception);
    });
    // Cette fonction est appelée après l'initialisation radio, ou directement pour PPRZ.
    // Les objets capturés par référence restent vivants pendant tout context.run().
    auto listen = [&] {
      if (xbee && !options.skipXbeeInitialization) {
        // L'optional n'est renseigné que si l'initialisation a produit un résultat de débit.
        if (const auto baud = xbee->getBaudrateInfo()) {
          std::cout << "XBee baud rate: detected " << baud->detected
                    << ", configured " << baud->configured
                    << (baud->savedToFlash ? " (saved to flash)" : " (unchanged)") << std::endl;
        }
        std::cout << "XBee modem ready (AP=1)" << std::endl;
      }
      // Envoie une seule fois le message préparé ; les adresses explicites sont celles de la radio.
      if (outgoing) {
        size_t written;
        if (options.destination16) written = xbee->sendMessageTo16(*outgoing, *options.destination16);
        else if (options.destination64) written = xbee->sendMessageTo64(*outgoing, *options.destination64);
        else written = transport->sendMessage(*outgoing);
        // Ce nombre confirme l'écriture série ; le statut radio arrivera séparément par callback.
        std::cout << "Wrote " << written << " serial bytes" << std::endl;
      }
      std::cout << "Listening with " << options.transport << " transport" << std::endl;
      // La durée d'écoute commence ici, lorsque le canal est prêt, après l'éventuel dialogue AT.
      if (options.duration) {
        duration.expires_after(std::chrono::seconds(options.duration));
        // Ignore operation_aborted si Ctrl+C a annulé ce timer avant son expiration.
        duration.async_wait([&](const boost::system::error_code &error) { if (!error) stop(); });
      }
    };
    if (xbee) {
      // Installe le callback avant start() ; l'envoi attendra que le modem soit prêt.
      xbee->onReady(listen);
      if (!options.skipXbeeInitialization) {
        // Prépare le dialogue AT ; les lectures et échéances Asio le feront progresser.
        xbee->startInitialization(options.xbeeConfiguration);
        std::cout << "Initializing XBee modem" << std::endl;
      }
    }
    // Arme la lecture avant l'envoi PPRZ ; XBee déclenchera listen via son callback onReady.
    transport->start();
    if (!xbee) listen();
    // Traite lectures, statuts, signaux et timers sur ce thread, jusqu'à l'arrêt complet.
    context.run();
  }
}

int main(int argc, char **argv)
{
  try {
    // L'aide termine sans ouvrir le port ; les options valides sont ensuite confiées à run().
    const auto options = parseOptions(argc, argv);
    if (options.help) { std::cout << usage; return 0; }
    run(options);
  } catch (const std::exception &error) {
    // Une erreur de saisie, de catalogue, d'initialisation radio ou d'I/O donne un code d'échec.
    std::cerr << error.what() << '\n';
    return 1;
  }
}
