import "./style.css";
import init, { WebApp } from "../pkg/td_game_rs.js";
import { App } from "./app.js";
import { Controller } from "./controller.js";

// Fetch + compile the .wasm. Must finish before any WebApp call.
await init();

const app = new WebApp();
const controller = new Controller(app);
const root = document.querySelector<HTMLDivElement>("#app")!;
new App(controller, root);
