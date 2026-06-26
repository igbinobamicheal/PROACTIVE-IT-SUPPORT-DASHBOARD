cmake_minimum_required(VERSION 3.15)
project(proactive_it_dashboard LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies using vcpkg
find_package(Crow CONFIG REQUIRED)
find_package(mysql-concpp CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# Setup bcrypt C library sources
set(BCRYPT_SOURCES
    third_party/libbcrypt/libbcrypt.c
    third_party/libbcrypt/crypt_blowfish/crypt_blowfish.c
    third_party/libbcrypt/crypt_blowfish/crypt_gensalt.c
    third_party/libbcrypt/crypt_blowfish/wrapper.c
)

# Setup our C++ application sources
set(APP_SOURCES
    src/main.cpp
    src/config/Config.cpp
    src/database/Database.cpp
    src/repositories/UserRepository.cpp
    src/repositories/DeviceRepository.cpp
    src/repositories/MetricRepository.cpp
    src/repositories/AlertRepository.cpp
    src/controllers/AuthController.cpp
    src/controllers/DeviceController.cpp
    src/controllers/MetricController.cpp
    src/controllers/AlertController.cpp
    src/utils/JWTUtil.cpp
    src/utils/BcryptUtil.cpp
    src/utils/EventBroker.cpp
)

# Executable target
add_executable(proactive_it_dashboard ${APP_SOURCES} ${BCRYPT_SOURCES})

# Include directories
target_include_directories(proactive_it_dashboard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libbcrypt
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libbcrypt/crypt_blowfish
)

# Compile definitions
target_compile_definitions(proactive_it_dashboard PRIVATE
    JWT_DISABLE_PICOJSON
)

# Link libraries
target_link_libraries(proactive_it_dashboard PRIVATE
    Crow::Crow
    mysql::concpp
    jwt-cpp::jwt-cpp
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Ensure CRT functions are secure (avoid warnings on MSVC) and enable modern standards
if(MSVC)
    target_compile_definitions(proactive_it_dashboard PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0A00
    )
    target_compile_options(proactive_it_dashboard PRIVATE /EHsc /utf-8)
endif()


