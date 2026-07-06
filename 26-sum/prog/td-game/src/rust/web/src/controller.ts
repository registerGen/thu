import type {
  BulletView,
  EnemyView,
  GameEvent,
  GameResult,
  GameState,
  LevelInfo,
  LevelView,
  TowerView,
  Vec2,
  WebApp,
} from "../pkg/td_game_rs.js";

export type PlaceResult =
  { ok: true } | { ok: false; reason: "not_placeable" | "not_enough_resource" };

/// Owns the WebApp and drives the simulation via requestAnimationFrame.
/// The loop's start/stop is the play/pause/idle control — the web equivalent
/// of Qt's QTimer (GameController owns the QTimer and calls tick(dt) per frame).
///
/// Exposes view/scalar getters so the renderer and HUD PULL state each tick,
/// mirroring Qt: GameWidget holds a GameController pointer and calls
/// controller_->towerViews() / levelView() in paintEvent.
export class Controller {
  private rafId = 0;
  private last = 0;
  private running = false;
  private ended = false;
  private fpsValue = 0;
  private selectedTower = "";
  private readonly app: WebApp;

  /// After each tick (state is current; events already drained).
  onTick?: () => void;
  /// The loop (re)started — a new game session begins. Views should invalidate
  /// any per-session caches (e.g. the renderer's background) and redraw.
  onStart?: () => void;
  /// The active level changed (select/advance/restart/custom). The palette
  /// rebuilds, the renderer invalidates, the HUD restores controls.
  onLevelStarted?: () => void;
  /// Discrete events drained this tick (tower_placed / enemy_killed / wave_started).
  onEvents?: (events: GameEvent[]) => void;
  /// The level ended (victory/defeat).
  onEnd?: (state: GameState) => void;

  constructor(app: WebApp) {
    this.app = app;
    // Auto-pause when the tab is hidden or the window loses focus, so the
    // simulation doesn't advance unattended. The user resumes manually.
    document.addEventListener("visibilitychange", () => {
      if (document.hidden && this.running && !this.app.paused()) this.app.pause();
    });
    window.addEventListener("blur", () => {
      if (this.running && !this.app.paused()) this.app.pause();
    });
  }

  // --- loop ---
  start() {
    if (this.running) return;
    this.running = true;
    this.ended = false;
    this.last = performance.now();
    this.onStart?.();
    const loop = (now: number) => {
      if (!this.running) return;
      // Real-time dt, clamped so a backgrounded tab doesn't fast-forward.
      const dt = Math.min((now - this.last) / 1000, 1 / 30);
      this.last = now;
      if (dt > 0) {
        const inst = 1 / dt;
        this.fpsValue = this.fpsValue === 0 ? inst : this.fpsValue * 0.9 + inst * 0.1;
      }

      // After the level ends, keep the loop alive (so the game-over banner and
      // float texts keep animating) but stop advancing the model. onEnd fires
      // once; the router stops the loop after its banner delay.
      if (!this.ended) {
        const ended = this.app.tick(dt);
        if (ended) {
          this.ended = true;
          this.onEnd?.(this.app.state());
        } else {
          this.onEvents?.(this.app.takeEvents());
        }
      }
      this.onTick?.();
      this.rafId = requestAnimationFrame(loop);
    };
    this.rafId = requestAnimationFrame(loop);
  }

  stop() {
    this.running = false;
    cancelAnimationFrame(this.rafId);
  }

  // --- tower selection + placement ---
  selectTowerType(kind: string) {
    this.selectedTower = kind;
  }
  selectedTowerType(): string {
    return this.selectedTower;
  }

  /// Place the currently-selected tower. Returns a typed result so the screen
  /// can react (beep on not-placeable, flash on insufficient resources),
  /// matching Qt's placementFailed / insufficientResource signals.
  placeTower(pos: Vec2): PlaceResult {
    if (!this.selectedTower) return { ok: false, reason: "not_placeable" };
    try {
      this.app.placeTower(this.selectedTower, pos);
      return { ok: true };
    } catch (e) {
      // TowerPlaceError is thrown as a bare string: "not_placeable" | "not_enough_resource".
      return {
        ok: false,
        reason: e as "not_placeable" | "not_enough_resource",
      };
    }
  }

  // --- commands ---
  togglePause() {
    if (this.app.paused()) this.app.resume();
    else this.app.pause();
  }
  applyCheat(code: string) {
    this.app.applyCheat(code);
  }

  // --- level lifecycle (each emits onLevelStarted) ---
  selectAndStartLevel(index: number) {
    this.app.selectAndStartLevel(index);
    this.selectedTower = "";
    this.onLevelStarted?.();
  }
  advanceAndStartLevel() {
    this.app.advanceAndStartLevel();
    this.selectedTower = "";
    this.onLevelStarted?.();
  }
  restartLevel() {
    this.app.restart();
    this.onLevelStarted?.();
  }
  nextLevel() {
    if (!this.app.hasNextOfficial()) return;
    this.app.advanceAndStartLevel();
    this.selectedTower = "";
    this.onLevelStarted?.();
  }
  /// Editor playtest. Returns the error message on failure, or null on success.
  startLevelJson(json: string): string | null {
    try {
      this.app.startLevelJson(json);
      this.selectedTower = "";
      this.onLevelStarted?.();
      return null;
    } catch (e) {
      return typeof e === "string" ? e : "Invalid level JSON";
    }
  }

  /// Validate a level JSON without starting it. Returns the error message on
  /// failure, or null on success.
  validateLevelJson(json: string): string | null {
    try {
      this.app.validateLevelJson(json);
      return null;
    } catch (e) {
      return typeof e === "string" ? e : "Invalid level JSON";
    }
  }

  // --- getters (pulled by the renderer / HUD each tick) ---
  levelView(): LevelView {
    return this.app.levelView();
  }
  levelName(): string {
    return this.app.levelName();
  }
  levelIndex(): number {
    return this.app.levelIndex();
  }
  towerViews(): TowerView[] {
    return this.app.towerViews();
  }
  enemyViews(): EnemyView[] {
    return this.app.enemyViews();
  }
  bulletViews(): BulletView[] {
    return this.app.bulletViews();
  }
  canPlaceAt(pos: Vec2): boolean {
    return this.app.canPlaceAt(pos);
  }
  state(): GameState {
    return this.app.state();
  }
  paused(): boolean {
    return this.app.paused();
  }
  score(): number {
    return this.app.score();
  }
  currentWave(): number {
    return this.app.currentWave();
  }
  resourceAmount(): number {
    return this.app.resourceAmount();
  }
  elapsedTime(): number {
    return this.app.elapsedTime();
  }
  fps(): number {
    return this.fpsValue;
  }
  hasNextLevel(): boolean {
    return this.app.hasNextOfficial();
  }
  currentLevelIndex(): number {
    return this.app.currentLevelIndex();
  }
  registryInfos(): LevelInfo[] {
    return this.app.registryInfos();
  }
  lastResult(): GameResult {
    return this.app.lastResult();
  }
}
