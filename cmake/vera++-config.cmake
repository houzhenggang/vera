if(WIN)
  get_filename_component(VERA++_EXECUTABLE ${CMAKE_CURRENT_LIST_FILE}/../../bin/vera++.exe ABSOLUTE)
else()
  get_filename_component(VERA++_EXECUTABLE ${CMAKE_CURRENT_LIST_FILE}/../../../bin/vera++ ABSOLUTE)
endif()