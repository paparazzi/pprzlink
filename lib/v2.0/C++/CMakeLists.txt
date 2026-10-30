cmake_minimum_required(VERSION 3.20)
project(pprzlink++ LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(BUILD_TESTING "Build the tests" OFF)
include(CTest)
option(PPRZLINK_WITH_IVY "Build the optional Ivy adapter" ON)
option(PPRZLINK_BUILD_EXAMPLES "Build the public API examples" OFF)
option(PPRZLINK_BUILD_DOCS "Add the Doxygen documentation target" OFF)

if(PPRZLINK_BUILD_DOCS)
    find_package(Doxygen 1.9.8 REQUIRED)
    set(DOXYGEN_PROJECT_NAME "pprzlink++")
    set(DOXYGEN_PROJECT_BRIEF "Runtime XML message definitions, codecs and transports")
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_FILE_PATTERNS "*.h" "*.cpp" "*.dox")
    set(DOXYGEN_EXCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    set(DOXYGEN_EXTRACT_ALL NO)
    set(DOXYGEN_EXTRACT_PRIVATE NO)
    set(DOXYGEN_EXTRACT_STATIC NO)
    set(DOXYGEN_EXTRACT_ANON_NSPACES YES)
    set(DOXYGEN_SOURCE_BROWSER YES)
    set(DOXYGEN_INLINE_SOURCES YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_GENERATE_XML YES)
    set(DOXYGEN_QUIET YES)
    set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
    set(DOXYGEN_WARN_IF_DOC_ERROR YES)
    set(DOXYGEN_WARN_IF_INCOMPLETE_DOC YES)
    set(DOXYGEN_WARN_NO_PARAMDOC YES)
    set(DOXYGEN_WARN_IF_UNDOC_ENUM_VAL YES)
    set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
    set(DOXYGEN_WARN_LOGFILE "${CMAKE_CURRENT_BINARY_DIR}/docs/warnings.log")
    set(DOXYGEN_MACRO_EXPANSION YES)
    set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
    set(DOXYGEN_EXPAND_AS_DEFINED DECLARE_PPRZLINK_EXCEPT)
    doxygen_add_docs(pprzlink_docs pprzlink docs
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generate the pprzlink++ API and source documentation")
endif()

find_package(tinyxml2 REQUIRED)
find_package(Boost 1.83 CONFIG REQUIRED)
find_package(Threads REQUIRED)
if(PPRZLINK_WITH_IVY)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(IVY REQUIRED IMPORTED_TARGET ivy-cpp>=3.18)
endif()

set(CORE_SOURCES
    pprzlink/FieldValue.cpp pprzlink/BinaryCodec.cpp pprzlink/TextCodec.cpp
    pprzlink/IvyMessageCodec.cpp pprzlink/LegacyIvyCodec.cpp
    pprzlink/Message.cpp pprzlink/MessageDefinition.cpp pprzlink/MessageDictionary.cpp
    pprzlink/MessageField.cpp pprzlink/MessageFieldTypes.cpp
    pprzlink/UnitConversion.cpp pprzlink/UnitAliases.cpp
    pprzlink/Receiver.cpp pprzlink/MemoryDevice.cpp pprzlink/PprzTransport.cpp pprzlink/PprzFrameCodec.cpp
    pprzlink/XbeeTransport.cpp pprzlink/XbeeModem.cpp)
set(IO_SOURCES pprzlink/BoostSerialPortDevice.cpp pprzlink/UdpTransport.cpp)
set(PUBLIC_HEADERS
    pprzlink/BytesBuffer.h pprzlink/Device.h pprzlink/MemoryDevice.h pprzlink/FieldValue.h pprzlink/BinaryCodec.h pprzlink/TextCodec.h
    pprzlink/IvyMessageCodec.h pprzlink/Message.h pprzlink/MessageDefinition.h
    pprzlink/MessageDictionary.h pprzlink/MessageField.h pprzlink/MessageFieldTypes.h pprzlink/RadioRetries.h
    pprzlink/PprzTransport.h pprzlink/PprzFrameCodec.h pprzlink/TransportStatistics.h
    pprzlink/XbeeAddress.h pprzlink/XbeeTransport.h pprzlink/XbeeModem.h pprzlink/Transport.h
    pprzlink/ReceivedMessage.h pprzlink/Receiver.h pprzlink/BoostSerialPortDevice.h pprzlink/UdpTransport.h)
if(UNIX)
    list(APPEND IO_SOURCES pprzlink/PosixFileDevice.cpp)
    list(APPEND PUBLIC_HEADERS pprzlink/PosixFileDevice.h)
endif()

function(configure_pprzlink_target target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
endfunction()

# Compile each implementation once. Public component archives and the historical
# shared/static libraries reuse those objects, without exporting object targets.
add_library(pprzlink_core_objects OBJECT ${CORE_SOURCES})
configure_pprzlink_target(pprzlink_core_objects)
target_link_libraries(pprzlink_core_objects PRIVATE tinyxml2::tinyxml2 Boost::headers)
add_library(pprzlink_core STATIC $<TARGET_OBJECTS:pprzlink_core_objects>)
configure_pprzlink_target(pprzlink_core)
target_link_libraries(pprzlink_core PUBLIC tinyxml2::tinyxml2 Boost::headers Threads::Threads)
add_library(pprzlink::core ALIAS pprzlink_core)

add_library(pprzlink_io_objects OBJECT ${IO_SOURCES})
configure_pprzlink_target(pprzlink_io_objects)
target_link_libraries(pprzlink_io_objects PRIVATE pprzlink_core Threads::Threads)
add_library(pprzlink_io STATIC $<TARGET_OBJECTS:pprzlink_io_objects>)
configure_pprzlink_target(pprzlink_io)
target_link_libraries(pprzlink_io PUBLIC pprzlink_core Threads::Threads)
add_library(pprzlink::io ALIAS pprzlink_io)

set(LEGACY_OBJECTS $<TARGET_OBJECTS:pprzlink_core_objects> $<TARGET_OBJECTS:pprzlink_io_objects>)
set(LEGACY_LIBRARIES tinyxml2::tinyxml2 Boost::headers Threads::Threads)
if(PPRZLINK_WITH_IVY)
    add_library(pprzlink_ivy_objects OBJECT pprzlink/IvyLink.cpp)
    configure_pprzlink_target(pprzlink_ivy_objects)
    target_link_libraries(pprzlink_ivy_objects PRIVATE pprzlink_core PkgConfig::IVY)
    add_library(pprzlink_ivy STATIC $<TARGET_OBJECTS:pprzlink_ivy_objects>)
    configure_pprzlink_target(pprzlink_ivy)
    target_link_libraries(pprzlink_ivy PUBLIC pprzlink_core PkgConfig::IVY Threads::Threads)
    add_library(pprzlink::ivy ALIAS pprzlink_ivy)
    list(APPEND LEGACY_OBJECTS $<TARGET_OBJECTS:pprzlink_ivy_objects>)
    list(APPEND LEGACY_LIBRARIES PkgConfig::IVY)
    list(APPEND PUBLIC_HEADERS pprzlink/IvyLink.h)
endif()

add_library(pprzlink++ SHARED ${LEGACY_OBJECTS})
add_library(pprzlink++_static STATIC ${LEGACY_OBJECTS})
foreach(target pprzlink++ pprzlink++_static)
    configure_pprzlink_target(${target})
    target_link_libraries(${target} PUBLIC ${LEGACY_LIBRARIES})
endforeach()

set(PPRZLINK_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/pprzlink++/cmake")
function(export_pprzlink_component component)
    set(target pprzlink_${component})
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${component})
    install(TARGETS ${target} EXPORT ${target}Targets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(EXPORT ${target}Targets NAMESPACE pprzlink:: DESTINATION ${PPRZLINK_CMAKE_INSTALL_DIR})
    export(EXPORT ${target}Targets NAMESPACE pprzlink:: FILE "${CMAKE_CURRENT_BINARY_DIR}/${target}Targets.cmake")
endfunction()
export_pprzlink_component(core)
export_pprzlink_component(io)
if(PPRZLINK_WITH_IVY)
    export_pprzlink_component(ivy)
endif()
install(TARGETS pprzlink++ pprzlink++_static EXPORT pprzlink++Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(EXPORT pprzlink++Targets DESTINATION ${PPRZLINK_CMAKE_INSTALL_DIR})
export(EXPORT pprzlink++Targets FILE "${CMAKE_CURRENT_BINARY_DIR}/pprzlink++Targets.cmake")
install(FILES ${PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pprzlink)
install(FILES pprzlink/exceptions/pprzlink_exception.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pprzlink/exceptions)
install(FILES pprzlink/detail/CheckedNumber.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pprzlink/detail)
configure_package_config_file(cmake/pprzlink++Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/pprzlink++Config.cmake" INSTALL_DESTINATION ${PPRZLINK_CMAKE_INSTALL_DIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pprzlink++Config.cmake" DESTINATION ${PPRZLINK_CMAKE_INSTALL_DIR})

if(PPRZLINK_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(BUILD_TESTING)
    if(PPRZLINK_BUILD_EXAMPLES)
        add_test(NAME si_units_example COMMAND si_units)
        set_tests_properties(si_units_example PROPERTIES TIMEOUT 15)
    endif()
    if(PPRZLINK_WITH_IVY AND UNIX)
        add_executable(link_test_bus tests/LinkTestBus.cpp)
        target_link_libraries(link_test_bus PRIVATE pprzlink++)
        target_compile_options(link_test_bus PRIVATE -Wall -Wextra -Werror)
    endif()
    function(add_pprzlink_test name source)
        add_executable(${name}_test tests/${source})
        target_link_libraries(${name}_test PRIVATE pprzlink::core)
        target_compile_options(${name}_test PRIVATE -Wall -Wextra -Werror)
        add_test(NAME ${name} COMMAND ${name}_test ${ARGN})
        set_tests_properties(${name} PROPERTIES TIMEOUT 30)
    endfunction()

    if(PPRZLINK_BUILD_EXAMPLES AND PPRZLINK_WITH_IVY)
        add_test(NAME ivy_round_trip COMMAND ivy_round_trip
                 "${CMAKE_CURRENT_SOURCE_DIR}/examples/messages.xml")
        set_tests_properties(ivy_round_trip PROPERTIES TIMEOUT 15)
    endif()

    add_pprzlink_test(serialization SerializationTest.cpp)
    add_pprzlink_test(field_value FieldValueTest.cpp)
    add_pprzlink_test(field_conversion FieldConversionTest.cpp)
    add_pprzlink_test(unit_conversion UnitConversionTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../message_definitions/v1.0/messages.xml")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME field_arguments_compile COMMAND ${Python3_EXECUTABLE}
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/FieldArgumentsCompileTest.py"
                 --compiler "${CMAKE_CXX_COMPILER}"
                 --include-dirs "$<TARGET_PROPERTY:field_conversion_test,INCLUDE_DIRECTORIES>")
        set_tests_properties(field_arguments_compile PROPERTIES TIMEOUT 60)
    endif()
    add_pprzlink_test(memory_device MemoryDeviceTest.cpp)
    add_pprzlink_test(receive ReceiveTest.cpp)
    add_pprzlink_test(receiver ReceiverTest.cpp)
    target_link_libraries(receiver_test PRIVATE pprzlink::io)
    add_pprzlink_test(udp_transport UdpTransportTest.cpp)
    target_link_libraries(udp_transport_test PRIVATE pprzlink::io)
    if(PPRZLINK_WITH_IVY)
        add_pprzlink_test(ivy_link IvyLinkTest.cpp)
        target_link_libraries(ivy_link_test PRIVATE pprzlink::ivy)
    endif()
    add_pprzlink_test(transport TransportTest.cpp)
    add_pprzlink_test(xbee_transport XbeeTransportTest.cpp)
    add_pprzlink_test(xbee_modem XbeeModemTest.cpp)
    add_pprzlink_test(xbee_baudrate XbeeBaudrateTest.cpp)
    add_pprzlink_test(dictionary DictionaryTest.cpp)
    add_pprzlink_test(ivy_message_codec IvyMessageCodecTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../message_definitions/v1.0/messages.xml")
    add_pprzlink_test(link_library LinkLibraryTest.cpp)
    if(UNIX)
        add_pprzlink_test(serial_device SerialDeviceTest.cpp)
        target_link_libraries(serial_device_test PRIVATE pprzlink::io)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_pprzlink_test(cts_status CtsStatusTest.cpp)
            target_link_libraries(cts_status_test PRIVATE pprzlink::io)
            target_link_options(cts_status_test PRIVATE "-Wl,--wrap=ioctl")
        endif()
        if(PPRZLINK_BUILD_EXAMPLES)
            find_package(Python3 REQUIRED COMPONENTS Interpreter)
            add_test(NAME pty_peers COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/PtyPeersTest.py"
                     --peer $<TARGET_FILE:pty_peer>)
            set_tests_properties(pty_peers PROPERTIES TIMEOUT 20)
            add_test(NAME udp_messages COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/UdpMessagesTest.py"
                     $<TARGET_FILE:udp_messages>)
            set_tests_properties(udp_messages PROPERTIES TIMEOUT 15)
            add_test(NAME pty_agents COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/PtyAgentsTest.py"
                     --agent $<TARGET_FILE:pty_agent>)
            set_tests_properties(pty_agents PROPERTIES TIMEOUT 30)
            add_test(NAME serial_messages COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/SerialMessagesTest.py"
                     $<TARGET_FILE:serial_messages>)
            set_tests_properties(serial_messages PROPERTIES TIMEOUT 30)
            set(client_ivy_args)
            if(PPRZLINK_WITH_IVY)
                list(APPEND client_ivy_args --ivy $<TARGET_FILE:ivy_receiver> --bus $<TARGET_FILE:link_test_bus>)
                add_test(NAME workshop_ivy COMMAND ${Python3_EXECUTABLE}
                         "${CMAKE_CURRENT_SOURCE_DIR}/tests/WorkshopIvyTest.py"
                         $<TARGET_FILE:workshop_ivy>)
                set_tests_properties(workshop_ivy PROPERTIES TIMEOUT 30)
            endif()
            add_test(NAME client_examples COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/ClientExamplesTest.py"
                     --serial $<TARGET_FILE:serial_aircraft> --udp $<TARGET_FILE:udp_recorder>
                     --udp-setting $<TARGET_FILE:udp_setting_sender> ${client_ivy_args})
            set_tests_properties(client_examples PROPERTIES TIMEOUT 40)
        endif()
    endif()
endif()
