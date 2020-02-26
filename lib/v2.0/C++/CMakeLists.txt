cmake_minimum_required(VERSION 3.10)
project(pprzlink++)

set(CMAKE_CXX_STANDARD 17)

include_directories(ivy-c++)
include_directories(pprzlink)
include_directories(pprzlink/exceptions)

find_package(tinyxml2 REQUIRED)
find_package(Boost COMPONENTS system)
find_library(IVY_LIB NAMES ivy)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pthread")

include_directories(./)

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

set(HEADERS
        ivy-c++/Ivy.h
        ivy-c++/IvyApplication.h
        ivy-c++/IvyCallback.h
        pprzlink/exceptions/pprzlink_exception.h
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

target_link_libraries(${PROJECT_NAME}
        #${Boost_LIBRARIES}
        ${IVY_LIB}
        tinyxml2
        Boost::system
        )

install(TARGETS pprzlink++ DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
