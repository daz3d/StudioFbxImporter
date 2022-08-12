# Daz Studio FBX Importer
* Owner: [Daz 3D][OwnerURL] — [@Daz3d][TwitterURL]
* License: [Apache License, Version 2.0][LicenseURL] - see ``LICENSE`` and ``NOTICE`` for more information.
* Official Project: [github.com/daz3d/StudioFbxImporter][RepositoryURL]

## Prerequisites
* **[Daz Studio][DazStudioURL]** application
  * 4.16.1.17 Public Build or newer
  * 4.20.0.17 General Release or newer
* **Daz Studio SDK**
  * [Version 4.5+][DazStudioSDKURL] or newer
* **FBX SDK**
  * Version 2016 or newer
* Operating System
  * Windows 7 or newer
* **CMake** for building
  * Version 3.4.0 or newer

## Build instructions
* Download and install the **Daz Studio** application.
  * Note the installation path, it _may_ be needed in a later step.
* Download and install the **Daz Studio SDK**.
  * Note the installation path, it _will_ be needed in a later step.
* Download and install the **FBX SDK**.
  * Note the installation path, it _will_ be needed in a later step.
* **CMake** is required to generate project files. 
  * CMake can be used via the GUI tool, or the command line interface.
  * Using cmake-gui:
    * Launch the CMake GUI tool.
    * Click _Browse Source..._ to specify the path where this repository has been cloned locally.
    * Click _Browse Build..._ to specify the path where build files should be created - e.g., ``.../build/x64``.
    * Click _Configure_ to initialize the setup for this project.
      * Choose a generator from the list provided.
      * Optionally choose a platform (e.g., ``x64``) for the generator.
      * In the optional toolset field, enter ``v100`` to indicate use of the Visual Studio 2010 toolset.
        * _NOTE: Other compilers may face memory allocation/de-allocation issues._
      * Click _Finish_ - CMake will begin processing and ultimately throw an error indicating a need to provide a valid path to the Daz Studio SDK.
    * Set the ``DAZ_SDK_DIR`` variable to the **Daz Studio SDK** installation path (as noted above).
    * Set the ``FBX_SDK_INSTALL_DIR`` variable to the **FBX SDK** installation path (as noted above).
    * Set the ``FBX_SDK_VERSION`` and ``FBX_SDK_VSTUDIO_VERSION`` variables to the version of the FBX SDK and corresponding Visual Studio version respectively
      * These variables are used to generate paths to the library files of FBX SDK.
    * Click _Configure_ - the remaining ``FBX_SDK_`` prefixed variables will be automatically populated based on the variables described above.
      * The values of these ``FBX_SDK_`` prefixed variables may need to be adjusted to reflect machine specific configurations if ``FBX_CUSTOM_LAYOUT`` is enabled.
    * Click _Generate_ once the configuration has successfully completed to generate the project.
  * Using cmake command line:
    * ``cmake`` can be run at the root level of the repository with specific options, as shown in the following command:
      * ``cmake -B <build-path> -G "Visual Studio 16" -T v100 -D DAZ_SDK_DIR=<daz-sdk-path> -D FBX_SDK_INSTALL_DIR=<fbx-sdk-path> -D FBX_SDK_VERSION=<fbx-sdk-version> -D FBX_SDK_VSTUDIO_VERSION=<vs-version>``
      * ``-B`` will set the path to the generated files.
      * ``-G`` specifies the Visual Studio generator version.
      * ``-T`` will specify the compiler toolset.
      * ``-D`` is used for setting the cmake variables for the build.
      * Configuration and generation of project files are both performed using the above command.
* Launch the project in Visual Studio and build the solution. 
* If the build is successful, copy ``dzfbximporter.dll`` from the project output directory. Paste the library to the ``plugins`` folder under Daz Studio install location.
  * This step will replace the existing FBX importer plugin file that is bundled with the application.
  * Installing the application again will replace this file to the version distributed with it.
  * Uninstalling the application will remove this file.
* Alternatively, the previous step can be automated by setting the ``DAZ_STUDIO_EXE_DIR`` variable while generating the project. In this case, while building the solution, make sure to quit any active instances of Daz Studio.
  * _NOTE: Visual Studio may need to be run with admin privileges if the ``DAZ_STUDIO_EXE_DIR`` variable is pointing to a protected path like ``Program Files``._
  * This step will replace the existing FBX importer plugin file that is bundled with the application.
  * Installing the application again will replace this file to the version distributed with it.
  * Uninstalling the application will remove this file.
* Daz Studio is now installed with the built version of the plugin.

[OwnerURL]: https://www.daz3d.com
[TwitterURL]: https://twitter.com/Daz3d
[LicenseURL]: http://www.apache.org/licenses/LICENSE-2.0
[RepositoryURL]: https://github.com/daz3d/StudioFbxImporter
[DazStudioURL]: https://www.daz3d.com/get_studio
[DazStudioSDKURL]: https://www.daz3d.com/daz-studio-4-5-sdk
