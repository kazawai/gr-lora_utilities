# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.29

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

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
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
CMAKE_SOURCE_DIR = /home/kazawai/dev/C++/LoRa/gr-first_lora

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/kazawai/dev/C++/LoRa/gr-first_lora/build

# Utility rule file for copy_module_for_tests.

# Include any custom commands dependencies for this target.
include python/first_lora/CMakeFiles/copy_module_for_tests.dir/compiler_depend.make

# Include the progress variables for this target.
include python/first_lora/CMakeFiles/copy_module_for_tests.dir/progress.make

python/first_lora/CMakeFiles/copy_module_for_tests:
	cd /home/kazawai/dev/C++/LoRa/gr-first_lora/build/python/first_lora && /usr/bin/cmake -E copy_directory /home/kazawai/dev/C++/LoRa/gr-first_lora/python/first_lora /home/kazawai/dev/C++/LoRa/gr-first_lora/build/test_modules/gnuradio/first_lora/

copy_module_for_tests: python/first_lora/CMakeFiles/copy_module_for_tests
copy_module_for_tests: python/first_lora/CMakeFiles/copy_module_for_tests.dir/build.make
.PHONY : copy_module_for_tests

# Rule to build all files generated by this target.
python/first_lora/CMakeFiles/copy_module_for_tests.dir/build: copy_module_for_tests
.PHONY : python/first_lora/CMakeFiles/copy_module_for_tests.dir/build

python/first_lora/CMakeFiles/copy_module_for_tests.dir/clean:
	cd /home/kazawai/dev/C++/LoRa/gr-first_lora/build/python/first_lora && $(CMAKE_COMMAND) -P CMakeFiles/copy_module_for_tests.dir/cmake_clean.cmake
.PHONY : python/first_lora/CMakeFiles/copy_module_for_tests.dir/clean

python/first_lora/CMakeFiles/copy_module_for_tests.dir/depend:
	cd /home/kazawai/dev/C++/LoRa/gr-first_lora/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/kazawai/dev/C++/LoRa/gr-first_lora /home/kazawai/dev/C++/LoRa/gr-first_lora/python/first_lora /home/kazawai/dev/C++/LoRa/gr-first_lora/build /home/kazawai/dev/C++/LoRa/gr-first_lora/build/python/first_lora /home/kazawai/dev/C++/LoRa/gr-first_lora/build/python/first_lora/CMakeFiles/copy_module_for_tests.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : python/first_lora/CMakeFiles/copy_module_for_tests.dir/depend

