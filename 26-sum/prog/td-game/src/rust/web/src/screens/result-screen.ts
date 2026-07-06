import { el } from "../dom.js";

export interface ResultOptions {
  victory: boolean;
  score: number;
  time: number;
  hasNext: boolean;
  fromEditor: boolean;
}

/// Shared victory/defeat card. `onPrimary` = Next Level (victory) or Retry (defeat);
/// `onMenu` = back to menu (or editor if fromEditor).
export function createResultScreen(
  opts: ResultOptions,
  onPrimary: () => void,
  onMenu: () => void,
): HTMLElement {
  const buttons = el("div", { cls: "row" });
  if (opts.victory && opts.hasNext && !opts.fromEditor) {
    buttons.append(
      el("button", {
        cls: "btn primary",
        text: "Next Level",
        on: { click: onPrimary },
      }),
    );
  } else if (!opts.victory) {
    buttons.append(
      el("button", {
        cls: "btn primary",
        text: "Retry Level",
        on: { click: onPrimary },
      }),
    );
  }
  buttons.append(
    el("button", {
      cls: "btn",
      text: opts.fromEditor ? "Back to Editor" : "Main Menu",
      on: { click: onMenu },
    }),
  );
  return el("section", { cls: `screen result ${opts.victory ? "victory" : "defeat"}` }, [
    el("div", { cls: "card" }, [
      el("h2", { text: opts.victory ? "Level Cleared!" : "Defeated" }),
      el("p", {
        cls: "summary",
        text: `Score: ${opts.score}   Time: ${opts.time.toFixed(1)}s`,
      }),
      buttons,
    ]),
  ]);
}
