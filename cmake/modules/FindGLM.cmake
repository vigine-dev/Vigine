# Locate the glm library
# This module defines
#  GLM_INCLUDE_DIRS, where to find glm headers
#  GLM_LIBRARIES, the libraries to link against to use glm

find_path(GLM_INCLUDE_DIR glm/glm.hpp
   PATHS
   ${CMAKE_SOURCE_DIR}/external/glm
   NO_DEFAULT_PATH
)

if(GLM_INCLUDE_DIR)
   set(GLM_FOUND TRUE)
   set(GLM_LIBRARIES glm)
   set(GLM_INCLUDE_DIRS ${GLM_INCLUDE_DIR})

   add_subdirectory(${CMAKE_SOURCE_DIR}/external/glm)

else()

   set(GLM_FOUND FALSE)
   message(FATAL_ERROR "GLM не знайдено в external/glm. Завантажте його вручну.")

endif()

mark_as_advanced(GLM_INCLUDE_DIRS GLM_LIBRARIES)
