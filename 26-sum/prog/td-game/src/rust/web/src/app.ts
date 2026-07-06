import type { Controller } from "./controller.js";
import type { GameState } from "../pkg/td_game_rs.js";
import { el } from "./dom.js";
import { recordResult } from "./progress.js";
import { EditorScreen } from "./screens/editor.js";
import { createResultScreen } from "./screens/result-screen.js";
import { GameScreen } from "./screens/game-screen.js";
import { createStartMenu } from "./screens/start-menu.js";

const BANNER_DELAY = 2000;

type ScreenName = "menu" | "playing" | "editor";

/// Top-level app: owns the controller + screens and routes between them.
/// Mirrors Qt's MainWindow + QStackedWidget.
export class App {
  private readonly controller: Controller;
  private readonly root: HTMLElement;
  private readonly menu: ReturnType<typeof createStartMenu>;
  private readonly game: GameScreen;
  private readonly editor: EditorScreen;
  private fromEditor = false;
  private endTimer = 0;
  private resultCard: HTMLElement | null = null;

  constructor(controller: Controller, mount: HTMLElement) {
    this.controller = controller;
    this.root = el("div", { cls: "app" });
    mount.append(this.root);

    this.menu = createStartMenu(
      controller,
      (slot) => this.play(slot),
      () => this.show("editor"),
    );
    this.game = new GameScreen(controller, () => this.quitGame());
    this.editor = new EditorScreen(
      (json) => this.playJson(json),
      () => this.show("menu"),
      (json) => this.controller.validateLevelJson(json),
    );

    this.root.append(this.menu.root, this.game.root, this.editor.root);
    this.game.hide();
    this.editor.hide();

    controller.onEnd = (state) => this.onEnd(state);
    this.show("menu");
  }

  private show(name: ScreenName) {
    this.removeResult();
    this.menu.root.classList.toggle("hidden", name !== "menu");
    this.game.root.classList.toggle("hidden", name !== "playing");
    this.editor.root.classList.toggle("hidden", name !== "editor");
    if (name === "playing") this.game.show();
    else this.game.hide();
    if (name === "editor") this.editor.show();
    else this.editor.hide();
  }

  private play(slot: number) {
    this.fromEditor = false;
    this.controller.selectAndStartLevel(slot);
    this.show("playing");
    this.controller.start();
  }

  private playJson(json: string) {
    const err = this.controller.startLevelJson(json);
    if (err) {
      alert(err);
      return;
    }
    this.fromEditor = true;
    this.show("playing");
    this.controller.start();
  }

  private toMenu() {
    this.controller.stop();
    this.menu.refresh();
    this.show("menu");
  }

  /// Quit from gameplay: back to the editor if the level came from there,
  /// otherwise back to the start menu.
  private quitGame() {
    this.controller.stop();
    if (this.fromEditor) this.show("editor");
    else {
      this.menu.refresh();
      this.show("menu");
    }
  }

  private onEnd(state: GameState) {
    const result = this.controller.lastResult();
    const official = this.controller.levelIndex() >= 1;
    if (official)
      recordResult(this.controller.levelIndex(), result.score, result.cheated, state === "victory");
    clearTimeout(this.endTimer);
    this.endTimer = window.setTimeout(() => this.showResult(state === "victory"), BANNER_DELAY);
  }

  private showResult(victory: boolean) {
    this.removeResult();
    this.controller.stop();
    const result = this.controller.lastResult();
    const fromEditor = this.fromEditor;
    const card = createResultScreen(
      {
        victory,
        score: result.score,
        time: result.time,
        hasNext: this.controller.hasNextLevel(),
        fromEditor,
      },
      () => {
        this.removeResult();
        if (victory) this.controller.nextLevel();
        else this.controller.restartLevel();
        this.show("playing");
        this.controller.start();
      },
      () => {
        this.removeResult();
        if (fromEditor) this.show("editor");
        else this.toMenu();
      },
    );
    this.resultCard = card;
    this.menu.root.classList.add("hidden");
    this.game.root.classList.add("hidden");
    this.editor.root.classList.add("hidden");
    this.game.hide();
    this.editor.hide();
    this.root.append(card);
  }

  private removeResult() {
    if (this.resultCard) {
      this.resultCard.remove();
      this.resultCard = null;
    }
  }
}
