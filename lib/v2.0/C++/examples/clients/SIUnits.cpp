// SPDX-License-Identifier: LGPL-3.0-or-later
// Exemple sans I/O : conversions SI, métadonnées XML et encodage binaire natif.
#include <pprzlink/MessageDictionary.h>
#include <pprzlink/Message.h>
#include <pprzlink/PprzFrameCodec.h>
#include <array>
#include <iostream>
#include <stdexcept>

int main()
{
  try {
    // Le XML est intégré pour rendre cet exemple autonome, sans fichier à installer.
    // L'altitude est stockée en mm, la température en degrés Celsius et la position en cm.
    tinyxml2::XMLDocument xml;
    if (xml.Parse(R"xml(<protocol><msg_class name="example" id="1">
      <message name="SI_EXAMPLE" id="1">
        <field name="altitude" type="int32" unit="mm" alt_unit="m"/>
        <field name="temperature" type="float" unit="deg_celsius"/>
        <field name="position" type="int16[3]" unit="cm"/>
      </message>
    </msg_class></protocol>)xml") != tinyxml2::XML_SUCCESS) {
      throw std::runtime_error("Invalid example XML");
    }
    // Le chargement valide les types et prépare les conversions d'unités de chaque champ.
    const pprzlink::MessageDictionary dictionary(xml.RootElement());
    // Le message possède ses valeurs ; sa définition décrit leur ordre et leur stockage.
    pprzlink::Message message(dictionary.getDefinition("SI_EXAMPLE"));
    message.setFieldSI("altitude", 1.23456); // Mètres SI, arrondis à l'entier XML le plus proche : 1235 mm.
    message.setFieldSI("temperature", 293.15); // Kelvins SI, convertis vers le float XML : 20 °C.
    // L'array fournit une vue temporaire ; le message copie les int16 convertis : 100, -200, 300 cm.
    message.setFieldSI("position", std::array<double, 3>{1.0, -2.0, 3.0});

    // Emprunte les métadonnées du message pour comparer l'unité XML à l'unité SI.
    const auto &field = message.getFieldDefinition("altitude");
    // getField<T>() lit le type natif exact ; getFieldSI() restitue la grandeur SI en double.
    std::cout << field.getUnit() << " -> " << field.getSIUnit() << '\n'
              << message.getField<int32_t>("altitude") << " mm = "
              << message.getFieldSI("altitude") << " m\n"
              << message.getField<float>("temperature") << " Celsius = "
              << message.getFieldSI("temperature") << " K\n";

    // Le framing encode les valeurs natives du XML ; les unités restent dans le catalogue.
    pprzlink::PprzFrameDecoder decoder(dictionary);
    // Injecte une trame complète en mémoire, comme si les octets venaient d'un périphérique.
    decoder.pushBytes(pprzlink::encodePprzFrame(message));
    // Le résultat est facultatif : il n'y aurait aucun message sans trame complète valide.
    const auto received = decoder.nextMessage();
    if (!received || received->getField<int32_t>("altitude") != 1235) {
      throw std::runtime_error("Native XML storage was not preserved");
    }
    // Crée un vecteur<double> possédé, indépendant du message, avec les trois valeurs en mètres.
    const auto position = received->getFieldArraySI("position");
    std::cout << "Decoded position:";
    for (double metres : position) std::cout << ' ' << metres;
    std::cout << " m\n";
  } catch (const std::exception &error) {
    // Les erreurs XML, de conversion ou de framing font échouer l'exemple explicitement.
    std::cerr << error.what() << '\n';
    return 1;
  }
}
