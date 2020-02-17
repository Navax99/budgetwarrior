cmake_minimum_required (VERSION 3.8)
project (budgetwarrier)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INSTALL_PREFIX /usr/local)

set(warnings "-Wall -Wextra -Werror")

find_package(OpenSSL REQUIRED)

include_directories(include)
include_directories(cpp-httplib)
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(/usr/local/include)

link_directories(/usr/local/lib)

add_subdirectory(src)
