import { defineConfig } from "vite";

// GitHub Pages project site: the app is served at
// https://registergen.github.io/thu/tower-defense/ so asset URLs must be
// prefixed with /thu/tower-defense/.
export default defineConfig({
  base: "/thu/tower-defense/",
});
