/// Editor state ↔ level JSON, matching the schema in game/config.cpp
/// (save_level_json / load_level_json). Custom levels use index = -1.

export interface EditorSpawn {
  kind: string;
  time: number;
  path: number;
}
export interface EditorWave {
  gap: number;
  spawns: EditorSpawn[];
}
export interface EditorPath {
  waypoints: [number, number][]; // [col, row]
  portals: [number, number][]; // [srcWaypointIdx, tgtWaypointIdx]
}
export interface EditorState {
  name: string;
  rows: number;
  cols: number;
  terrain: string[][]; // [row][col]
  paths: EditorPath[];
  gold: number;
  autoAmt: number;
  autoInt: number;
  towers: string[];
  waves: EditorWave[];
}

export const ALL_TOWERS = ["normal", "slow", "poison", "splash", "laser", "resource", "wall"];
const TERRAINS = ["grass", "fertile", "rock", "ice"];

export function defaultState(): EditorState {
  const rows = 7;
  const cols = 12;
  const terrain: string[][] = [];
  for (let r = 0; r < rows; r++) terrain.push(Array(cols).fill("grass"));
  return {
    name: "Custom Level",
    rows,
    cols,
    terrain,
    paths: [{ waypoints: [], portals: [] }],
    gold: 150,
    autoAmt: 4,
    autoInt: 1,
    towers: [...ALL_TOWERS],
    waves: generateWaves("medium", 1),
  };
}

export function serialize(s: EditorState): string {
  return JSON.stringify({
    name: s.name,
    index: -1,
    map: { rows: s.rows, cols: s.cols, terrain: s.terrain },
    paths: s.paths.map((p) => ({ waypoints: p.waypoints, portals: p.portals })),
    starting_resources: s.gold,
    resource_auto_inc_amount: s.autoAmt,
    resource_auto_inc_interval: s.autoInt,
    available_towers: s.towers,
    waves: s.waves,
  });
}

interface RawLevel {
  name?: unknown;
  map?: { rows?: unknown; cols?: unknown; terrain?: unknown };
  paths?: { waypoints?: unknown; portals?: unknown }[];
  starting_resources?: unknown;
  resource_auto_inc_amount?: unknown;
  resource_auto_inc_interval?: unknown;
  available_towers?: unknown;
  waves?: {
    gap?: unknown;
    spawns?: { kind?: unknown; time?: unknown; path?: unknown }[];
  }[];
}

export function parse(json: string): EditorState | null {
  try {
    const o = JSON.parse(json) as RawLevel;
    const rows = Number(o.map?.rows ?? 7);
    const cols = Number(o.map?.cols ?? 12);
    return {
      name: String(o.name ?? "Custom"),
      rows,
      cols,
      terrain: (o.map?.terrain as string[][]) ?? [],
      paths: (o.paths ?? []).map((p) => ({
        waypoints: (p.waypoints as [number, number][]) ?? [],
        portals: (p.portals as [number, number][]) ?? [],
      })),
      gold: Number(o.starting_resources ?? 150),
      autoAmt: Number(o.resource_auto_inc_amount ?? 4),
      autoInt: Number(o.resource_auto_inc_interval ?? 1),
      towers: (o.available_towers as string[]) ?? [],
      waves: (o.waves ?? []).map((w) => ({
        gap: Number(w.gap ?? 3),
        spawns: (w.spawns ?? []).map((sp) => ({
          kind: String(sp.kind),
          time: Number(sp.time),
          path: Number(sp.path),
        })),
      })),
    };
  } catch {
    return null;
  }
}

type Difficulty = "easy" | "medium" | "hard";

const TEMPLATES: Record<Difficulty, { gap: number; spawns: [string, number][] }[]> = {
  easy: [
    {
      gap: 4,
      spawns: [
        ["normal", 0],
        ["normal", 1],
        ["normal", 2],
      ],
    },
    {
      gap: 4,
      spawns: [
        ["normal", 0],
        ["fast", 1],
        ["normal", 2],
      ],
    },
    {
      gap: 5,
      spawns: [
        ["armored", 0],
        ["normal", 1],
        ["fast", 2],
      ],
    },
    {
      gap: 5,
      spawns: [
        ["normal", 0],
        ["armored", 1],
        ["boss", 2],
      ],
    },
  ],
  medium: [
    {
      gap: 3,
      spawns: [
        ["normal", 0],
        ["normal", 1],
        ["fast", 2],
      ],
    },
    {
      gap: 3,
      spawns: [
        ["armored", 0],
        ["normal", 1],
        ["fast", 2],
      ],
    },
    {
      gap: 4,
      spawns: [
        ["splitter", 0],
        ["normal", 1],
        ["armored", 2],
      ],
    },
    {
      gap: 4,
      spawns: [
        ["resistant", 0],
        ["fast", 1],
        ["normal", 2],
      ],
    },
    {
      gap: 5,
      spawns: [
        ["normal", 0],
        ["armored", 1],
        ["boss", 2],
      ],
    },
  ],
  hard: [
    {
      gap: 2,
      spawns: [
        ["fast", 0],
        ["fast", 1],
        ["normal", 2],
      ],
    },
    {
      gap: 3,
      spawns: [
        ["armored", 0],
        ["fast", 1],
        ["resistant", 2],
      ],
    },
    {
      gap: 3,
      spawns: [
        ["splitter", 0],
        ["fast", 1],
        ["armored", 2],
      ],
    },
    {
      gap: 4,
      spawns: [
        ["resistant", 0],
        ["splitter", 1],
        ["fast", 2],
      ],
    },
    {
      gap: 4,
      spawns: [
        ["fast", 0],
        ["boss", 1],
        ["armored", 2],
      ],
    },
    {
      gap: 5,
      spawns: [
        ["boss", 0],
        ["resistant", 1],
        ["boss", 2],
      ],
    },
  ],
};

export function generateWaves(difficulty: Difficulty, numPaths: number): EditorWave[] {
  const n = Math.max(1, numPaths);
  const randPath = () => Math.floor(Math.random() * n);
  return TEMPLATES[difficulty].map((w) => ({
    gap: w.gap,
    spawns: w.spawns.map(([kind, time]) => ({ kind, time, path: randPath() })),
  }));
}

export { TERRAINS };
