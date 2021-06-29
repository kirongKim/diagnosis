cmake_minimum_required(VERSION 3.0)
project(diagnosis)

add_executable(diagnosis 
	source/main.cpp 
	source/diagnosisMgr.cpp
)

set(CMAKE_MODULE_PATH 
    ${CMAKE_MODULE_PATH} 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)

set(CMAKE_COMMONLIB_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_COMMONLIB_PATH}/common/include)

add_library(${PROJECT_NAME}_common STATIC 
	${CMAKE_COMMONLIB_PATH}/common/source/log.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/common.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/timer_util.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/ThreadPool.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/eventSocket.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/eventPacket.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/ByteBuffer.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/CameraStatusEvent.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/RadarStatusEvent.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/ModemStatusEvent.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/AccelerometerStatusInd.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/AccelerometerPositionInd.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/BatteryLevelInd.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/common_util.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/DiagServerSocket.cpp
	${CMAKE_COMMONLIB_PATH}/common/source/slipcManager.cpp
	)

target_link_libraries (${PROJECT_NAME}_common pthread)

find_package (Glib)
if (Glib_FOUND)
  include_directories(${Glib_INCLUDE_DIRS})
  target_link_libraries (diagnosis ${Glib_LIBRARIES})
endif (Glib_FOUND)

target_link_libraries(${PROJECT_NAME}_common
	rt
)

target_link_libraries(diagnosis 
	pthread
	${PROJECT_NAME}_common
	)

//install(TARGETS diagnosis RUNTIME DESTINATION bin)
