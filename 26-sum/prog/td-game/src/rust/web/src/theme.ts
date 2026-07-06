// Color palette ported from app/Theme.h (the string-keyed helpers).
// Returns CSS rgb() strings for canvas fillStyle / strokeStyle.

export const MONO_FONT = "ui-monospace, 'SF Mono', 'Cascadia Code', Menlo, Consolas, monospace";

export function terrainColor(name: string): string {
  switch (name) {
    case "portal":
      return "rgb(142,68,173)";
    case "ice":
      return "rgb(174,227,240)";
    case "rock":
      return "rgb(90,90,90)";
    case "fertile":
      return "rgb(107,142,35)";
    default:
      return "rgb(74,124,58)"; // grass
  }
}

export function towerColor(type: string): string {
  switch (type) {
    case "normal":
      return "rgb(231,76,60)";
    case "slow":
      return "rgb(52,152,219)";
    case "poison":
      return "rgb(39,174,96)";
    case "splash":
      return "rgb(230,126,34)";
    case "laser":
      return "rgb(155,89,182)";
    case "resource":
      return "rgb(241,196,15)";
    case "wall":
      return "rgb(127,140,141)";
    default:
      return "rgb(200,200,200)";
  }
}

export function enemyColor(type: string): string {
  switch (type) {
    case "fast":
      return "rgb(243,156,18)";
    case "armored":
      return "rgb(127,140,141)";
    case "resistant":
      return "rgb(22,160,133)";
    case "splitter":
      return "rgb(142,68,173)";
    case "boss":
      return "rgb(192,57,43)";
    default:
      return "rgb(231,76,60)"; // normal
  }
}

export function bulletColor(type: string): string {
  switch (type) {
    case "slow":
      return "rgb(52,152,219)";
    case "poison":
      return "rgb(39,174,96)";
    case "splash":
      return "rgb(230,126,34)";
    case "laser":
      return "rgb(155,89,182)";
    default:
      return "rgb(231,76,60)"; // normal
  }
}

export function isRectTower(type: string): boolean {
  return type === "resource" || type === "wall";
}

/// Draw a tower preview icon into `ctx` within the box (x, y, size)
/// (mirrors Qt's theme::drawTowerPreview). Attack towers get a barrel along +x;
/// resource/wall are squares.
export function drawTowerPreview(
  ctx: CanvasRenderingContext2D,
  kind: string,
  x: number,
  y: number,
  size: number,
) {
  const r = size * 0.4;
  const cx = x + size / 2;
  const cy = y + size / 2;
  if (!isRectTower(kind)) {
    ctx.strokeStyle = "rgb(40,40,40)";
    ctx.lineWidth = Math.max(2, size * 0.12);
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + r * 1.4, cy);
    ctx.stroke();
  }
  ctx.fillStyle = towerColor(kind);
  ctx.strokeStyle = "black";
  ctx.lineWidth = kind === "wall" ? 3 : 2;
  if (isRectTower(kind)) {
    ctx.fillRect(cx - r, cy - r, r * 2, r * 2);
    ctx.strokeRect(cx - r, cy - r, r * 2, r * 2);
  } else {
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
  }
}
