#!/bin/bash

# Check if an argument was provided for the folder name
if [ $# -eq 0 ]; then
  # No argument was provided, generate a unique folder name
  id=$(shuf -i 1000-9999 -n 1)
  folder_name="build_$id"
else
  # Use the provided argument as the folder name
  folder_name=$1
fi

# Create the build folder
mkdir "$folder_name"

# Change directory into the build folder
cd "$folder_name"

# Setup the build environment
cmake -G "Visual Studio 16 2019" -A x64 -T host=x64 ..

# Build the project
cmake --build . --config Release

# Open the Windows file explorer in the VideoPostProcessExample directory inside the build folder
explorer.exe "$(pwd)"/bin"
