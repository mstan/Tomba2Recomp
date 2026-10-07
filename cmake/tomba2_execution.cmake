# Both builds retain the original generated game. ENHANCED installs the
# verified USA dispatch adapter when the resident-loading product mod is on;
# REFERENCE registers that catalog callback without intercepting the loader.
function(tomba2_add_loading_implementation target)
    set(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    psxrecomp_add_implementation(${target}
        NAME tomba2-resident-loader CONTRACT scus94454-resource-worker-v1
        HLE_SOURCES
            "${_root}/src/mods/tomba2_seamless.c"
            "${_root}/src/mods/tomba2_seamless_prepare.cpp"
        LLE_SOURCES "${_root}/src/mods/tomba2_seamless_reference.c"
        CONTRACT_FILES
            "${_root}/src/mods/tomba2_seamless_catalog.inc"
            "${_root}/src/mods/tomba2_seamless_textures.inc"
            "${_root}/docs/SEAMLESS_LOADING_SPIKE.md")
endfunction()
