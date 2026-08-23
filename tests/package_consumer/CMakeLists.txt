cmake_minimum_required(VERSION 3.16)
project(cjong4-opponent-package-consumer LANGUAGES C)

find_package(cjong4-opponent 1.0 CONFIG REQUIRED)

add_executable(cj4_opponent_package_consumer main.c)
target_link_libraries(cj4_opponent_package_consumer PRIVATE cjong4::opponents)

enable_testing()
add_test(
    NAME cj4_opponent_package_consumer
    COMMAND cj4_opponent_package_consumer
)
