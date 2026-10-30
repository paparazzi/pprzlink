# Chaque exemple sélectionne seulement le composant nécessaire : core, io ou ivy.
function(add_pprzlink_example target source component)
    add_executable(${target} ${source})
    # La cible transmet les en-têtes, les dépendances et le standard C++23.
    target_link_libraries(${target} PRIVATE pprzlink::${component})
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    # Place les exécutables à la racine de la construction, utilisée par les commandes du guide.
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()

# Les exemples série/UDP utilisent io ; l'exemple SI fonctionne avec le seul cœur.
add_pprzlink_example(serial_messages SerialMessages.cpp io)
add_pprzlink_example(si_units clients/SIUnits.cpp core)
add_pprzlink_example(serial_aircraft clients/SerialAircraft.cpp io)
add_pprzlink_example(pty_peer clients/PtyPeer.cpp io)
add_pprzlink_example(pty_requester clients/PtyRequester.cpp io)
add_pprzlink_example(pty_responder clients/PtyResponder.cpp io)
add_pprzlink_example(pty_agent clients/PtyAgent.cpp io)
add_pprzlink_example(workshop_udp clients/WorkshopUdp.cpp io)
add_pprzlink_example(udp_messages clients/UdpMessages.cpp io)
add_pprzlink_example(udp_recorder clients/UdpRecorder.cpp io)
add_pprzlink_example(udp_setting_sender clients/UdpSettingSender.cpp io)
if(PPRZLINK_WITH_IVY)
    # Ces programmes sont ajoutés uniquement lorsque le composant Ivy est disponible.
    add_pprzlink_example(ivy_round_trip IvyRoundTrip.cpp ivy)
    add_pprzlink_example(ivy_receiver clients/IvyReceiver.cpp ivy)
    add_pprzlink_example(workshop_ivy clients/WorkshopIvy.cpp ivy)
endif()
