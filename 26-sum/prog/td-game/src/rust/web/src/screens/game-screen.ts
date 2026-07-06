import type { Controller } from "../controller.js";
import type { LevelView } from "../../pkg/td_game_rs.js";
import { el } from "../dom.js";
import { Renderer } from "../renderer.js";
import { drawTowerPreview, enemyColor, terrainColor } from "../theme.js";

const ENEMY_KINDS = ["normal", "fast", "armored", "resistant", "splitter", "boss"];
const TILE_KINDS = ["grass", "fertile", "rock", "ice", "portal"];

export class GameScreen {
  readonly root: HTMLElement;
  private active = false;
  private readonly controller: Controller;
  private readonly renderer: Renderer;
  private readonly canvas: HTMLCanvasElement;
  private readonly hudLevel: HTMLElement;
  private readonly hudWave: HTMLElement;
  private readonly hudGold: HTMLElement;
  private readonly hudScore: HTMLElement;
  private readonly hudTime: HTMLElement;
  private readonly hudFps: HTMLElement;
  private readonly pauseBtn: HTMLButtonElement;
  private readonly restartBtn: HTMLButtonElement;
  private readonly quitBtn: HTMLButtonElement;
  private readonly paletteWrap: HTMLElement;
  private readonly insuffLabel: HTMLElement;
  private insuffTimer = 0;
  private readonly onQuit: () => void;
  private readonly keyHandler: (e: KeyboardEvent) => void;

  constructor(controller: Controller, onQuit: () => void) {
    this.controller = controller;
    this.onQuit = onQuit;
    this.canvas = el("canvas", { cls: "game-canvas" });
    this.renderer = new Renderer(this.canvas, controller);

    this.hudLevel = el("span", { cls: "val" });
    this.hudWave = el("span", { cls: "val" });
    this.hudGold = el("span", { cls: "val gold" });
    this.hudScore = el("span", { cls: "val score" });
    this.hudTime = el("span", { cls: "val" });
    this.hudFps = el("span", { cls: "val" });
    this.pauseBtn = el("button", { cls: "btn", text: "Pause" });
    this.restartBtn = el("button", { cls: "btn", text: "Restart" });
    this.quitBtn = el("button", { cls: "btn", text: "Quit" });
    this.paletteWrap = el("div", { cls: "palette" });
    this.insuffLabel = el("div", {
      cls: "insuff",
      text: "Not enough resource!",
    });

    this.root = el("section", { cls: "screen game-screen" }, [
      this.buildHud(),
      el("div", { cls: "game-mid" }, [
        el("div", { cls: "game-stage" }, [this.canvas]),
        el("aside", { cls: "sidebar" }, [this.paletteWrap, this.buildLegend()]),
      ]),
    ]);

    this.pauseBtn.addEventListener("click", () => {
      this.controller.togglePause();
      this.refreshHud();
    });
    this.canvas.addEventListener("click", (e) => this.onClick(e));
    this.canvas.addEventListener("mousemove", (e) => this.onHover(e));
    this.canvas.addEventListener("mouseleave", () => this.renderer.setHover(null));
    this.keyHandler = (e: KeyboardEvent) => this.onKey(e);

    // Wire controller signals (GameScreen owns these; the router owns onEnd).
    controller.onTick = () => {
      this.renderer.render();
      this.refreshHud();
    };
    controller.onStart = () => {
      this.renderer.invalidate();
      this.renderer.render();
      this.refreshHud();
    };
    controller.onLevelStarted = () => {
      this.rebuildPalette();
      this.renderer.invalidate();
    };
    controller.onEvents = (evs) => this.renderer.addEvents(evs);

    this.rebuildPalette();
    this.refreshHud();
  }

  show() {
    this.active = true;
    this.root.classList.remove("hidden");
    window.addEventListener("keydown", this.keyHandler);
  }
  hide() {
    this.active = false;
    this.root.classList.add("hidden");
    window.removeEventListener("keydown", this.keyHandler);
  }

  private buildHud(): HTMLElement {
    const stat = (label: string, val: HTMLElement) =>
      el("div", { cls: "stat" }, [el("span", { cls: "label", text: label }), val]);
    this.restartBtn.addEventListener("click", () => this.controller.restartLevel());
    this.quitBtn.addEventListener("click", this.onQuit);
    return el("header", { cls: "hud" }, [
      stat("Level", this.hudLevel),
      stat("Wave", this.hudWave),
      stat("Gold", this.hudGold),
      stat("Score", this.hudScore),
      stat("Time", this.hudTime),
      stat("FPS", this.hudFps),
      el("div", { cls: "spacer" }),
      this.pauseBtn,
      this.restartBtn,
      this.quitBtn,
    ]);
  }

  private buildLegend(): HTMLElement {
    const swatch = (color: string, round = false) =>
      el("span", {
        cls: "swatch" + (round ? " round" : ""),
        attrs: { style: `background:${color}` },
      });
    const entry = (color: string, text: string, round = false) =>
      el("div", { cls: "entry" }, [swatch(color, round), el("span", { text })]);
    const group = (title: string, items: HTMLElement[]) =>
      el("div", { cls: "legend-group" }, [el("h4", { text: title }), ...items]);
    return el("div", { cls: "legend" }, [
      el("h3", { text: "Legend" }),
      group(
        "Tiles",
        TILE_KINDS.map((k) => entry(terrainColor(k), k)),
      ),
      group(
        "Enemies",
        ENEMY_KINDS.map((k) => entry(enemyColor(k), k, true)),
      ),
      group("Markers", [entry("rgb(30,220,30)", "spawn", true), entry("rgb(220,30,30)", "exit")]),
      group("Status", [
        entry("rgb(52,152,219)", "slow", true),
        entry("rgb(39,174,96)", "poison", true),
        entry("rgb(241,196,15)", "regen", true),
      ]),
    ]);
  }

  private rebuildPalette() {
    this.paletteWrap.innerHTML = "";
    this.paletteWrap.append(el("h3", { text: "Towers" }));
    const lv: LevelView = this.controller.levelView();
    const costs = new Map(lv.towerCosts.map((tc) => [tc.kind, tc.cost]));
    for (const kind of lv.availableTowers) {
      const cost = costs.get(kind) ?? 0;
      const icon = el("canvas", {
        cls: "tower-icon",
        attrs: { width: "22", height: "22" },
      });
      drawTowerPreview(icon.getContext("2d")!, kind, 0, 0, 22);
      const btn = el("button", { cls: "palette-btn" }, [
        icon,
        el("span", { text: `${kind} (${cost})` }),
      ]);
      btn.addEventListener("click", () => {
        if (btn.classList.contains("selected")) {
          btn.classList.remove("selected");
          this.controller.selectTowerType("");
        } else {
          this.paletteWrap
            .querySelectorAll(".palette-btn.selected")
            .forEach((b) => b.classList.remove("selected"));
          btn.classList.add("selected");
          this.controller.selectTowerType(kind);
        }
      });
      this.paletteWrap.append(btn);
    }
    this.paletteWrap.append(this.insuffLabel);
  }

  showInsufficient() {
    this.insuffLabel.classList.add("show");
    clearTimeout(this.insuffTimer);
    this.insuffTimer = window.setTimeout(() => this.insuffLabel.classList.remove("show"), 1500);
  }

  private refreshHud() {
    const c = this.controller;
    this.hudLevel.textContent =
      c.levelIndex() >= 1 ? `${c.levelIndex()}. ${c.levelName()}` : `Custom: ${c.levelName()}`;
    this.hudWave.textContent = String(c.currentWave());
    this.hudGold.textContent = String(c.resourceAmount());
    this.hudScore.textContent = String(c.score());
    this.hudTime.textContent = `${c.elapsedTime().toFixed(1)}s`;
    this.hudFps.textContent = c.fps().toFixed(0);
    this.pauseBtn.textContent = c.paused() ? "Resume" : "Pause";
    // Hide all three control buttons when the game ends (mirrors Qt's
    // setControlsVisible(false) — the result card provides its own buttons).
    const playing = c.state() === "playing";
    this.pauseBtn.style.visibility = playing ? "" : "hidden";
    this.restartBtn.style.visibility = playing ? "" : "hidden";
    this.quitBtn.style.visibility = playing ? "" : "hidden";
  }

  private onClick(e: MouseEvent) {
    const res = this.controller.placeTower(this.renderer.worldFromEvent(e));
    if (!res.ok && res.reason === "not_enough_resource") this.showInsufficient();
  }

  private onHover(e: MouseEvent) {
    this.renderer.setHover(this.renderer.worldFromEvent(e));
  }

  private onKey(e: KeyboardEvent) {
    if (!this.active) return;
    switch (e.key) {
      case " ":
        e.preventDefault();
        this.controller.togglePause();
        this.refreshHud();
        break;
      case "r":
      case "R":
        this.controller.restartLevel();
        break;
      case "g":
      case "G":
        this.controller.applyCheat("gold");
        break;
      case "k":
      case "K":
        this.controller.applyCheat("killall");
        break;
      case "w":
      case "W":
        this.controller.applyCheat("win");
        break;
    }
  }
}
