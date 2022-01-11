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

SET( FBX_SDK_VERSION "2020.2.0" CACHE STRING "FBX SDK version, used for locating the path to the FBX SDK libraries." )
SET( FBX_SDK_VSTUDIO_VERSION "vs2019" CACHE STRING "FBX SDK VS compiler version, used for locating the path to the FBX SDK libraries." )

SET( DZFBX_SDK_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/FBXSDK/${FBX_SDK_VERSION}" CACHE PATH "Root directory of the FBX SDK." )
SET( DZFBX_SDK_INCLUDE "${DZFBX_SDK_ROOT_DIR}/include" CACHE PATH "Include directory of the FBX SDK." )
if( WIN32 )
	SET( DZFBX_SDK_LIB "${DZFBX_SDK_ROOT_DIR}/lib/${FBX_SDK_VSTUDIO_VERSION}/${DZ_PLATFORM}/release/libfbxsdk.lib" CACHE FILEPATH "" )
	SET( DZFBX_SDK_SHARED "${DZFBX_SDK_ROOT_DIR}/lib/${FBX_SDK_VSTUDIO_VERSION}/${DZ_PLATFORM}/release/libfbxsdk.dll" CACHE FILEPATH "" )

	SET( DZFBX_SDK_DEBUG_LIB "${DZFBX_SDK_ROOT_DIR}/lib/${FBX_SDK_VSTUDIO_VERSION}/${DZ_PLATFORM}/debug/libfbxsdk.lib" CACHE FILEPATH "" )
	SET( DZFBX_SDK_DEBUG_SHARED "${DZFBX_SDK_ROOT_DIR}/lib/${FBX_SDK_VSTUDIO_VERSION}/${DZ_PLATFORM}/debug/libfbxsdk.dll" CACHE FILEPATH "" )
else()
	SET( DZFBX_SDK_LIB "${DZFBX_SDK_ROOT_DIR}/libmac/gcc4/ub/debug/shared/libfbxsdk.dylib" CACHE FILEPATH "" )
	SET( DZFBX_SDK_SHARED ${DZFBX_SDK_LIB} CACHE FILEPATH "" )
	SET( DZFBX_SDK_DEBUG_LIB "${DZFBX_SDK_ROOT_DIR}/libmac/gcc4/ub/release/shared/libfbxsdk.dylib" CACHE FILEPATH "" )
	SET( DZFBX_SDK_DEBUG_SHARED ${DZFBX_SDK_DEBUG_LIB} CACHE FILEPATH "" )
endif( WIN32 )

add_library( DzFbx SHARED IMPORTED )
	set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_LOCATION ${DZFBX_SDK_SHARED} )
	set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_LOCATION_DEBUG ${DZFBX_SDK_DEBUG_SHARED} )
	if(WIN32)
		set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_IMPLIB ${DZFBX_SDK_LIB})
		set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_IMPLIB_DEBUG ${DZFBX_SDK_DEBUG_LIB} )
	endif( WIN32 )
set_property( TARGET DzFbx APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${DZFBX_SDK_INCLUDE}" )
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
