cmake_minimum_required(VERSION 3.10)
project(pprzlink++)

set(CMAKE_CXX_STANDARD 17)
include(GNUInstallDirs)


find_package(tinyxml2 REQUIRED)
find_package(Boost COMPONENTS system)
find_library(IVY_LIB NAMES ivy)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pthread")

include_directories(./ ${CMAKE_PREFIX_PATH}/include)
link_directories(${CMAKE_PREFIX_PATH}/lib)

set(SOURCE
        ivy-c++/Ivy.cpp
        ivy-c++/IvyApplication.cpp
        pprzlink/BoostSerialPortDevice.cpp
        pprzlink/BoostSerialPortDevice.h
        pprzlink/FieldValue.cpp
        pprzlink/IvyLink.cpp
        pprzlink/Link.cpp
        pprzlink/Message.cpp
        pprzlink/MessageDefinition.cpp
        pprzlink/MessageDictionary.cpp
        pprzlink/MessageField.cpp
        pprzlink/MessageFieldTypes.cpp
        pprzlink/PprzTransport.cpp)

set(HEADERS_IVY
        ivy-c++/Ivy.h
        ivy-c++/IvyApplication.h
        ivy-c++/IvyCallback.h
)
set(HEADERS_PPRZLINK_EXCEPTION
        pprzlink/exceptions/pprzlink_exception.h)
set(HEADERS_PPRZLINK
        pprzlink/BoostSerialPortDevice.h
        pprzlink/Device.h
        pprzlink/FieldValue.h
        pprzlink/IvyLink.h
        pprzlink/Link.h
        pprzlink/Message.h
        pprzlink/MessageDefinition.h
        pprzlink/MessageDictionary.h
        pprzlink/MessageField.h
        pprzlink/MessageFieldTypes.h
        pprzlink/PprzTransport.h
        pprzlink/Transport.h)

add_library(pprzlink++_static ${SOURCE})
add_library(pprzlink++ SHARED ${SOURCE})
#set_target_properties(pprzlink++ PROPERTIES PUBLIC_HEADER "${HEADERS_IVY};${HEADERS_PPRZLINK_EXCEPTION};${HEADERS_PPRZLINK}")
target_link_libraries(${PROJECT_NAME}
        #${Boost_LIBRARIES}
        ${IVY_LIB}
        tinyxml2
        Boost::system
        )

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Config 
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${HEADERS_IVY} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ivy-c++")
install(FILES ${HEADERS_PPRZLINK} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/pprzlink")
install(FILES ${HEADERS_PPRZLINK_EXCEPTION} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/pprzlink/exceptions")

export(TARGETS
        ${PROJECT_NAME}
        FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
)
install(EXPORT ${PROJECT_NAME}Config DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/cmake")
