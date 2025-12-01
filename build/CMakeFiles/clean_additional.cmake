# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\CSCI-1230-FINAL-PROJECT_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\CSCI-1230-FINAL-PROJECT_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\StaticGLEW_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\StaticGLEW_autogen.dir\\ParseCache.txt"
  "CSCI-1230-FINAL-PROJECT_autogen"
  "StaticGLEW_autogen"
  )
endif()
