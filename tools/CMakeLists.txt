
file(GLOB APP_SOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} "*.cpp")
foreach(exe_src ${APP_SOURCES})
    string(REPLACE ".cpp" "" exe ${exe_src})
    add_executable(${exe} ${exe_src})

    target_include_directories(${exe}
        PUBLIC #$<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/hcana>
        $<BUILD_INTERFACE:${Podd_DIR}/../../include/podd2>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>  # for hc_compiledata.h
        $<BUILD_INTERFACE:${SPDLOG_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${SPDLOG_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${FMT_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CODA_ET_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${EVIO_INCLUDE_DIR}>
    )

    target_link_libraries(${exe}
    PUBLIC
        ${PROJECT_NAME}::HallC
        Podd::Podd
        Podd::Decode
        coda_et::coda_et
        EVIO::evioxx
    )
    install(TARGETS ${exe} DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach(exe_src ${APP_SOURCES})

