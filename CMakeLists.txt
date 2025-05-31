# CMakeList.txt : CMake project for ControllerModeMonitor, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("ControllerModeMonitor")

ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

file(GLOB monitorSRC
     "src/*.h"
     "src/*.cpp"
     "src/TV/*.h"
     "src/TV/*.cpp"
)

execute_process(COMMAND "pwsh" "build_ico.ps1"
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE ico_out)

find_package(CURL REQUIRED)
find_package(boost_asio REQUIRED CONFIG)
find_package(pugixml CONFIG REQUIRED)

message(MONITOR_FILES="${monitorSRC}")

# Add source to this project's executable.

add_executable (ControllerModeMonitor WIN32 ${monitorSRC})

target_sources (ControllerModeMonitor PRIVATE ControllerModeMonitor.rc)

target_link_libraries(ControllerModeMonitor PRIVATE Boost::asio)
target_link_libraries(ControllerModeMonitor PRIVATE CURL::libcurl)
target_link_libraries(ControllerModeMonitor PRIVATE pugixml::shared pugixml::pugixml)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ControllerModeMonitor PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
