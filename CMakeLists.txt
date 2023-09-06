cmake_minimum_required(VERSION 2.8.11)

project(VolumeRenderingDemo)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/GL3W/include/)

if (MSVC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /WX -D_SCL_SECURE_NO_WARNINGS")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX -D_SCL_SECURE_NO_WARNINGS")
else(MSVC)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
endif(MSVC)

# Used to locate shader sources for debugging
add_definitions(-DSOURCE_PATH="${PROJECT_SOURCE_DIR}")

find_package(MITK REQUIRED)

# Check that MITK has been build with Qt support
if(NOT MITK_USE_QT)
  message(SEND_ERROR "MITK needs to be built with MITK_USE_QT set to ON")
endif()

mitk_create_executable(${PROJECT_NAME} DEPENDS MitkQtWidgetsExt PACKAGE_DEPENDS VTK)
