if(NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "apply_vendor_patch.cmake requires -DPATCH_FILE=<path>")
endif()

find_package(Git REQUIRED)

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${PATCH_FILE}"
    RESULT_VARIABLE already_applied
)
if(already_applied EQUAL 0)
    message(STATUS "[LocalWisperFlow] vendor patch already applied, skipping: ${PATCH_FILE}")
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply "${PATCH_FILE}"
    RESULT_VARIABLE apply_result
)
if(NOT apply_result EQUAL 0)
    message(FATAL_ERROR "[LocalWisperFlow] failed to apply vendor patch: ${PATCH_FILE}")
endif()
message(STATUS "[LocalWisperFlow] applied vendor patch: ${PATCH_FILE}")
