import type { Controller } from "../controller.js";
import { el } from "../dom.js";
import { clearProgress, getProgress } from "../progress.js";
import { enemyColor, isRectTower, terrainColor, towerColor } from "../theme.js";

const TOWERS: [string, string][] = [
  ["normal", "single-target damage"],
  ["slow", "slows enemies"],
  ["poison", "damages + roots"],
  ["splash", "area damage"],
  ["laser", "piercing ray"],
  ["resource", "generates gold"],
  ["wall", "blocks enemies (high HP)"],
];
const ENEMIES: [string, string][] = [
  ["normal", "standard"],
  ["fast", "low HP, high speed"],
  ["armored", "high HP, slow"],
  ["resistant", "shrugs slow & splash"],
  ["splitter", "spawns children on death"],
  ["boss", "shield + regen"],
];
const TILES: [string, string][] = [
  ["grass", "buildable"],
  ["fertile", "cheaper towers"],
  ["rock", "blocked"],
  ["ice", "enemies faster, slow stronger"],
  ["portal", "teleports"],
];

function swatch(color: string, round = false): HTMLElement {
  return el("span", {
    cls: "swatch" + (round ? " round" : ""),
    attrs: { style: `background:${color}` },
  });
}

function section(
  title: string,
  entries: [string, string][],
  color: (k: string) => string,
  shape: (k: string) => boolean = () => false,
): HTMLElement {
  const wrap = el("div", { cls: "help-section" });
  wrap.append(el("h3", { text: title }));
  for (const [k, desc] of entries) {
    wrap.append(
      el("div", { cls: "entry" }, [
        swatch(color(k), shape(k)),
        el("span", { text: `${k} — ${desc}` }),
      ]),
    );
  }
  return wrap;
}

function buildHelp(): HTMLElement {
  const help = el("section", { cls: "help" });
  help.append(el("h2", { text: "How to Play" }));
  help.append(
    section(
      "Towers (pick from palette, then click the map)",
      TOWERS,
      towerColor,
      (k) => !isRectTower(k),
    ),
  );
  help.append(section("Enemies", ENEMIES, enemyColor, () => true));
  help.append(section("Tiles", TILES, terrainColor));
  help.append(
    el("div", { cls: "help-section" }, [
      el("h3", { text: "Markers" }),
      el("div", { cls: "entry" }, [
        swatch("rgb(30,220,30)", true),
        el("span", { text: "spawn — where enemies appear" }),
      ]),
      el("div", { cls: "entry" }, [
        swatch("rgb(220,30,30)"),
        el("span", { text: "exit — enemies reaching it defeat you" }),
      ]),
    ]),
  );
  help.append(
    el("div", { cls: "help-section" }, [
      el("h3", { text: "Status effects" }),
      el("div", { cls: "entry" }, [
        swatch("rgb(52,152,219)", true),
        el("span", { text: "slow — moving slower" }),
      ]),
      el("div", { cls: "entry" }, [
        swatch("rgb(39,174,96)", true),
        el("span", { text: "poison — taking damage" }),
      ]),
      el("div", { cls: "entry" }, [
        swatch("rgb(241,196,15)", true),
        el("span", { text: "regen — healing over time" }),
      ]),
    ]),
  );
  help.append(
    el("div", { cls: "help-section" }, [
      el("h3", { text: "Controls" }),
      el("p", {
        text: "Click a tower button then click the map to build. Space = pause/resume, R = restart.",
      }),
      el("h3", { text: "Cheats" }),
      el("p", {
        text: "G = +1000 gold, K = kill all enemies, W = instant win.",
      }),
    ]),
  );
  return help;
}

export interface StartMenu {
  root: HTMLElement;
  refresh: () => void;
}

export function createStartMenu(
  controller: Controller,
  onPlay: (slot: number) => void,
  onEditor: () => void,
): StartMenu {
  const root = el("section", { cls: "screen start-menu" });
  root.append(el("h1", { text: "Tower Defense" }), el("h2", { text: "Levels" }));
  const listWrap = el("div");
  root.append(listWrap);
  root.append(
    el("div", { cls: "row menu-actions" }, [
      el("button", {
        cls: "btn primary",
        text: "Level Editor",
        on: { click: onEditor },
      }),
      el("button", {
        cls: "btn",
        text: "Clear Progress",
        on: {
          click: () => {
            clearProgress();
            renderLevels();
          },
        },
      }),
    ]),
  );
  root.append(buildHelp());
  root.append(
    el("footer", { cls: "footnote" }, [
      el("a", {
        text: "Source code",
        attrs: {
          href: "https://github.com/registerGen/thu/tree/main/26-sum/prog/td-game/src",
          target: "_blank",
        },
      }),
      el("span", { text: " · MIT License · © 2026 registerGen · " }),
      el("a", {
        text: "Life in THU",
        attrs: {
          href: "https://registergen.github.io/thu/",
          target: "_blank",
        },
      }),
    ]),
  );

  function renderLevels() {
    listWrap.innerHTML = "";
    const list = el("div", { cls: "level-list" });
    list.append(
      el("div", { cls: "level-row level-header" }, [
        el("span", { text: "Level" }),
        el("span", { text: "Cleared" }),
        el("span", { text: "Max Score" }),
      ]),
    );
    controller.registryInfos().forEach((info, slot) => {
      const prog = getProgress(info.index);
      list.append(
        el("div", { cls: "level-row" }, [
          el("button", {
            cls: "level-btn",
            text: info.index >= 1 ? `Level ${info.index}: ${info.name}` : `Custom: ${info.name}`,
            on: { click: () => onPlay(slot) },
          }),
          el("span", {
            cls: prog.cleared ? "yes" : "no",
            text: prog.cleared ? "✓" : "—",
          }),
          el("span", {
            text: prog.maxScore > 0 ? String(prog.maxScore) : "—",
          }),
        ]),
      );
    });
    listWrap.append(list);
  }
  renderLevels();

  return { root, refresh: renderLevels };
}
