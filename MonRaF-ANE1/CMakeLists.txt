cmake_minimum_required(VERSION 3.10)
project(MonRaF-ANE1 C)

# Usar C11 y optimización Release por defecto
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Opciones
option(USE_HACKRF "Link with libhackrf if available" ON)
option(BUILD_SHARED_LIBS "Build shared libs if possible" OFF)

# Fuentes (ajusta si renombraste archivos)
set(SRC
    test_capture.c
    Modules/capture.c
    Modules/processing.c
    Modules/storage.c
    Modules/bacn_RF.c
    Modules/cs8_to_iq.c
    Modules/welch.c
    Modules/save_to_file.c
)

add_executable(test_capture ${SRC})

# Flags de compilación adicionales (imitando -O3 -pthread -std=c11)
target_compile_options(test_capture PRIVATE
  $<$<CONFIG:Release>:-O3>
)

# Buscar bibliotecas del sistema
find_library(FFTW3_LIB fftw3 HINTS /usr/lib /usr/local/lib)
if(FFTW3_LIB)
  message(STATUS "Found FFTW3: ${FFTW3_LIB}")
  target_link_libraries(test_capture PRIVATE ${FFTW3_LIB})
else()
  message(WARNING "libfftw3 not found. Install libfftw3-dev (Debian/Ubuntu) or set FFTW3_LIB.")
endif()

# math lib
target_link_libraries(test_capture PRIVATE m)

# pthread (en la mayoría de sistemas no es necesario añadir explícitamente, pero lo dejo)
find_package(Threads REQUIRED)
target_link_libraries(test_capture PRIVATE Threads::Threads)

# hackrf (opcional)
if(USE_HACKRF)
  find_library(HACKRF_LIB hackrf HINTS /usr/lib /usr/local/lib)
  if(HACKRF_LIB)
    message(STATUS "Found libhackrf: ${HACKRF_LIB}")
    target_link_libraries(test_capture PRIVATE ${HACKRF_LIB})
  else()
    message(WARNING "libhackrf not found. If you need HackRF support install libhackrf-dev or set HACKRF_LIB.")
  endif()
endif()

# Instalación (opcional)
install(TARGETS test_capture RUNTIME DESTINATION bin)
