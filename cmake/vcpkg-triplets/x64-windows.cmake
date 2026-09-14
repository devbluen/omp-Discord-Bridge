set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# Host tools used by vcpkg are built with this triplet.  See x86-windows.cmake
# for why manifests are disabled.
set(VCPKG_LINKER_FLAGS "/MANIFEST:NO")
