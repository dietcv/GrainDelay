# Build the GrainDelay plugin
add_library(GrainDelay MODULE
    GrainDelay.cpp
)

# platform-specific linking
if(APPLE)
    target_link_libraries(GrainDelay "-undefined dynamic_lookup")
endif()