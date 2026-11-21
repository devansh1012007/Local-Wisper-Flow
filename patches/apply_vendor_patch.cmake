# Idempotent vendor patch applier (see AGENTS.md "Dependencies (offline
# recipe)"). Invoked from FetchContent PATCH_COMMAND as:
#
#   cmake -DPATCH_FILE=<patch> -P patches/apply_vendor_patch.cmake
#
# Behavior:
#   - PATCH_FILE missing from the checkout: status message and success, so a
#     partial checkout still configures (the patch is a build optimization
#     guard, not a correctness requirement for first-pass builds).
#   - Applies with `patch --forward --strip=1` so an already-patched source
#     tree is a no-op instead of an error.
#   - `patch` and `git` both missing: warning, success (best effort).

if(NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "apply_vendor_patch.cmake: PATCH_FILE is not set")
endif()

if(NOT EXISTS "${PATCH_FILE}")
    message(STATUS "apply_vendor_patch: '${PATCH_FILE}' not present; skipping "
                   "(vendor source stays unpatched)")
    return()
endif()

find_program(PATCH_TOOL patch)
find_program(GIT_TOOL git)

if(GIT_TOOL)
    execute_process(
        COMMAND "${GIT_TOOL}" apply --check "${PATCH_FILE}"
        RESULT_VARIABLE git_check
        OUTPUT_QUIET ERROR_QUIET
    )
    if(git_check EQUAL 0)
        execute_process(
            COMMAND "${GIT_TOOL}" apply "${PATCH_FILE}"
            RESULT_VARIABLE git_apply
        )
        if(git_apply EQUAL 0)
            message(STATUS "apply_vendor_patch: applied ${PATCH_FILE} (git)")
            return()
        endif()
    endif()
    # git apply --check failing means already applied or not applicable;
    # fall through to `patch --forward` which treats both idempotently.
endif()

if(PATCH_TOOL)
    execute_process(
        COMMAND "${PATCH_TOOL}" --forward --strip=1 --batch
                --input "${PATCH_FILE}"
        RESULT_VARIABLE patch_result
    )
    if(patch_result EQUAL 0)
        message(STATUS "apply_vendor_patch: applied ${PATCH_FILE} (patch)")
    else()
        message(WARNING "apply_vendor_patch: patch returned ${patch_result} "
                        "(source may already be patched)")
    endif()
    return()
endif()

message(WARNING "apply_vendor_patch: neither git nor patch found; "
                "${PATCH_FILE} was not applied")
