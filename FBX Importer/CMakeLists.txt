#######################################################################
#	Copyright (C) 2016-2021 Daz 3D, Inc. All Rights Reserved.
#
#	This is UNPUBLISHED PROPRIETARY SOURCE CODE of DAZ 3D, Inc.
#	the contents of this file may not be disclosed to third parties,
#	copied or duplicated in any form, in whole or in part, without the
#	prior written permission of DAZ 3D, Inc.
#######################################################################

SET( CMAKE_AUTOMOC ON )
SET( CMAKE_INCLUDE_CURRENT_DIR ON )

SET( DZ_PLUGIN_TGT_NAME dzfbximporter )
SET( DZ_PLUGIN_PROJECT_NAME "FBX Importer" )

if( WIN32 )
	SET( OS_SOURCES
		DzFbxImporter.rc
		DzFbxImporter.def
	)
elseif( APPLE )
	SET( OS_SOURCES "" )
endif()

SET( PLUGIN_MOC_SOURCES "" )

dz_moc_headers( PLUGIN_MOC_OPTIONS PLUGIN_MOC_SOURCES
	dzfbximporter.h
)

option( FBX_CUSTOM_LAYOUT "If the FBX SDK directory has a custom layout." OFF )

SET( FBX_SDK_INSTALL_DIR "" CACHE PATH "Install directory of the FBX SDK." )
SET( FBX_SDK_VERSION "" CACHE STRING "FBX SDK version, used for locating the path to the FBX SDK libraries." )
SET( FBX_SDK_VSTUDIO_VERSION "" CACHE STRING "FBX SDK VS compiler version, used for locating the path to the FBX SDK libraries." )

SET( FBX_SDK_ROOT_DIR "${FBX_SDK_INSTALL_DIR}/${FBX_SDK_VERSION}" CACHE PATH "Root directory of the FBX SDK." FORCE)
SET( FBX_SDK_LIB_ROOT_DIR "${FBX_SDK_ROOT_DIR}/lib/${FBX_SDK_VSTUDIO_VERSION}" CACHE PATH "Root directory of the FBX SDK." FORCE)

if( NOT FBX_CUSTOM_LAYOUT )
	SET( FBX_SDK_DIR TRUE )
	if( NOT EXISTS ${FBX_SDK_INSTALL_DIR} )
		message( SEND_ERROR "Provide a valid path to the FBX SDK location (FBX_SDK_INSTALL_DIR)." )
		SET( FBX_SDK_DIR FALSE )
	endif()
	if( NOT EXISTS ${FBX_SDK_ROOT_DIR} OR NOT FBX_SDK_DIR )
		message( SEND_ERROR "Provide a valid path to the FBX SDK location with correct SDK version (FBX_SDK_VERSION)." )
		SET( FBX_SDK_DIR FALSE )
	endif()
	if( NOT EXISTS ${FBX_SDK_LIB_ROOT_DIR} )
		message( SEND_ERROR "Provide a valid path to the FBX SDK library directory with correct visual studio version (FBX_SDK_VSTUDIO_VERSION)." )
		SET( FBX_SDK_DIR FALSE )
	endif()
	if( NOT FBX_SDK_DIR )
		return()
	endif()
endif()

SET( FBX_SDK_INCLUDE "${FBX_SDK_ROOT_DIR}/include" CACHE PATH "Include directory of the FBX SDK." )
if( WIN32 )
	SET( FBX_SDK_LIB "${FBX_SDK_LIB_ROOT_DIR}/${DZ_PLATFORM}/release/libfbxsdk.lib" CACHE FILEPATH "" )
	SET( FBX_SDK_SHARED "${FBX_SDK_LIB_ROOT_DIR}/${DZ_PLATFORM}/release/libfbxsdk.dll" CACHE FILEPATH "" )
	SET( FBX_SDK_DEBUG_LIB "${FBX_SDK_LIB_ROOT_DIR}/${DZ_PLATFORM}/debug/libfbxsdk.lib" CACHE FILEPATH "" )
	SET( FBX_SDK_DEBUG_SHARED "${FBX_SDK_LIB_ROOT_DIR}/${DZ_PLATFORM}/debug/libfbxsdk.dll" CACHE FILEPATH "" )
else()
	SET( FBX_SDK_LIB "${FBX_SDK_ROOT_DIR}/libmac/gcc4/ub/debug/shared/libfbxsdk.dylib" CACHE FILEPATH "" )
	SET( FBX_SDK_SHARED "${FBX_SDK_LIB}" CACHE FILEPATH "" )
	SET( FBX_SDK_DEBUG_LIB "${FBX_SDK_ROOT_DIR}/libmac/gcc4/ub/release/shared/libfbxsdk.dylib" CACHE FILEPATH "" )
	SET( FBX_SDK_DEBUG_SHARED "${FBX_SDK_DEBUG_LIB}" CACHE FILEPATH "" )
endif( WIN32 )

SET( FBX_LIB_EXISTS TRUE )
if( NOT ( EXISTS ${FBX_SDK_INCLUDE} ) )
	message( SEND_ERROR "Invalid path to the FBX SDK include directory (FBX_SDK_INCLUDE)." )
	SET( FBX_LIB_EXISTS FALSE )
endif()
if( NOT ( EXISTS ${FBX_SDK_LIB} ) )
	message( SEND_ERROR "Unable to find the file ${FBX_SDK_LIB} (FBX_SDK_LIB)." )
	SET( FBX_LIB_EXISTS FALSE )
endif()
if( NOT ( EXISTS ${FBX_SDK_SHARED} ) )
	message( SEND_ERROR "Unable to find the file ${FBX_SDK_SHARED} (FBX_SDK_SHARED)." )
	SET( FBX_LIB_EXISTS FALSE )
endif()
if( NOT ( EXISTS ${FBX_SDK_DEBUG_LIB} ) )
	message( SEND_ERROR "Unable to find the file ${FBX_SDK_DEBUG_LIB} (FBX_SDK_DEBUG_LIB)." )
	SET( FBX_LIB_EXISTS FALSE )
endif()
if( NOT ( EXISTS ${FBX_SDK_DEBUG_SHARED} ) )
	message( SEND_ERROR "Unable to find the file ${FBX_SDK_DEBUG_SHARED} (FBX_SDK_DEBUG_SHARED)." )
	SET( FBX_LIB_EXISTS FALSE )
endif()
if( NOT FBX_LIB_EXISTS )
	return()
endif()

add_library( DzFbx SHARED IMPORTED )
	set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_LOCATION "${FBX_SDK_SHARED}" )
	set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_LOCATION_DEBUG "${FBX_SDK_DEBUG_SHARED}" )
	if(WIN32)
		set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_IMPLIB "${FBX_SDK_LIB}" )
		set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_IMPLIB_DEBUG "${FBX_SDK_DEBUG_LIB}" )
	endif( WIN32 )
set_property( TARGET DzFbx APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${FBX_SDK_INCLUDE}" )
# Fbx requires this to be defined when linking dynamically.
set_property( TARGET DzFbx APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS FBXSDK_SHARED )

add_library( ${DZ_PLUGIN_TGT_NAME} SHARED
	dzfbximporter.cpp
	dzfbximporter.h
	pluginmain.cpp
	version.h
	${OS_SOURCES}
	${PLUGIN_MOC_SOURCES}
)

target_link_libraries( ${DZ_PLUGIN_TGT_NAME}
	PRIVATE
	dzcore
	DzFbx
	${DZSDK_QT_CORE_TARGET}
	${DZSDK_QT_GUI_TARGET}
	${DZSDK_QT_SCRIPT_TARGET}
)

target_include_directories(${DZ_PLUGIN_TGT_NAME}
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties( ${DZ_PLUGIN_TGT_NAME}
	PROPERTIES
	FOLDER "My Plugins/Importers"
	PROJECT_LABEL ${DZ_PLUGIN_PROJECT_NAME}
)

setup_standard_plugin_source_groups()

if( WIN32 )
	target_compile_definitions( ${DZ_PLUGIN_TGT_NAME} PRIVATE -D_CRT_SECURE_NO_DEPRECATE )
endif()
