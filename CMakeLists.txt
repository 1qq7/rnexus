cmake_minimum_required(VERSION 3.10)
project(rnexus LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Header-only library
# ------------------------------------------------------------------------------

add_library(rnexus INTERFACE)
target_include_directories(rnexus INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ------------------------------------------------------------------------------
# Examples
# ------------------------------------------------------------------------------

add_executable(basic_pubsub examples/basic_pubsub.cpp)
target_link_libraries(basic_pubsub PRIVATE rnexus)

add_executable(basic_service examples/basic_service.cpp)
target_link_libraries(basic_service PRIVATE rnexus)

# ------------------------------------------------------------------------------
# Install (optional, for system-wide use)
# ------------------------------------------------------------------------------

include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS rnexus EXPORT rnexusTargets)

install(EXPORT rnexusTargets
    FILE rnexusConfig.cmake
    NAMESPACE rnexus::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rnexus
)
