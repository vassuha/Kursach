cmake_minimum_required(VERSION 3.26)
project(Kursach)

set(CMAKE_CXX_STANDARD 17)

# Укажите путь к папке с заголовочными файлами cURL
set(CURL_INCLUDE_DIRS "C:/curl/include")

find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

add_executable(Kursach main.cpp)

# Линковка с библиотекой cURL
target_link_libraries(Kursach ${CURL_LIBRARIES})
