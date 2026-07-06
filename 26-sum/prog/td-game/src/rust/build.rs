fn main() {
    // The cxx C++ bridge is only built for the cxx-bridge feature (the Qt/CMake
    // path). Excluded for the wasm `web` build, where `src/cxxbridge.rs` is absent.
    #[cfg(feature = "cxx-bridge")]
    {
        cxx_build::bridge("src/cxxbridge.rs").compile("td-game-rs");
        println!("cargo:rerun-if-changed=src/cxxbridge.rs");
    }
}
