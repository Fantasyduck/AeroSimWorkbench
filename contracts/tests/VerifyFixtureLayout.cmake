if(NOT DEFINED CONTRACT_FIXTURE_ROOT)
    message(FATAL_ERROR "CONTRACT_FIXTURE_ROOT is required")
endif()

set(required_files
    "${CONTRACT_FIXTURE_ROOT}/README.md"
    "${CONTRACT_FIXTURE_ROOT}/legacy/README.md"
    "${CONTRACT_FIXTURE_ROOT}/v2/README.md")

foreach(required_file IN LISTS required_files)
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Missing contract fixture layout file: ${required_file}")
    endif()
endforeach()

file(READ "${CONTRACT_FIXTURE_ROOT}/README.md" fixture_readme)
foreach(required_label IN ITEMS "legacy/" "v2/" "schemaVersion")
    string(FIND "${fixture_readme}" "${required_label}" label_position)
    if(label_position EQUAL -1)
        message(FATAL_ERROR "Fixture README does not document ${required_label}")
    endif()
endforeach()
