set(TARGET_NAME metavoxels)

auto_mtc()

# use setup_hifi_library macro to setup our project and link appropriate Qt modules
setup_hifi_library(Network Script Widgets)

# link in the networking library
link_hifi_libraries(shared networking)

include_glm()

# call macro to include our dependency includes and bubble them up via a property on our target
include_dependency_includes()