#######################################################################
#	Copyright (C) 2002-2022 DAZ 3D, Inc. All Rights Reserved.
#
#	Licensed under the Apache License, Version 2.0 (the "License");
#	you may not use this file except in compliance with the License.
#	You may obtain a copy of the License at
#
#		http://www.apache.org/licenses/LICENSE-2.0
#
#	Unless required by applicable law or agreed to in writing, software
#	distributed under the License is distributed on an "AS IS" BASIS,
#	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#	See the License for the specific language governing permissions and
#	limitations under the License.
#######################################################################

cmake_minimum_required( VERSION 3.4.0 )

project( "DAZ Studio Fbx Importer" )

set_property( GLOBAL PROPERTY USE_FOLDERS ON )

set( DAZ_STUDIO_EXE_DIR "" CACHE PATH "Path to DAZ Studio, needs to be installed to a writeable location." )

set( DAZ_SDK_DIR "" CACHE PATH "Path to root of the DAZ Studio SDK." )
if( NOT EXISTS ${DAZ_SDK_DIR} )
	message( SEND_ERROR "Provide a valid path to the DAZ Studio SDK (DAZ_SDK_DIR)." )
	return()
endif()

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )
include( DzMocFiles )

# Define platform specific variables
if( WIN32 )
	set( DZ_LIB_SUFFIX ".lib" )
	set( DZ_BIN_SUFFIX ".dll" )
	set( DZ_LIB_PREFIX "" )
	set( UTIL_EXT ".exe" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 4 )
		set( DZ_PLATFORM x86 )
		set( DZ_MIXED_PLATFORM Win32 )
		set( DZ_OS_PLATFORM Win32 )
	elseif( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( DZ_PLATFORM x64 )
		set( DZ_MIXED_PLATFORM x64 )
		set( DZ_OS_PLATFORM Win64 )
	else()
		message( FATAL_ERROR "Unknown architecture" )
	endif()
elseif(APPLE)
	set( DZ_LIB_SUFFIX ".dylib" )
	set( DZ_BIN_SUFFIX ".dylib" )
	set( DZ_LIB_PREFIX "lib" )
	set( UTIL_EXT "" )
	if( CMAKE_SIZEOF_VOID_P EQUAL 4 )
		set( DZ_PLATFORM x86 )
		set( DZ_MIXED_PLATFORM Mac32 )
		set( DZ_OS_PLATFORM Mac32 )
	elseif( CMAKE_SIZEOF_VOID_P EQUAL 8 )
		set( DZ_PLATFORM x64 )
		set( DZ_MIXED_PLATFORM Mac64 )
		set( DZ_OS_PLATFORM Mac64 )
	else()
		message( FATAL_ERROR "Unknown architecture" )
	endif()
	set( CMAKE_MACOSX_RPATH TRUE )
	set( CMAKE_BUILD_WITH_INSTALL_RPATH TRUE )
else()
	message( FATAL_ERROR "Unknown architecture" )
endif( WIN32 )

set( DAZ_SDK_INCLUDE_DIR "${DAZ_SDK_DIR}/include" CACHE FILEPATH "Path to daz sdk includes." )
set( DAZ_SDK_CORE_RELATIVE_PATH "lib/${DZ_MIXED_PLATFORM}/${DZ_LIB_PREFIX}dzcore${DZ_LIB_SUFFIX}" )
set( DAZ_SDK_LIB "${DAZ_SDK_DIR}/${DAZ_SDK_CORE_RELATIVE_PATH}" CACHE FILEPATH "Path to dzcore."  )

add_library( dzcore SHARED IMPORTED )
if( WIN32 )
	set_property( TARGET dzcore APPEND PROPERTY IMPORTED_IMPLIB ${DAZ_SDK_LIB} )
else()
	set_property( TARGET dzcore APPEND PROPERTY IMPORTED_LOCATION ${DAZ_SDK_LIB} )
endif( WIN32 )
set_property( TARGET dzcore APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${DAZ_SDK_INCLUDE_DIR}" )

# Setup Qt from the DAZ SDK
if( WIN32 )
	set( DAZ_SDK_QTCORE_LIBRARY "${DAZ_SDK_DIR}/lib/${DZ_MIXED_PLATFORM}/QtCore4.lib" )
elseif( APPLE )
	set( DAZ_SDK_QTCORE_LIBRARY "${DAZ_SDK_DIR}/lib/${DZ_MIXED_PLATFORM}/QtCore.framework" )
endif()

set( QT_QTCORE_LIBRARY_RELEASE ${DAZ_SDK_QTCORE_LIBRARY} )
set( QT_QMAKE_EXECUTABLE "${DAZ_SDK_DIR}/bin/${DZ_MIXED_PLATFORM}/qmake${UTIL_EXT}" )
set( QT_BINARY_DIR "${DAZ_SDK_DIR}/bin/${DZ_MIXED_PLATFORM}" )
set( QT_HEADERS_DIR "${DAZ_SDK_DIR}/include" )
set( QT_QTCORE_INCLUDE_DIR "${DAZ_SDK_DIR}/include/QtCore" )

# the qt find module needs this folder but our build does not so just fake it
file( MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/DUMMY_MKSPECS/default" )
set( QT_MKSPECS_DIR "${CMAKE_CURRENT_BINARY_DIR}/DUMMY_MKSPECS" )

find_package( Qt4 4.8.1 REQUIRED QtCore QtGui QtScript QtOpenGL QtNetwork QtSql QtXml )

set( DZSDK_QT_CORE_TARGET Qt4::QtCore )
set( DZSDK_QT_GUI_TARGET Qt4::QtGui )
set( DZSDK_QT_SCRIPT_TARGET Qt4::QtScript )
set( DZSDK_QT_OPENGL_TARGET Qt4::QtOpenGL )
set( DZSDK_QT_NETWORK_TARGET Qt4::QtNetwork )
set( DZSDK_QT_SQL_TARGET Qt4::QtSql )
set( DZSDK_QT_XML_TARGET Qt4::QtXml )

set( PLUGIN_MOC_OPTIONS 
	"-I${DAZ_SDK_INCLUDE_DIR}"
)

function( setup_standard_plugin_source_groups )
	source_group( "Resource Files" REGULAR_EXPRESSION ".*\\.(def|rc|ico|icns|rgs)" )
	source_group( "Source Files\\generated\\moc" REGULAR_EXPRESSION "/moc_.*\\.c(xx|pp)$" )
	source_group( "Header Files\\generated\\ui files" REGULAR_EXPRESSION "/ui_.*\\.h$" )
	source_group( "Source Files\\ui files" REGULAR_EXPRESSION ".*\\.ui$" )
endfunction()

# if DAZ_STUDIO_EXE_DIR is not defined the compiled result will be placed in
# the default CMake binary folder
# if DAZ_STUDIO_EXE_DIR is defined the compiled result placed in the Daz Studio ./plugins directory
if( DAZ_STUDIO_EXE_DIR )
	foreach( curConfig ${CMAKE_CONFIGURATION_TYPES} )
		string( TOUPPER ${curConfig} curConfig )
		set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${curConfig} ${DAZ_STUDIO_EXE_DIR}/plugins )
		set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${curConfig} ${DAZ_STUDIO_EXE_DIR}/plugins )
	endforeach()
endif()

add_subdirectory( "FBX Importer" )
