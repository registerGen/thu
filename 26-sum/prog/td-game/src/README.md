# td-game

A 2D grid-based tower defense game with three build paths sharing one codebase:

| Path | Model core | Frontend | Binary |
|------|-----------|----------|--------|
| **C++ + Qt** | `game/` (C++) | `app/` (Qt widgets) | `td` |
| **Rust + Qt** | `rust/` (Rust, via cxx bridge) | `app/` (same Qt widgets) | `td-rs` |
| **Rust + Web** | `rust/` (Rust, via wasm-bindgen) | `rust/web/` (TypeScript + Canvas) | static site |

## Architecture

```
src/
├── game/          C++ model core (towers, enemies, waves, pathfinding, config)
├── rust/          Rust model core — a framework-agnostic rewrite of game/
│   ├── src/
│   │   ├── cxxbridge.rs   cxx bridge → C++ (feature: cxx-bridge)
│   │   ├── web.rs         wasm-bindgen adapter → JS (feature: web)
│   │   └── ...            shared core (game, enemy, tower, wave, level, ...)
│   └── web/               Vite + TypeScript frontend (mirrors the Qt app)
├── app/           Qt frontend (MainWindow, GameWidget, Screens, GameController)
│   ├── GameModel.h        abstract model interface
│   ├── CppGameModel       impl backed by game/ (C++ model)
│   └── RustGameModel      impl backed by rust/ via cxx
├── config/        Level JSONs, tower/enemy stats (towers.json, enemies.json)
└── tests/         Catch2 tests for the C++ model
```

**Model/view separation:** both frontends (Qt and Web) talk to the model through
the same conceptual interface (`GameModel` in C++, `WebApp` in JS). The model
never knows how it's rendered. The Rust core is one crate with two FFI adapters
(`cxxbridge.rs` for C++, `web.rs` for JS), feature-gated so they don't interfere.

**Config:** the Qt app reads `config/` from disk at runtime. The web build embeds
the same files at compile time via `include_str!`.

## Prerequisites

- **C++ / Rust + Qt:** CMake 3.20+, a C++20 compiler, Qt 6 (Widgets), and
  (for the Rust path) the Rust toolchain + `wasm32-unknown-unknown` target if
  also building web.
- **Web:** Rust toolchain, `wasm-pack`, Node.js 20+, and pnpm.

## Build: C++ + Qt (default)

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=<path-to-qt6>
cmake --build build -j
./build/bin/td
```

Tests (Catch2, exercise the C++ model):

```sh
cmake -S . -B build-tests -DTD_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=<path-to-qt6>
cmake --build build-tests -j && ctest --test-dir build-tests
```

## Build: Rust + Qt

```sh
cmake -S . -B build-rust -DTD_USE_RUST_MODEL=ON -DCMAKE_PREFIX_PATH=<path-to-qt6>
cmake --build build-rust -j
./build-rust/bin/td-rs
```

Corrosion fetches and builds the Rust crate with the `cxx-bridge` feature
(`NO_DEFAULT_FEATURES` + `FEATURES cxx-bridge` in `rust/CMakeLists.txt`).
The cxx codegen generates `td_game_rs_cxxbridge/cxxbridge.h`, which
`app/RustGameModel.cpp` includes to call the Rust model.

## Build: Rust + Web

```sh
cd rust/web
pnpm install
pnpm dev      # Vite dev server with HMR (http://localhost:5173)
```

Production build:

```sh
pnpm build    # outputs dist/ (static HTML + JS + .wasm)
```

`pnpm wasm` rebuilds the Wasm package (run after changing Rust code during dev).
`pnpm build` runs `wasm` + `tsc` + `vite build` in sequence.

### Deploy to GitHub Pages

A GitHub Actions workflow (`.github/workflows/deploy.yml`) builds and deploys on
push to `main`. The app is served at
`https://registergen.github.io/thu/tower-defense/`.

One-time setup: repo Settings → Pages → Source: **GitHub Actions**.
