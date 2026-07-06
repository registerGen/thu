/// Per-level progress (cleared + max score), persisted to localStorage.
/// Mirrors Qt's in-memory QVector<LevelProgress>, but survives reloads.
/// Only official levels (index >= 1) are tracked; cheated clears don't count.

export interface LevelProgress {
  cleared: boolean;
  maxScore: number;
}

const KEY = "td-game:progress";

function load(): Record<number, LevelProgress> {
  try {
    const raw = localStorage.getItem(KEY);
    return raw ? (JSON.parse(raw) as Record<number, LevelProgress>) : {};
  } catch {
    return {};
  }
}

function save(data: Record<number, LevelProgress>) {
  try {
    localStorage.setItem(KEY, JSON.stringify(data));
  } catch {
    // ignore quota / disabled storage
  }
}

export function getProgress(index: number): LevelProgress {
  const data = load();
  return data[index] ?? { cleared: false, maxScore: 0 };
}

/// Wipe all stored progress.
export function clearProgress() {
  try {
    localStorage.removeItem(KEY);
  } catch {
    // ignore
  }
}

/// Record a run's outcome. On victory the level is marked cleared; on defeat
/// only the max score is updated (cleared stays as it was). Cheated runs and
/// custom levels (index < 1) are ignored.
export function recordResult(index: number, score: number, cheated: boolean, cleared: boolean) {
  if (index < 1 || cheated) return;
  const data = load();
  const prev = data[index] ?? { cleared: false, maxScore: 0 };
  data[index] = {
    cleared: prev.cleared || cleared, // once cleared, stays cleared
    maxScore: Math.max(prev.maxScore, score),
  };
  save(data);
}
