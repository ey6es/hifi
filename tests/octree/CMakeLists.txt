set(TARGET_NAME octree-tests)

setup_hifi_project(Script Network)

include_glm()

# link in the shared libraries
link_hifi_libraries(shared octree gpu model fbx metavoxels networking entities avatars audio animation script-engine physics)

include_dependency_includes()
