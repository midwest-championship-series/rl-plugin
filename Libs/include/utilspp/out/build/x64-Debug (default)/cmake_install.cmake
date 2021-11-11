# Install script for directory: H:/MNCS/rl-plugin-updater/Libs/include/utilspp

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/out/install/x64-Debug (default)")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/Strawberry/c/bin/objdump.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/include/utilspp/EmptyType.hpp;/include/utilspp/Functors.hpp;/include/utilspp/NonCopyable.hpp;/include/utilspp/NullType.hpp;/include/utilspp/Singleton.hpp;/include/utilspp/SmartPtr.hpp;/include/utilspp/ThreadingFactoryMutex.hpp;/include/utilspp/ThreadingFactoryMutex.inl;/include/utilspp/ThreadingSingle.hpp;/include/utilspp/ThreadingSingle.inl;/include/utilspp/TypeList.hpp;/include/utilspp/TypeTrait.hpp;/include/utilspp/clone_ptr.hpp")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
file(INSTALL DESTINATION "/include/utilspp" TYPE FILE FILES
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/EmptyType.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/Functors.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/NonCopyable.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/NullType.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/Singleton.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/SmartPtr.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/ThreadingFactoryMutex.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/ThreadingFactoryMutex.inl"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/ThreadingSingle.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/ThreadingSingle.inl"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/TypeList.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/TypeTrait.hpp"
    "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/clone_ptr.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "H:/MNCS/rl-plugin-updater/Libs/include/utilspp/out/build/x64-Debug (default)/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
