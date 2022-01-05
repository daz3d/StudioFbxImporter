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

SET( FBX_SDK_VERSION "2014.2.1" )
SET( FBX_SDK_VSTUDIO_VERSION "vs2010" )

SET( DZFBX_SDK_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/FBXSDK/${FBX_SDK_VERSION}" )
SET( DZFBX_SDK_INCLUDE "${DZFBX_SDK_ROOT_DIR}/include" )
if( WIN32 )
	SET( DZFBX_SDK_LIB "${DZFBX_SDK_ROOT_DIR}/lib/${FBX_SDK_VSTUDIO_VERSION}/${DZ_PLATFORM}/release/libfbxsdk-md.lib" )
	SET( DZFBX_SDK_SHARED "${DZFBX_SDK_ROOT_DIR}/lib/${FBX_SDK_VSTUDIO_VERSION}/${DZ_PLATFORM}/release/libfbxsdk.dll" )
	SET( DZFBX_SDK_DEBUG_LIB "${DZFBX_SDK_ROOT_DIR}/lib/${FBX_SDK_VSTUDIO_VERSION}/${DZ_PLATFORM}/release/libfbxsdk-md.lib" )
	SET( DZFBX_SDK_DEBUG_SHARED "${DZFBX_SDK_ROOT_DIR}/lib/${FBX_SDK_VSTUDIO_VERSION}/${DZ_PLATFORM}/release/libfbxsdk.dll" )
else()
	SET( DZFBX_SDK_LIB "${DZFBX_SDK_ROOT_DIR}/libmac/gcc4/ub/debug/shared/libfbxsdk.dylib" )
	SET( DZFBX_SDK_SHARED ${DZFBX_SDK_LIB} )
	SET( DZFBX_SDK_DEBUG_LIB "${DZFBX_SDK_ROOT_DIR}/libmac/gcc4/ub/release/shared/libfbxsdk.dylib" )
	SET( DZFBX_SDK_DEBUG_SHARED ${DZFBX_SDK_DEBUG_LIB} )
endif( WIN32 )

add_library( DzFbx SHARED IMPORTED )
	set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_LOCATION ${DZFBX_SDK_SHARED} )
	set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_LOCATION_DEBUG ${DZFBX_SDK_DEBUG_SHARED} )
	if(WIN32)
		set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_IMPLIB ${DZFBX_SDK_LIB})
		set_property( TARGET DzFbx APPEND PROPERTY IMPORTED_IMPLIB_DEBUG ${DZFBX_SDK_DEBUG_LIB} )
	endif( WIN32 )
set_property( TARGET DzFbx APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${DZFBX_SDK_INCLUDE}" )

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
