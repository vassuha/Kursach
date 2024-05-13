cmake_minimum_required(VERSION 3.26)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

project(Kursach)

set(CMAKE_CXX_STANDARD 17)

# Укажите путь к папке с заголовочными файлами cURL
set(CURL_INCLUDE_DIRS "C:/curl/include")

find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

add_executable(Kursach main.cpp)
add_link_options(-static -static-libgcc -static-libstdc++)

# Линковка с библиотекой cURL
target_link_libraries(Kursach ${CURL_LIBRARIES})

#target_include_directories(Kursach PRIVATE ${CMAKE_SOURCE_DIR}/sgp4)

#target_link_libraries(Kursach PRIVATE path/to/sgp4/build/sgp4.lib)

# Добавьте зависимость от библиотеки perturb
include(FetchContent)

FetchContent_Declare(
        perturb
        GIT_REPOSITORY "https://github.com/gunvirranu/perturb"
)


FetchContent_MakeAvailable(perturb)

# Link perturb into your project
target_link_libraries(Kursach perturb)