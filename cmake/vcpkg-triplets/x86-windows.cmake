set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# Some Windows SDKs (for example 10.0.28000) ship an rc.exe that cannot compile
# the manifest resource CMake generates while checking the compiler.  The
# dependencies do not need an embedded manifest, so skip it.
set(VCPKG_LINKER_FLAGS "/MANIFEST:NO")
