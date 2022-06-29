cmake_minimum_required(VERSION 3.0.0)
project(znet VERSION 0.1.0)

include_directories("include")
include_directories("opt/kcp-1.7")
include_directories("opt/libuv-1.44.1/include")

add_subdirectory("opt/libuv-1.44.1")
add_subdirectory("opt/kcp-1.7")

include(CTest)
enable_testing()

add_executable(server app/server.cpp)
target_link_libraries(server uv kcp)
add_executable(client app/client.cpp)
target_link_libraries(client uv kcp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
