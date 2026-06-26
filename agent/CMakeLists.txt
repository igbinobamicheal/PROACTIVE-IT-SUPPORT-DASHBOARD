cmake_minimum_required(VERSION 3.15)
project(proactive_it_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(AGENT_SOURCES
    src/main.cpp
    src/MetricCollector.cpp
    src/HttpClient.cpp
    src/ConfigManager.cpp
    src/DeviceRegistrar.cpp
    src/AgentService.cpp
)

add_executable(proactive_it_agent ${AGENT_SOURCES})

target_link_libraries(proactive_it_agent PRIVATE
    nlohmann_json::nlohmann_json
    cpr::cpr
)

if(MSVC)
    target_compile_options(proactive_it_agent PRIVATE /EHsc /utf-8)
endif()
