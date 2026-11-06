set(AEROSIM_CONTRACT_HEADER_COMPILE_SOURCES
    PublicHeadersCompileMain.cpp
    compile/DataAccessHeader.cpp
    compile/IntegrationPortsHeader.cpp
    compile/PostProcessRefsHeader.cpp
    compile/ReferencesHeader.cpp
    compile/TaskModelsHeader.cpp
    compile/TaskPortsHeader.cpp
    compile/TelemetryHeader.cpp
    compile/WorkflowModelsHeader.cpp
)

add_executable(aerosim_contract_headers_compile
    ${AEROSIM_CONTRACT_HEADER_COMPILE_SOURCES})
target_link_libraries(aerosim_contract_headers_compile PRIVATE aerosim_contracts)
add_test(NAME contracts_public_headers_compile
    COMMAND aerosim_contract_headers_compile)

add_executable(aerosim_contract_legacy_ref_shape_test
    LegacyReferenceShapeTest.cpp)
target_link_libraries(aerosim_contract_legacy_ref_shape_test PRIVATE aerosim_contracts)
add_test(NAME contracts_legacy_ref_shape
    COMMAND aerosim_contract_legacy_ref_shape_test)

add_executable(aerosim_contract_enum_tokens_test
    StableEnumTokenManifestTest.cpp)
target_link_libraries(aerosim_contract_enum_tokens_test PRIVATE aerosim_contracts)
add_test(NAME contracts_stable_enum_token_manifest
    COMMAND aerosim_contract_enum_tokens_test)

add_test(NAME contracts_migration_fixture_layout
    COMMAND "${CMAKE_COMMAND}"
            "-DCONTRACT_FIXTURE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/VerifyFixtureLayout.cmake")
