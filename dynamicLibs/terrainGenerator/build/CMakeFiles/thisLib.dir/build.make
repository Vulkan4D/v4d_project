# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.17

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/build

# Include any dependencies generated for this target.
include CMakeFiles/thisLib.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/thisLib.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/thisLib.dir/flags.make

CMakeFiles/thisLib.dir/terrainGenerator.cpp.o: CMakeFiles/thisLib.dir/flags.make
CMakeFiles/thisLib.dir/terrainGenerator.cpp.o: ../terrainGenerator.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/thisLib.dir/terrainGenerator.cpp.o"
	/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/thisLib.dir/terrainGenerator.cpp.o -c /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/terrainGenerator.cpp

CMakeFiles/thisLib.dir/terrainGenerator.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/thisLib.dir/terrainGenerator.cpp.i"
	/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/terrainGenerator.cpp > CMakeFiles/thisLib.dir/terrainGenerator.cpp.i

CMakeFiles/thisLib.dir/terrainGenerator.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/thisLib.dir/terrainGenerator.cpp.s"
	/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/terrainGenerator.cpp -o CMakeFiles/thisLib.dir/terrainGenerator.cpp.s

# Object files for target thisLib
thisLib_OBJECTS = \
"CMakeFiles/thisLib.dir/terrainGenerator.cpp.o"

# External object files for target thisLib
thisLib_EXTERNAL_OBJECTS =

thisLib.so: CMakeFiles/thisLib.dir/terrainGenerator.cpp.o
thisLib.so: CMakeFiles/thisLib.dir/build.make
thisLib.so: CMakeFiles/thisLib.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX shared library thisLib.so"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/thisLib.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/thisLib.dir/build: thisLib.so

.PHONY : CMakeFiles/thisLib.dir/build

CMakeFiles/thisLib.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/thisLib.dir/cmake_clean.cmake
.PHONY : CMakeFiles/thisLib.dir/clean

CMakeFiles/thisLib.dir/depend:
	cd /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/build /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/build /home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/build/CMakeFiles/thisLib.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/thisLib.dir/depend

