import { el } from "../dom.js";
import {
  ALL_TOWERS,
  TERRAINS,
  defaultState,
  generateWaves,
  parse,
  serialize,
  type EditorState,
} from "../level-json.js";
import { terrainColor } from "../theme.js";

const ENEMY_KINDS = ["normal", "fast", "armored", "resistant", "splitter", "boss"];
type Tool = "terrain" | "path" | "portal";

function sanitize(name: string): string {
  const s = name
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");
  return s || "custom";
}

export class EditorScreen {
  readonly root: HTMLElement;
  private state: EditorState;
  private tool: Tool = "terrain";
  private selectedTerrain = "grass";
  private activePath = 0;
  private portalFirst = -1;
  private dragging = false;
  private hover: [number, number] | null = null;
  private tile = 48;
  private layoutDirty = true;
  private ro: ResizeObserver | null = null;
  private readonly canvas: HTMLCanvasElement;
  private readonly ctx: CanvasRenderingContext2D;
  private readonly onPlay: (json: string) => void;
  private readonly onBack: () => void;
  private readonly onValidate: (json: string) => string | null;
  private readonly status: HTMLElement;
  private readonly pathSelect: HTMLSelectElement;
  private readonly wavesWrap: HTMLElement;
  private readonly nameInput: HTMLInputElement;

  constructor(
    onPlay: (json: string) => void,
    onBack: () => void,
    onValidate: (json: string) => string | null,
  ) {
    this.onPlay = onPlay;
    this.onBack = onBack;
    this.onValidate = onValidate;
    this.state = defaultState();
    this.canvas = el("canvas", { cls: "editor-canvas" });
    this.ctx = this.canvas.getContext("2d")!;
    this.status = el("div", { cls: "status" });
    this.pathSelect = el("select");
    this.wavesWrap = el("div", { cls: "waves" });
    this.nameInput = el("input", { attrs: { value: this.state.name } });

    this.root = el("section", { cls: "screen editor" }, [
      this.buildHud(),
      el("div", { cls: "editor-body" }, [
        el("div", { cls: "editor-stage" }, [this.canvas]),
        this.buildToolbox(),
      ]),
    ]);

    this.nameInput.addEventListener("input", () => {
      this.state.name = this.nameInput.value;
    });
    this.canvas.addEventListener("mousedown", (e) => this.onDown(e));
    this.canvas.addEventListener("mousemove", (e) => this.onMove(e));
    window.addEventListener("mouseup", () => {
      this.dragging = false;
    });

    this.rebuildWaves();
    this.render();
    this.setStatus("Paint terrain, draw a path (2+ waypoints), set waves, then Play to test.");
  }

  show() {
    this.root.classList.remove("hidden");
    this.layoutDirty = true;
    this.render();
  }
  hide() {
    this.root.classList.add("hidden");
  }

  // --- toolbox ---
  private buildToolbox(): HTMLElement {
    const box = el("div", { cls: "toolbox" });

    box.append(el("h3", { text: "Level" }));
    box.append(this.nameInput);

    box.append(el("h3", { text: "Tool" }));
    const toolGroup = el("div", { cls: "btn-group wrap" });
    const selectTool = (btn: HTMLElement) => {
      toolGroup.querySelectorAll(".chip").forEach((c) => c.classList.remove("selected"));
      btn.classList.add("selected");
    };
    for (const t of TERRAINS) {
      const b = el("button", {
        cls: "chip",
        text: t,
      });
      b.addEventListener("click", () => {
        this.selectedTerrain = t;
        this.tool = "terrain";
        selectTool(b);
        this.setStatus(`Terrain: ${t} — drag to paint.`);
      });
      toolGroup.append(b);
    }
    for (const t of ["path", "portal"] as Tool[]) {
      const b = el("button", {
        cls: "chip",
        text: t,
      });
      b.addEventListener("click", () => {
        this.tool = t;
        this.portalFirst = -1;
        selectTool(b);
        this.setStatus(
          t === "path"
            ? "Path mode — click to add waypoints."
            : "Portal mode — click two waypoints to pair.",
        );
      });
      toolGroup.append(b);
    }
    box.append(toolGroup);

    box.append(el("h3", { text: "Path" }));
    this.pathSelect.addEventListener("change", () => {
      this.activePath = Number(this.pathSelect.value);
      this.portalFirst = -1;
      this.setStatus(`Switched to path ${this.activePath + 1}.`);
      this.render();
    });
    const addPath = el("button", { cls: "btn", text: "Add" });
    addPath.addEventListener("click", () => {
      this.state.paths.push({ waypoints: [], portals: [] });
      this.activePath = this.state.paths.length - 1;
      this.regenWaves();
      this.refreshPathSelect();
      this.setStatus(`Added path ${this.state.paths.length}.`);
      this.render();
    });
    const delPath = el("button", { cls: "btn", text: "Del" });
    delPath.addEventListener("click", () => {
      if (this.state.paths.length <= 1) {
        this.setStatus("Can't delete the last path.");
        return;
      }
      this.state.paths.splice(this.activePath, 1);
      this.activePath = Math.min(this.activePath, this.state.paths.length - 1);
      this.regenWaves();
      this.refreshPathSelect();
      this.setStatus("Path deleted.");
      this.render();
    });
    const clearPath = el("button", { cls: "btn", text: "Clear path" });
    clearPath.addEventListener("click", () => {
      this.state.paths[this.activePath] = { waypoints: [], portals: [] };
      this.render();
      this.setStatus(`Cleared path ${this.activePath + 1}.`);
    });
    box.append(el("div", { cls: "row" }, [this.pathSelect, addPath, delPath, clearPath]));
    this.refreshPathSelect();

    box.append(el("h3", { text: "Map Size" }));
    const rowsSpin = this.spinner("Rows", this.state.rows, 5, 10, (v) => {
      this.resize(v, this.state.cols);
      this.setStatus(`Rows: ${v}.`);
    });
    const colsSpin = this.spinner("Cols", this.state.cols, 9, 16, (v) => {
      this.resize(this.state.rows, v);
      this.setStatus(`Cols: ${v}.`);
    });
    box.append(el("div", { cls: "row" }, [rowsSpin, colsSpin]));

    box.append(el("h3", { text: "Economy" }));
    const goldSpin = this.spinner("Gold", this.state.gold, 50, 999, (v) => {
      this.state.gold = v;
      this.setStatus(`Starting gold: ${v}.`);
    });
    const amtSpin = this.spinner("Auto", this.state.autoAmt, 0, 20, (v) => {
      this.state.autoAmt = v;
      this.setStatus(`Auto-inc amount: ${v}.`);
    });
    const intSpin = this.spinner("Int", this.state.autoInt, 1, 30, (v) => {
      this.state.autoInt = v;
      this.setStatus(`Auto-inc interval: ${v}s.`);
    });
    box.append(el("div", { cls: "row" }, [goldSpin, amtSpin, intSpin]));

    box.append(el("h3", { text: "Towers" }));
    const towerGroup = el("div", { cls: "btn-group wrap" });
    for (const t of ALL_TOWERS) {
      const c = el("button", { cls: "chip selected", text: t });
      c.addEventListener("click", () => {
        const on = c.classList.toggle("selected");
        if (on) this.state.towers.push(t);
        else this.state.towers = this.state.towers.filter((x) => x !== t);
        this.setStatus(`${t} ${on ? "enabled" : "disabled"}.`);
      });
      towerGroup.append(c);
    }
    box.append(towerGroup);

    box.append(el("h3", { text: "Waves" }));
    const preset = el("select");
    for (const d of ["easy", "medium", "hard"]) {
      preset.append(el("option", { text: d, attrs: { value: d } }));
    }
    const genBtn = el("button", { cls: "btn", text: "Generate" });
    genBtn.addEventListener("click", () => {
      this.state.waves = generateWaves(
        preset.value as "easy" | "medium" | "hard",
        this.usablePaths(),
      );
      this.rebuildWaves();
      this.setStatus(`Generated ${this.state.waves.length} waves (${preset.value}).`);
    });
    box.append(el("div", { cls: "row" }, [preset, genBtn]));
    box.append(this.wavesWrap);

    return box;
  }

  /// Top HUD mirroring the gameplay HUD: prompt message left, action buttons right.
  private buildHud(): HTMLElement {
    const playBtn = el("button", { cls: "btn primary", text: "Play" });
    playBtn.addEventListener("click", () => this.onPlayClick());
    const exportBtn = el("button", { cls: "btn", text: "Export" });
    exportBtn.addEventListener("click", () => this.onExport());
    const importBtn = el("button", { cls: "btn", text: "Import" });
    importBtn.addEventListener("click", () => this.onImport());
    const backBtn = el("button", { cls: "btn", text: "Back to Menu" });
    backBtn.addEventListener("click", this.onBack);
    return el("header", { cls: "hud editor-hud" }, [
      this.status,
      el("div", { cls: "spacer" }),
      playBtn,
      exportBtn,
      importBtn,
      backBtn,
    ]);
  }

  private spinner(
    label: string,
    val: number,
    min: number,
    max: number,
    on: (v: number) => void,
  ): HTMLElement {
    const input = el("input", {
      attrs: {
        type: "number",
        value: String(val),
        min: String(min),
        max: String(max),
      },
    });
    input.addEventListener("change", () => {
      const v = Math.max(min, Math.min(max, Number(input.value) || val));
      input.value = String(v);
      on(v);
      this.render();
    });
    return el("label", { cls: "spin" }, [el("span", { text: label }), input]);
  }

  private refreshPathSelect() {
    this.pathSelect.innerHTML = "";
    this.state.paths.forEach((_, i) => {
      this.pathSelect.append(el("option", { text: `Path ${i + 1}`, attrs: { value: String(i) } }));
    });
    this.pathSelect.value = String(this.activePath);
  }

  private usablePaths(): number {
    return this.state.paths.filter((p) => p.waypoints.length >= 2).length;
  }

  private regenWaves() {
    this.state.waves = generateWaves("medium", this.usablePaths());
    this.rebuildWaves();
  }

  private rebuildWaves() {
    const wrap = this.wavesWrap;
    wrap.innerHTML = "";
    this.state.waves.forEach((wave, wi) => {
      const gapInput = el("input", {
        attrs: {
          type: "number",
          value: String(wave.gap),
          step: "0.5",
          min: "0",
        },
      });
      gapInput.addEventListener("change", () => {
        wave.gap = Number(gapInput.value) || 0;
      });
      const card = el("div", { cls: "wave-card" }, [
        el("div", { cls: "wave-head" }, [
          el("span", { text: `Wave ${wi + 1}` }),
          el("label", { cls: "spin" }, [el("span", { text: "gap" }), gapInput]),
        ]),
      ]);
      wave.spawns.forEach((sp, si) => {
        const kindSel = el("select");
        for (const k of ENEMY_KINDS) kindSel.append(el("option", { text: k, attrs: { value: k } }));
        kindSel.value = sp.kind;
        kindSel.addEventListener("change", () => {
          sp.kind = kindSel.value;
        });
        const timeInput = el("input", {
          attrs: {
            type: "number",
            value: String(sp.time),
            step: "0.5",
            min: "0",
          },
        });
        timeInput.addEventListener("change", () => {
          sp.time = Number(timeInput.value) || 0;
        });
        const pathSel = el("select");
        this.state.paths.forEach((_, pi) =>
          pathSel.append(
            el("option", {
              text: `Path ${pi + 1}`,
              attrs: { value: String(pi) },
            }),
          ),
        );
        pathSel.value = String(Math.min(sp.path, Math.max(0, this.state.paths.length - 1)));
        pathSel.addEventListener("change", () => {
          sp.path = Number(pathSel.value);
        });
        const rm = el("button", { cls: "btn icon", text: "✕" });
        rm.addEventListener("click", () => {
          wave.spawns.splice(si, 1);
          this.rebuildWaves();
          this.setStatus("Spawn removed.");
        });
        card.append(el("div", { cls: "spawn-row" }, [kindSel, timeInput, pathSel, rm]));
      });
      const addSpawn = el("button", { cls: "btn", text: "+ Spawn" });
      addSpawn.addEventListener("click", () => {
        wave.spawns.push({ kind: "normal", time: 0, path: 0 });
        this.rebuildWaves();
        this.setStatus(`Spawn added to wave ${wi + 1}.`);
      });
      const rmWave = el("button", { cls: "btn", text: "Remove wave" });
      rmWave.addEventListener("click", () => {
        this.state.waves.splice(wi, 1);
        this.rebuildWaves();
        this.setStatus(`Wave ${wi + 1} removed.`);
      });
      card.append(el("div", { cls: "row" }, [addSpawn, rmWave]));
      wrap.append(card);
    });
    const addWave = el("button", { cls: "btn", text: "+ Wave" });
    addWave.addEventListener("click", () => {
      this.state.waves.push({
        gap: 3,
        spawns: [{ kind: "normal", time: 0, path: 0 }],
      });
      this.rebuildWaves();
      this.setStatus(`Wave ${this.state.waves.length} added.`);
    });
    wrap.append(addWave);
  }

  private resize(rows: number, cols: number) {
    const terrain: string[][] = [];
    for (let r = 0; r < rows; r++) {
      const row: string[] = [];
      for (let c = 0; c < cols; c++) {
        row.push(this.state.terrain[r]?.[c] ?? "grass");
      }
      terrain.push(row);
    }
    this.state.rows = rows;
    this.state.cols = cols;
    this.state.terrain = terrain;
    for (const p of this.state.paths) {
      p.waypoints = p.waypoints.filter(([c, r]) => c < cols && r < rows);
    }
    this.layoutDirty = true;
    this.render();
  }

  private ensureResizeObserver() {
    if (this.ro || !this.canvas.parentElement) return;
    this.ro = new ResizeObserver(() => {
      this.layoutDirty = true;
      this.render();
    });
    this.ro.observe(this.canvas.parentElement);
  }

  /// Recompute `tile` so the map fills the editor stage (centered), then size
  /// the canvas backing store at devicePixelRatio.
  private layout() {
    const stage = this.canvas.parentElement;
    if (stage && stage.clientWidth > 0) {
      this.tile = Math.max(
        1,
        Math.floor(
          Math.min(stage.clientWidth / this.state.cols, stage.clientHeight / this.state.rows),
        ),
      );
    }
    const dpr = window.devicePixelRatio || 1;
    const w = this.state.cols * this.tile;
    const h = this.state.rows * this.tile;
    this.canvas.width = w * dpr;
    this.canvas.height = h * dpr;
    this.canvas.style.width = `${w}px`;
    this.canvas.style.height = `${h}px`;
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  // --- input ---
  private cellAt(e: MouseEvent): [number, number] | null {
    const rect = this.canvas.getBoundingClientRect();
    const x = ((e.clientX - rect.left) / rect.width) * this.state.cols * this.tile;
    const y = ((e.clientY - rect.top) / rect.height) * this.state.rows * this.tile;
    const c = Math.floor(x / this.tile);
    const r = Math.floor(y / this.tile);
    if (c < 0 || r < 0 || c >= this.state.cols || r >= this.state.rows) return null;
    return [c, r];
  }

  private onDown(e: MouseEvent) {
    const cell = this.cellAt(e);
    if (!cell) return;
    const [c, r] = cell;
    if (this.tool === "terrain") {
      this.dragging = true;
      this.paint(c, r);
    } else if (this.tool === "path") {
      this.state.paths[this.activePath].waypoints.push([c, r]);
      this.render();
    } else if (this.tool === "portal") {
      this.portalClick(c, r);
    }
  }

  private onMove(e: MouseEvent) {
    const cell = this.cellAt(e);
    this.hover = cell;
    if (this.dragging && cell && this.tool === "terrain") this.paint(cell[0], cell[1]);
    else this.render();
  }

  private paint(c: number, r: number) {
    if (!this.state.terrain[r]) return;
    this.state.terrain[r][c] = this.selectedTerrain;
    this.render();
  }

  private portalClick(c: number, r: number) {
    const path = this.state.paths[this.activePath];
    const idx = path.waypoints.findIndex(([wc, wr]) => wc === c && wr === r);
    if (idx < 0) {
      this.setStatus("Portals connect two path waypoints — click a waypoint first.");
      return;
    }
    if (this.portalFirst < 0) {
      this.portalFirst = idx;
      this.setStatus("Portal start set — click the destination waypoint to pair.");
      this.render();
    } else {
      if (idx === this.portalFirst) {
        this.portalFirst = -1;
        this.setStatus("Portal cancelled — click a waypoint to start again.");
        this.render();
        return;
      }
      // Portal direction: source = earlier waypoint (lower index = further
      // back on the path), destination = later. Swap if the user clicked
      // them in reverse order.
      const src = Math.min(this.portalFirst, idx);
      const dst = Math.max(this.portalFirst, idx);
      path.portals.push([src, dst]);
      this.portalFirst = -1;
      this.setStatus("Portal paired.");
      this.render();
    }
  }

  private onPlayClick() {
    const badPath = this.state.paths.findIndex((p) => p.waypoints.length < 2);
    if (badPath >= 0) {
      this.setStatus(`Path ${badPath + 1} needs 2+ waypoints before you can Play.`);
      return;
    }
    if (this.state.towers.length === 0) {
      this.setStatus("Pick at least one tower type.");
      return;
    }
    if (this.state.waves.length === 0) {
      this.setStatus("Add at least one wave (or Generate).");
      return;
    }
    const json = serialize(this.state);
    const verr = this.onValidate(json);
    if (verr) {
      this.setStatus("Validation error: " + verr);
      return;
    }
    this.onPlay(json);
  }

  private onExport() {
    const json = serialize(this.state);
    const blob = new Blob([json], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = el("a", {
      attrs: { href: url, download: sanitize(this.state.name) + ".json" },
    });
    document.body.append(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
    this.setStatus(
      "Exported " + sanitize(this.state.name) + ".json — re-import it anytime via Import.",
    );
  }

  private onImport() {
    const input = el("input", { attrs: { type: "file", accept: ".json" } });
    input.addEventListener("change", () => {
      const file = input.files?.[0];
      if (!file) return;
      void file.text().then((text) => {
        const s = parse(text);
        if (!s) {
          alert("That file isn't a valid level JSON.");
          return;
        }
        const verr = this.onValidate(serialize(s));
        if (verr) {
          alert("Import rejected: " + verr);
          return;
        }
        this.setState(s);
        this.setStatus("Imported " + file.name + " — edit or Play to test.");
      });
    });
    input.click();
  }

  private setState(s: EditorState) {
    this.state = s;
    this.activePath = 0;
    this.portalFirst = -1;
    this.nameInput.value = s.name;
    this.refreshPathSelect();
    this.rebuildWaves();
    this.layoutDirty = true;
    this.render();
  }

  private setStatus(msg: string) {
    this.status.textContent = msg;
  }

  // --- render ---
  private render() {
    this.ensureResizeObserver();
    if (this.layoutDirty) {
      this.layoutDirty = false;
      this.layout();
    }
    const { ctx, state, tile } = this;
    const w = state.cols * tile;
    const h = state.rows * tile;
    ctx.clearRect(0, 0, w, h);
    for (let r = 0; r < state.rows; r++) {
      for (let c = 0; c < state.cols; c++) {
        ctx.fillStyle = terrainColor(state.terrain[r]?.[c] ?? "grass");
        ctx.fillRect(c * tile, r * tile, tile, tile);
      }
    }
    // Tile separators.
    ctx.strokeStyle = "rgba(0,0,0,0.2)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let c = 0; c <= state.cols; c++) {
      ctx.moveTo(c * tile, 0);
      ctx.lineTo(c * tile, h);
    }
    for (let r = 0; r <= state.rows; r++) {
      ctx.moveTo(0, r * tile);
      ctx.lineTo(w, r * tile);
    }
    ctx.stroke();
    // paths
    state.paths.forEach((p, pi) => {
      const active = pi === this.activePath;
      ctx.globalAlpha = active ? 1 : 0.35;
      ctx.strokeStyle = "white";
      ctx.lineWidth = 2;
      ctx.beginPath();
      p.waypoints.forEach(([c, r], i) => {
        const x = c * tile + tile / 2;
        const y = r * tile + tile / 2;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();
      p.waypoints.forEach(([c, r], i) => {
        const x = c * tile + tile / 2;
        const y = r * tile + tile / 2;
        ctx.fillStyle =
          i === 0 ? "rgb(30,220,30)" : i === p.waypoints.length - 1 ? "rgb(220,30,30)" : "white";
        ctx.beginPath();
        ctx.arc(x, y, 5, 0, Math.PI * 2);
        ctx.fill();
      });
      for (const [a, b] of p.portals) {
        const wa = p.waypoints[a];
        const wb = p.waypoints[b];
        if (!wa || !wb) continue;
        ctx.strokeStyle = "rgb(142,68,173)";
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(wa[0] * tile + tile / 2, wa[1] * tile + tile / 2);
        ctx.lineTo(wb[0] * tile + tile / 2, wb[1] * tile + tile / 2);
        ctx.stroke();
      }
      ctx.globalAlpha = 1;
      if (active && this.portalFirst >= 0) {
        const wp = p.waypoints[this.portalFirst];
        if (wp) {
          ctx.strokeStyle = "yellow";
          ctx.lineWidth = 2;
          ctx.strokeRect(wp[0] * tile, wp[1] * tile, tile, tile);
        }
      }
    });
    if (this.hover) {
      const [c, r] = this.hover;
      ctx.strokeStyle = "rgba(255,255,255,0.5)";
      ctx.lineWidth = 2;
      ctx.strokeRect(c * tile, r * tile, tile, tile);
    }
  }
}
