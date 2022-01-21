#######################################################################
#	Copyright (C) 2016-2022 DAZ 3D, Inc. All Rights Reserved.
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

macro( dz_moc_headers is_qt5_moc addtional_options outputList )
	foreach(mocHeader ${ARGN})
		get_filename_component(bareMocHeaderName ${mocHeader} NAME)
		if( ${is_qt5_moc} )
			qt5_wrap_cpp( ${outputList} ${mocHeader} 
				OPTIONS 
					${${addtional_options}}
					-f${bareMocHeaderName} )
		else()
			qt4_wrap_cpp( ${outputList} ${mocHeader} 
				OPTIONS 
					${${addtional_options}}
					-f${bareMocHeaderName} )
		endif()
		list(APPEND ${outputList} ${sourceFile} ${mocHeader} )
	endforeach()
endmacro( dz_moc_headers )

macro( dz_uic_headers is_qt5 out_path addtional_options outputList )
	#Adapted from qt4_wrap_ui found in Qt4Macros.cmake
	#BSD License, see cmake modules
	foreach(uicFile ${ARGN})
		get_filename_component(outfile ${uicFile} NAME_WE)
		get_filename_component(infile ${uicFile} ABSOLUTE)
		set(outfile ${out_path}/ui_${outfile}.h)
		if( ${is_qt5} )
			set(UIC_COMMAND Qt5::uic )
		else()
			set(UIC_COMMAND Qt4::uic )
		endif()
		add_custom_command(OUTPUT ${outfile}
			COMMAND ${UIC_COMMAND} 
			ARGS ${${addtional_options}} -o ${outfile} ${infile}
			MAIN_DEPENDENCY ${infile} VERBATIM)
		list(APPEND ${outputList} ${outfile} ${uicFile})
	endforeach()
endmacro( dz_uic_headers )


macro( dz_rcc is_qt5 addtional_options outputList )
	foreach(rccFile ${ARGN})
		get_filename_component(bareFilename ${rccFile} NAME_WE)
		if( ${is_qt5} )
			qt5_add_resources( ${outputList} ${rccFile} 
				OPTIONS 
					${${addtional_options}} )
		else()
			qt4_add_resources( ${outputList} ${rccFile} 
				OPTIONS 
					${${addtional_options}} )
		endif()
		list(APPEND ${outputList} ${sourceFile} ${rccFile} )
	endforeach()
endmacro( dz_rcc )

function(dz_source_subgroup name)
	string(REPLACE "/" "\\" folder_name ${name})
	source_group("Header Files\\${folder_name}" REGULAR_EXPRESSION ".*/${name}/.*\\.[hH]")
	source_group("Source Files\\${folder_name}" REGULAR_EXPRESSION ".*/${name}/.*\\.c(|pp)")
endfunction()

function(dz_optimize_target target)
	if (MSVC)
		get_property(dz_current_rel_flags TARGET ${target} PROPERTY LINK_FLAGS_RELWITHDEBINFO)
		set_target_properties(
			${target} 
			PROPERTIES
				LINK_FLAGS_RELWITHDEBINFO "${dz_current_rel_flags} /OPT:REF /OPT:ICF /INCREMENTAL:NO"
		)
	endif(MSVC)
endfunction()

function(dz_uac_linker_target target)
	if (MSVC)
		get_property(dz_linker_flags TARGET ${target} PROPERTY LINK_FLAGS)
		set_target_properties(
			${target} 
			PROPERTIES
				LINK_FLAGS "${dz_linker_flags} /MANIFESTUAC:\"level=asInvoker uiAccess=false\""
		)
	endif(MSVC)
endfunction()

function(dz_grow_stack_linker_target target)
	get_property(dz_linker_flags TARGET ${target} PROPERTY LINK_FLAGS)
	if (MSVC)
		set_target_properties(
			${target} 
			PROPERTIES
				LINK_FLAGS "${dz_linker_flags} /STACK:0x1000000"
		)
	elseif(APPLE)
		set_target_properties(
			${target} 
			PROPERTIES
				LINK_FLAGS "${dz_linker_flags} -Wl,-stack_size -Wl,0x1000000"
		)
	endif()
endfunction()