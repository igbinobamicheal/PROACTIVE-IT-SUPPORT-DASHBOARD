cmake_minimum_required(VERSION 3.15)
project(proactive_it_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(AGENT_SOURCES
    src/Infrastructure/Logging/Logger.cpp
    src/Infrastructure/Config/JsonConfigRepository.cpp
    src/Infrastructure/Config/OfflineCache.cpp
    src/Infrastructure/Metrics/WindowsMetricCollector.cpp
    src/Infrastructure/Network/CprHttpSender.cpp
    src/Application/AgentDaemon.cpp
    src/App/ServiceManager.cpp
    src/App/main.cpp
)

add_executable(proactive_it_agent ${AGENT_SOURCES})

target_link_libraries(proactive_it_agent PRIVATE
    nlohmann_json::nlohmann_json
    cpr::cpr
    ws2_32
    iphlpapi
    wtsapi32
    advapi32
    bcrypt
)

if(MSVC)
    target_compile_options(proactive_it_agent PRIVATE /EHsc /utf-8 /W4)
endif()
