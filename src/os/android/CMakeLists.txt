if(ANDROID)
    target_sources(openttd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/android_main.cpp)
    target_link_libraries(openttd android log)
endif()
