cmake_minimum_required(VERSION 3.16...3.21)

project(GeoTest)

option(WITH_GEANT4_UIVIS "Build example with Geant4 UI and Vis drivers" ON)
if(WITH_GEANT4_UIVIS)
  find_package(Geant4 REQUIRED ui_all vis_all)
else()
  find_package(Geant4 REQUIRED)
endif()

find_package(ROOT REQUIRED)

# Find nlohmann_json
find_package(nlohmann_json REQUIRED)

include(${Geant4_USE_FILE})
include(${ROOT_USE_FILE})

include_directories(${PROJECT_SOURCE_DIR}/include)

file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh)

add_executable(GeoTest GeoTest.cc ${sources} ${headers})
target_link_libraries(GeoTest 
    ${Geant4_LIBRARIES} 
    ${ROOT_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Create config directory in build
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/config)

# Copy configuration files
set(CONFIG_FILES

)

foreach(_config ${CONFIG_FILES})
    configure_file(
        ${PROJECT_SOURCE_DIR}/${_config}
        ${PROJECT_BINARY_DIR}/${_config}
        COPYONLY
    )
endforeach()

# Copy visualization macro
set(GEOTEST_SCRIPTS
    run_batch.mac
)

foreach(_script ${GEOTEST_SCRIPTS})
    configure_file(
        ${PROJECT_SOURCE_DIR}/${_script}
        ${PROJECT_BINARY_DIR}/${_script}
        COPYONLY
    )
endforeach()
