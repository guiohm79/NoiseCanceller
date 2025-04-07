cmake_minimum_required(VERSION 3.10)
project(NoiseInverter VERSION 1.0)

# Configuration C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options de compilation spécifiques pour la faible latence
if(MSVC)
    add_compile_options(/O2 /W3)
else()
    add_compile_options(-O3 -Wall -march=native)
endif()

# Trouver RtAudio
find_package(RtAudio QUIET)

# Si RtAudio n'est pas trouvé via find_package, chercher manuellement
if(NOT RtAudio_FOUND)
    # Pour Windows avec MinGW ou MSYS2
    if(WIN32)
        set(RTAUDIO_INCLUDE_DIR "C:/msys64/mingw64/include" CACHE PATH "Path to RtAudio headers")
        set(RTAUDIO_LIBRARY "C:/msys64/mingw64/lib/librtaudio.dll.a" CACHE FILEPATH "Path to RtAudio library")
    # Pour Linux/macOS, essayer les emplacements courants
    else()
        set(RTAUDIO_INCLUDE_DIR "/usr/include;/usr/local/include" CACHE PATH "Path to RtAudio headers")
        set(RTAUDIO_LIBRARY "/usr/lib/librtaudio.so;/usr/local/lib/librtaudio.so;/usr/lib/x86_64-linux-gnu/librtaudio.so" CACHE FILEPATH "Path to RtAudio library")
    endif()
endif()

# Ajouter les sources
set(SOURCES 
    src/main.cpp
    src/NoiseInverter.cpp
)

# Créer l'exécutable
add_executable(noise_inverter ${SOURCES})

# Inclure les répertoires d'en-têtes
if(RtAudio_FOUND)
    target_include_directories(noise_inverter PRIVATE ${RTAUDIO_INCLUDE_DIRS})
    target_link_libraries(noise_inverter PRIVATE RtAudio::rtaudio)
else()
    target_include_directories(noise_inverter PRIVATE ${RTAUDIO_INCLUDE_DIR})
    
    # Gérer différemment selon la plateforme
    if(WIN32)
        target_link_libraries(noise_inverter PRIVATE ${RTAUDIO_LIBRARY})
    elseif(APPLE)
        find_library(COREAUDIO_LIBRARY CoreAudio)
        find_library(COREFOUNDATION_LIBRARY CoreFoundation)
        target_link_libraries(noise_inverter PRIVATE ${RTAUDIO_LIBRARY} ${COREAUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY})
    else() # Linux
        target_link_libraries(noise_inverter PRIVATE ${RTAUDIO_LIBRARY} asound pthread)
    endif()
endif()

# Options de compilation spécifiques à l'OS
if(WIN32)
    # Activer les apis audio spécifiques à Windows
    target_compile_definitions(noise_inverter PRIVATE __WINDOWS_ASIO__)
    target_compile_definitions(noise_inverter PRIVATE __WINDOWS_WASAPI__)
elseif(APPLE)
    target_compile_definitions(noise_inverter PRIVATE __MACOSX_CORE__)
else() # Linux
    target_compile_definitions(noise_inverter PRIVATE __LINUX_ALSA__)
    target_compile_definitions(noise_inverter PRIVATE __UNIX_JACK__)
endif()

# Installation
install(TARGETS noise_inverter DESTINATION bin)