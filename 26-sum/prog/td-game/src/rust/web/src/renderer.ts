import type { Controller } from "./controller.js";
import type {
  BulletView,
  EnemyView,
  GameEvent,
  LevelView,
  TowerView,
  Vec2,
} from "../pkg/td_game_rs.js";
import {
  bulletColor,
  enemyColor,
  isRectTower,
  MONO_FONT,
  terrainColor,
  towerColor,
} from "./theme.js";

/// Canvas renderer: holds a Controller reference and pulls state each render
/// (mirrors Qt's GameWidget holding a GameController*). The static background
/// (terrain + spawn/exit markers) is cached to an offscreen canvas and rebuilt
/// only when the level changes; entities are drawn fresh each frame.
export class Renderer {
  private readonly ctx: CanvasRenderingContext2D;
  private readonly bg: HTMLCanvasElement;
  private readonly bgCtx: CanvasRenderingContext2D;
  private readonly canvas: HTMLCanvasElement;
  private readonly controller: Controller;
  private tileSize = 64;
  private level: LevelView | null = null;
  // DPR scaling: backing store is cssW*dpr, context scaled so we draw in CSS px.
  private cssW = 0;
  private cssH = 0;
  // Overlay state (mirrors GameWidget's float texts / banner / hover ghost).
  private floats: {
    x: number;
    y: number;
    text: string;
    color: string;
    age: number;
  }[] = [];
  private banner: { text: string; color: string; age: number } | null = null;
  private hover: Vec2 | null = null;
  private lastTime = 0;
  // Dynamic layout: tileSize is recomputed from the stage size so the map fills
  // the available space (centered), not a fixed size.
  private layoutDirty = true;
  private ro: ResizeObserver | null = null;

  constructor(canvas: HTMLCanvasElement, controller: Controller) {
    this.canvas = canvas;
    this.controller = controller;
    this.ctx = canvas.getContext("2d")!;
    this.bg = document.createElement("canvas");
    this.bgCtx = this.bg.getContext("2d")!;
  }

  /// Map a mouse event to world coordinates (DPR- and CSS-scale independent).
  worldFromEvent(e: MouseEvent): Vec2 {
    const rect = this.canvas.getBoundingClientRect();
    const lv = this.level ?? this.controller.levelView();
    return {
      x: ((e.clientX - rect.left) / rect.width) * lv.mapWidth,
      y: ((e.clientY - rect.top) / rect.height) * lv.mapHeight,
    };
  }

  invalidate() {
    this.level = null;
  }

  private ensureResizeObserver() {
    if (this.ro || !this.canvas.parentElement) return;
    this.ro = new ResizeObserver(() => {
      this.layoutDirty = true;
    });
    this.ro.observe(this.canvas.parentElement);
  }

  /// Recompute tileSize so the map fills the stage (canvas's parent) — centered
  /// and using all available space, like Qt's recomputeLayout.
  private recomputeLayout() {
    const stage = this.canvas.parentElement;
    if (!stage || stage.clientWidth === 0) return; // not laid out yet (hidden)
    const lv = this.level ?? this.controller.levelView();
    const ts = Math.max(
      1,
      Math.floor(Math.min(stage.clientWidth / lv.mapWidth, stage.clientHeight / lv.mapHeight)),
    );
    if (ts !== this.tileSize) {
      this.tileSize = ts;
      this.level = null; // force background rebuild at the new size
    }
  }

  setHover(pos: Vec2 | null) {
    this.hover = pos;
  }

  addEvents(events: GameEvent[]) {
    for (const e of events) {
      if (e.type === "tower_placed") {
        this.floats.push({
          x: e.pos.x,
          y: e.pos.y,
          text: `-${e.cost}`,
          color: towerColor(e.kind),
          age: 0,
        });
      } else if (e.type === "enemy_killed") {
        this.floats.push({
          x: e.pos.x,
          y: e.pos.y,
          text: `+${e.score}`,
          color: enemyColor(e.kind),
          age: 0,
        });
      } else if (e.type === "wave_started") {
        const tag = e.isLast ? "LAST" : "NEW";
        const boss = e.hasBoss ? "  [BOSS!]" : "";
        this.banner = {
          text: `${tag} wave! (No. ${e.index})${boss}`,
          color: "rgb(231,126,34)",
          age: 0,
        };
      }
    }
  }

  private worldToPixel(pos: Vec2): [number, number] {
    return [pos.x * this.tileSize, pos.y * this.tileSize];
  }

  render() {
    this.ensureResizeObserver();
    if (this.layoutDirty) {
      this.layoutDirty = false;
      this.recomputeLayout();
    }
    if (this.level === null || this.controller.levelName() !== this.level.info.name) {
      this.level = this.controller.levelView();
      this.rebuildBackground(this.level);
    }
    const { ctx, controller, cssW, cssH } = this;
    const now = performance.now();
    const dt = this.lastTime === 0 ? 0 : (now - this.lastTime) / 1000;
    this.lastTime = now;

    ctx.clearRect(0, 0, cssW, cssH);
    ctx.drawImage(this.bg, 0, 0, cssW, cssH);
    this.drawHoverGhost();
    this.drawTowers(controller.towerViews());
    this.drawEnemies(controller.enemyViews());
    this.drawBullets(controller.bulletViews());
    this.drawFloats(dt);
    this.drawBanner(dt);
    const state = controller.state();
    if (state !== "playing") this.drawGameOverBanner(state);
    if (controller.paused()) this.drawPausedOverlay();
  }

  private drawHoverGhost() {
    if (!this.hover) return;
    if (this.controller.state() !== "playing") return;
    if (!this.controller.selectedTowerType()) return;
    // Snap to the tile under the cursor (not the fractional mouse position).
    const cx = Math.floor(this.hover.x);
    const cy = Math.floor(this.hover.y);
    const { ctx, tileSize } = this;
    const ok = this.controller.canPlaceAt({ x: cx + 0.5, y: cy + 0.5 });
    ctx.fillStyle = ok ? "rgba(255,255,255,0.3)" : "rgba(255,0,0,0.3)";
    ctx.fillRect(cx * tileSize, cy * tileSize, tileSize, tileSize);
  }

  private drawFloats(dt: number) {
    const { ctx, tileSize } = this;
    const lifetime = 0.9;
    ctx.font = `bold ${Math.max(14, tileSize * 0.32)}px ${MONO_FONT}`;
    ctx.textAlign = "center";
    for (const f of this.floats) {
      f.age += dt;
      const t = f.age / lifetime;
      if (t >= 1) continue;
      const [x, y] = this.worldToPixel({ x: f.x, y: f.y });
      ctx.globalAlpha = 1 - t;
      ctx.fillStyle = f.color;
      ctx.fillText(f.text, x, y - 24 * t);
    }
    ctx.globalAlpha = 1;
    this.floats = this.floats.filter((f) => f.age < lifetime);
  }

  private drawBanner(dt: number) {
    if (!this.banner) return;
    this.banner.age += dt;
    if (this.banner.age >= 2.0) {
      this.banner = null;
      return;
    }
    const { ctx, cssW, tileSize } = this;
    ctx.font = `bold ${Math.max(18, tileSize * 0.4)}px ${MONO_FONT}`;
    ctx.textAlign = "center";
    ctx.fillStyle = this.banner.color;
    ctx.fillText(this.banner.text, cssW / 2, Math.max(24, tileSize * 0.5));
  }

  private drawGameOverBanner(state: string) {
    const { ctx, cssW, cssH, tileSize } = this;
    const text = state === "victory" ? "Game cleared!" : "You lose!";
    ctx.font = `bold ${Math.max(28, tileSize * 0.7)}px ${MONO_FONT}`;
    ctx.textAlign = "center";
    ctx.fillStyle = state === "victory" ? "rgb(39,174,96)" : "rgb(231,76,60)";
    ctx.fillText(text, cssW / 2, cssH / 2);
  }

  private drawPausedOverlay() {
    const { ctx, cssW, cssH, tileSize } = this;
    ctx.fillStyle = "rgba(0,0,0,0.55)";
    const w = Math.max(200, cssW * 0.45);
    const h = Math.max(96, cssH * 0.24);
    const x = (cssW - w) / 2;
    const y = (cssH - h) / 2;
    ctx.beginPath();
    ctx.roundRect(x, y, w, h, 14);
    ctx.fill();
    ctx.fillStyle = "white";
    ctx.font = `bold ${Math.max(26, tileSize * 0.55)}px sans-serif`;
    ctx.textAlign = "center";
    ctx.fillText("Paused", cssW / 2, y + h / 2 - 4);
    ctx.fillStyle = "rgb(220,220,220)";
    ctx.font = `${Math.max(13, tileSize * 0.24)}px sans-serif`;
    ctx.fillText("Press Space to resume", cssW / 2, y + h / 2 + 22);
  }

  /// Resize both canvases to fit the level (at devicePixelRatio) and redraw
  /// terrain + markers once.
  private rebuildBackground(level: LevelView) {
    const { tileSize, bgCtx } = this;
    const dpr = window.devicePixelRatio || 1;
    const w = Math.round(level.mapWidth * tileSize);
    const h = Math.round(level.mapHeight * tileSize);
    this.cssW = w;
    this.cssH = h;
    this.canvas.width = w * dpr;
    this.canvas.height = h * dpr;
    this.canvas.style.width = `${w}px`;
    this.canvas.style.height = `${h}px`;
    this.bg.width = w * dpr;
    this.bg.height = h * dpr;
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    bgCtx.setTransform(dpr, 0, 0, dpr, 0, 0);

    let i = 0;
    for (let wy = 0; wy < level.mapHeight; wy++) {
      for (let wx = 0; wx < level.mapWidth; wx++) {
        const [x, y] = this.worldToPixel({ x: wx, y: wy });
        bgCtx.fillStyle = terrainColor(level.terrain[i++]);
        bgCtx.fillRect(x, y, tileSize, tileSize);
      }
    }
    // Subtle grid lines (tile separators).
    bgCtx.strokeStyle = "rgba(0,0,0,0.16)";
    bgCtx.lineWidth = 1;
    bgCtx.beginPath();
    for (let c = 0; c <= level.mapWidth; c++) {
      bgCtx.moveTo(c * tileSize, 0);
      bgCtx.lineTo(c * tileSize, h);
    }
    for (let r = 0; r <= level.mapHeight; r++) {
      bgCtx.moveTo(0, r * tileSize);
      bgCtx.lineTo(w, r * tileSize);
    }
    bgCtx.stroke();

    for (const wps of level.pathWaypoints) {
      if (wps.length === 0) continue;
      const [sx, sy] = this.worldToPixel(wps[0]);
      bgCtx.strokeStyle = "rgb(30,220,30)";
      bgCtx.lineWidth = Math.max(tileSize * 0.12, 2);
      bgCtx.beginPath();
      bgCtx.arc(sx, sy, tileSize * 0.35, 0, Math.PI * 2);
      bgCtx.stroke();

      const [ex, ey] = this.worldToPixel(wps[wps.length - 1]);
      const arm = tileSize * 0.35;
      bgCtx.strokeStyle = "rgb(220,30,30)";
      bgCtx.beginPath();
      bgCtx.moveTo(ex - arm, ey - arm);
      bgCtx.lineTo(ex + arm, ey + arm);
      bgCtx.moveTo(ex - arm, ey + arm);
      bgCtx.lineTo(ex + arm, ey - arm);
      bgCtx.stroke();
    }
  }

  private drawHpBar(cx: number, y: number, halfWidth: number, health: number, maxHealth: number) {
    const { ctx } = this;
    const pct = maxHealth > 0 ? health / maxHealth : 0;
    ctx.fillStyle = "rgba(0,0,0,0.6)";
    ctx.fillRect(cx - halfWidth, y, halfWidth * 2, 5);
    ctx.fillStyle = `hsl(${pct * 120},90%,45%)`;
    ctx.fillRect(cx - halfWidth, y, halfWidth * 2 * pct, 5);
  }

  private drawTowers(towers: TowerView[]) {
    const { ctx, tileSize } = this;
    const r = tileSize * 0.4;
    for (const t of towers) {
      const [x, y] = this.worldToPixel(t.pos);
      if (t.aim) {
        ctx.strokeStyle = "rgb(40,40,40)";
        ctx.lineWidth = 6;
        ctx.beginPath();
        ctx.moveTo(x, y);
        ctx.lineTo(x + t.aim.x * r * 1.4, y + t.aim.y * r * 1.4);
        ctx.stroke();
      }
      ctx.fillStyle = towerColor(t.kind);
      ctx.strokeStyle = "black";
      ctx.lineWidth = t.kind === "wall" ? 3 : 2;
      if (isRectTower(t.kind)) {
        ctx.fillRect(x - r, y - r, r * 2, r * 2);
        ctx.strokeRect(x - r, y - r, r * 2, r * 2);
      } else {
        ctx.beginPath();
        ctx.arc(x, y, r, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();
      }
      if (t.health < t.maxHealth) {
        this.drawHpBar(x, y - r - 8, r, t.health, t.maxHealth);
      }
    }
  }

  private drawEnemies(enemies: EnemyView[]) {
    const { ctx, tileSize } = this;
    for (const e of enemies) {
      const [x, y] = this.worldToPixel(e.pos);
      const rx = Math.max(e.halfWidth * tileSize, 4);
      const ry = Math.max(e.halfHeight * tileSize, 4);
      ctx.fillStyle = enemyColor(e.kind);
      ctx.strokeStyle = "black";
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.ellipse(x, y, rx, ry, 0, 0, Math.PI * 2);
      ctx.fill();
      ctx.stroke();
      const rings: string[] = [];
      const s = e.statusHint;
      if (s.slow) rings.push("rgb(52,152,219)");
      if (s.poison) rings.push("rgb(39,174,96)");
      if (s.regen) rings.push("rgb(241,196,15)");
      const rrx = rx + 4;
      const rry = ry + 4;
      for (let i = 0; i < rings.length; i++) {
        const start = (Math.PI * 2 * i) / rings.length + Math.PI / 2;
        const end = (Math.PI * 2 * (i + 1)) / rings.length + Math.PI / 2;
        ctx.strokeStyle = rings[i];
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.ellipse(x, y, rrx, rry, 0, start, end);
        ctx.stroke();
      }
      this.drawHpBar(x, y - ry - 8, rx, e.health, e.maxHealth);
    }
  }

  private drawBullets(bullets: BulletView[]) {
    const { ctx, tileSize } = this;
    for (const b of bullets) {
      const [x, y] = this.worldToPixel(b.pos);
      const c = bulletColor(b.kind);
      if (b.kind === "laser") {
        const m = Math.hypot(b.vel.x, b.vel.y) || 1;
        const len = tileSize * 8;
        ctx.strokeStyle = c;
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(x, y);
        ctx.lineTo(x + (b.vel.x / m) * len, y + (b.vel.y / m) * len);
        ctx.stroke();
      } else if (b.kind === "splash" && b.radius) {
        ctx.globalAlpha = 0.25;
        ctx.fillStyle = c;
        ctx.beginPath();
        ctx.arc(x, y, b.radius * tileSize, 0, Math.PI * 2);
        ctx.fill();
        ctx.globalAlpha = 1;
        ctx.beginPath();
        ctx.arc(x, y, 4, 0, Math.PI * 2);
        ctx.fill();
      } else {
        ctx.fillStyle = c;
        ctx.beginPath();
        ctx.arc(x, y, 3, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }
}
