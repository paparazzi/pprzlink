cmake_minimum_required(VERSION 3.20)
project(pprzlink_clients LANGUAGES CXX)

# Ce répertoire peut être copié hors du dépôt et compilé contre un SDK installé.
option(CLIENTS_WITH_IVY "Build the Ivy examples as well as serial/UDP clients" ON)
# io inclut le cœur, la série et UDP ; Ivy est une dépendance facultative distincte.
set(components io)
if(CLIENTS_WITH_IVY)
    list(APPEND components ivy)
endif()
# CMAKE_PREFIX_PATH indique le préfixe du SDK ; ses cibles fourniront aussi le standard C++23.
find_package(pprzlink++ CONFIG REQUIRED COMPONENTS ${components})

# Chaque exécutable lie uniquement le composant dont il utilise l'API publique.
add_executable(si_units SIUnits.cpp)
target_link_libraries(si_units PRIVATE pprzlink::core)
add_executable(serial_aircraft SerialAircraft.cpp)
target_link_libraries(serial_aircraft PRIVATE pprzlink::io)
add_executable(pty_peer PtyPeer.cpp)
target_link_libraries(pty_peer PRIVATE pprzlink::io)
add_executable(pty_requester PtyRequester.cpp)
target_link_libraries(pty_requester PRIVATE pprzlink::io)
add_executable(pty_responder PtyResponder.cpp)
target_link_libraries(pty_responder PRIVATE pprzlink::io)
add_executable(pty_agent PtyAgent.cpp)
target_link_libraries(pty_agent PRIVATE pprzlink::io)
add_executable(workshop_udp WorkshopUdp.cpp)
target_link_libraries(workshop_udp PRIVATE pprzlink::io)
add_executable(udp_messages UdpMessages.cpp)
target_link_libraries(udp_messages PRIVATE pprzlink::io)
add_executable(udp_recorder UdpRecorder.cpp)
target_link_libraries(udp_recorder PRIVATE pprzlink::io)
add_executable(udp_setting_sender UdpSettingSender.cpp)
target_link_libraries(udp_setting_sender PRIVATE pprzlink::io)
if(CLIENTS_WITH_IVY)
    # Avec CLIENTS_WITH_IVY=OFF, aucun en-tête ni bibliothèque Ivy n'est demandé.
    add_executable(ivy_receiver IvyReceiver.cpp)
    target_link_libraries(ivy_receiver PRIVATE pprzlink::ivy)
endif()
