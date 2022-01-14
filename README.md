# Daz Studio FBX Importer
* Owner: [Daz 3D][OwnerURL] — [@Daz3d][TwitterURL]
* License: [Apache License, Version 2.0][LicenseURL] - see ``LICENSE`` and ``NOTICE`` for more information.
* Official Project: [github.com/daz3d/StudioFbxImporter][RepositoryURL]

## Prerequisites
* A compatible version of the [Daz Studio][DazStudioURL] application
  * Studio Public Beta 4.16.1.17 or later
* Daz Studio SDK
  * [Version 4.5+][DazStudioSDKURL] or later
* FBX SDK
  * Version 2016 or later
* Operating System
  * Windows 7 or later
* CMake for building
  * Version 3.4.0 or later

## Build instructions
* Make sure to install the version of Daz Studio as per prerequisites.
* Download and install Daz Studio SDK and note down the install location.
* Download FBX SDK and install to any system location.
* CMake is required to generate the project files. CMake can be used either by the gui tool or the command line interface.
  * Using cmake-gui:
    * Run CMake GUI tool.
    * Provide source code path to the repository's root level by using ``Browse Source...`` and a path to the build location by using ``Browse Build...``
    * Click ``Configure``. Specify the generator from the available list based on the preference and use ``v100`` as a toolset option.
      * NOTE: Other compilers may face memory allocation/de-allocation issues.
    * Run ``Configure`` and during first time the tool will throw errors asking the user to set necessary directory paths as described in the following steps. Try configuring after setting required variables.
    * Set ``DAZ_SDK_DIR`` variable to the Daz Studio SDK install location.
    * Set ``FBX_SDK_INSTALL_DIR`` variable to the FBX SDK install location.
    * Set ``FBX_SDK_VERSION`` and ``FBX_SDK_VSTUDIO_VERSION`` variables to the version of the FBX SDK and corresponding visual studio version respectively. These variables are used to generate paths to the library files of FBX SDK.
    * Remaining variables are automatically set based on variables described above. These may also need to be modified in some cases.
    * Once the configuration is successfully done, ``Generate`` the project.
  * Using cmake command line:
    * ``cmake`` can be run at the root level of the repository with specific options as shown in the following command.
    * ``cmake -B <build-path> -G "Visual Studio 16" -T v100 -D DAZ_SDK_DIR=<daz-sdk-path> -D FBX_SDK_INSTALL_DIR=<fbx-sdk-path> -D FBX_SDK_VERSION=<fbx-sdk-version> -D FBX_SDK_VSTUDIO_VERSION=<vs-version>``
      * ``-B`` will set the path to the generated files.
      * ``-G`` specifies the visual studio generator version.
      * ``-T`` will specify the compiler toolset.
      * ``-D`` is used for setting the cmake variables for the build.
    * Both configuration and generation of the projects files is done using the above command.
* Launch the project in Visual Studio and build the solution. 
* If the build is successful, copy ``dzfbximporter.dll`` from the project output directory. Paste the library to the ``plugins`` folder under Daz Studio install location.
  * This step will replace the existing FBX importer plugin file that is bundled with Daz Studio.
  * Installing Daz Studio will replace this file to the version distributed with Daz Studio.
  * Uninstalling Daz Studio will remove this file.
* Alternatively, the previous step can be automated by setting the ``DAZ_STUDIO_EXE_DIR`` variable while generating the project. In this case, while building the solution, make sure to quit any active instances of Daz Studio.
  * NOTE: Visual Studio may need to be with admin privileges if the ``DAZ_STUDIO_EXE_DIR`` is pointing to a protected path like ``Program Files``.
  * This step will replace the existing FBX importer plugin file that is bundled with Daz Studio.
  * Installing Daz Studio will replace this file to the version distributed with Daz Studio.
  * Uninstalling Daz Studio will remove this file.
* Daz Studio is now installed with the built version of the plugin.

[OwnerURL]: https://www.daz3d.com
[TwitterURL]: https://twitter.com/Daz3d
[LicenseURL]: http://www.apache.org/licenses/LICENSE-2.0
[RepositoryURL]: https://github.com/daz3d/StudioFbxImporter
[DazStudioURL]: https://www.daz3d.com/get_studio
[DazStudioSDKURL]: https://www.daz3d.com/daz-studio-4-5-sdk
