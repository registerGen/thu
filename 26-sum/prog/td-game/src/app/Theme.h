#pragma once

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <cmath>
#include <string>

#include "game/bullet.h"
#include "game/enemy.h"
#include "game/tile.h"
#include "game/tower.h"

/// Procedural visual theme: colors for terrain/towers/enemies/bullets plus
/// shared draw helpers used by both the game view and the tower palette.
/// No image assets — everything is drawn with QPainter.
namespace theme {

constexpr double kPi = 3.14159265358979;

inline QColor terrainColor(Tile const& c) {
  // NOTE: path tiles are intentionally NOT distinguished — the enemy route is
  // not hinted to the player. A tile is colored by its terrain only.
  if (c.is_portal()) return QColor(142, 68, 173);                    // purple
  if (c.enemy_speed_factor() > 1.0f) return QColor(174, 227, 240);   // ice
  if (!c.can_place_tower()) return QColor(90, 90, 90);               // rock
  if (c.resource_cost_factor() < 1.0f) return QColor(107, 142, 35);  // fertile
  return QColor(74, 124, 58);                                        // grass
}

/// Same colors keyed by terrain name, for the legend sidebar.
inline QColor terrainColorForName(std::string const& name) {
  if (name == "portal") return QColor(142, 68, 173);
  if (name == "ice") return QColor(174, 227, 240);
  if (name == "rock") return QColor(90, 90, 90);
  if (name == "fertile") return QColor(107, 142, 35);
  return QColor(74, 124, 58);  // grass
}

inline QColor towerColorForType(std::string const& type) {
  if (type == "normal") return QColor(231, 76, 60);
  if (type == "slow") return QColor(52, 152, 219);
  if (type == "poison") return QColor(39, 174, 96);
  if (type == "splash") return QColor(230, 126, 34);
  if (type == "laser") return QColor(155, 89, 182);
  if (type == "resource") return QColor(241, 196, 15);
  if (type == "wall") return QColor(127, 140, 141);
  return QColor(200, 200, 200);
}
inline QColor towerColor(Tower const& t) { return towerColorForType(t.type()); }

inline bool isRectTower(std::string const& type) { return type == "resource" || type == "wall"; }

inline QColor enemyColorForType(std::string const& type) {
  if (type == "fast") return QColor(243, 156, 18);
  if (type == "armored") return QColor(127, 140, 141);
  if (type == "resistant") return QColor(22, 160, 133);
  if (type == "splitter") return QColor(142, 68, 173);
  if (type == "boss") return QColor(192, 57, 43);
  return QColor(231, 76, 60);  // normal
}
inline QColor enemyColor(Enemy const& e) { return enemyColorForType(e.type()); }

inline QColor bulletColor(Bullet const& b) {
  if (dynamic_cast<ExplosiveBullet const*>(&b)) return QColor(230, 126, 34);
  if (dynamic_cast<LaserBullet const*>(&b)) return QColor(155, 89, 182);
  if (dynamic_cast<SlowBullet const*>(&b)) return QColor(52, 152, 219);
  if (dynamic_cast<PoisonBullet const*>(&b)) return QColor(39, 174, 96);
  return QColor(231, 76, 60);  // normal
}

/// Draw a rectangular "muzzle"/barrel from the center of `r` in direction `dir`
/// (rotated). Drawn BEFORE the tower body so the inner half is hidden under it
/// and only the part extending past the body is visible.
inline void drawMuzzle(QPainter& p, QRectF const& r, Vec2 dir) {
  if (dir.length_sq() == 0.0f) dir = {1.0f, 0.0f};
  float angle = static_cast<float>(std::atan2(dir.y, dir.x) * 180.0 / kPi);
  p.save();
  p.translate(r.center());
  p.rotate(angle);
  p.setBrush(QBrush(QColor(40, 40, 40)));
  p.setPen(QPen(Qt::black, 1));
  p.drawRect(QRectF(0, -3, r.width() * 0.7, 6));
  p.restore();
}

inline void drawTowerBody(QPainter& p, QColor c, bool is_rect, QRectF const& r, int pen_w) {
  p.setBrush(QBrush(c));
  p.setPen(QPen(Qt::black, pen_w));
  if (is_rect)
    p.drawRect(r);
  else
    p.drawEllipse(r);
}

/// Draw a real tower (uses its live aim direction for the muzzle).
inline void drawTower(QPainter& p, Tower const& t, QRectF const& r) {
  bool is_wall = dynamic_cast<WallTower const*>(&t) != nullptr;
  bool is_resource = dynamic_cast<ResourceTower const*>(&t) != nullptr;
  if (auto* a = dynamic_cast<AttackTower const*>(&t)) drawMuzzle(p, r, a->aim());
  drawTowerBody(p, towerColor(t), is_wall || is_resource, r, is_wall ? 3 : 2);
}

/// Draw a tower preview by type string (default muzzle direction). For palette icons.
inline void drawTowerPreview(QPainter& p, std::string const& type, QRectF const& r) {
  bool is_rect = isRectTower(type);
  if (!is_rect) drawMuzzle(p, r, Vec2{1.0f, 0.0f});
  drawTowerBody(p, towerColorForType(type), is_rect, r, type == "wall" ? 3 : 2);
}

/// Render a tower of `type` into a transparent pixmap of the given size.
/// Used for the app icon and other standalone tower glyphs.
inline QPixmap makeTowerPixmap(std::string const& type, int size) {
  QPixmap pm(size, size);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  drawTowerPreview(p, type, QRectF(size * 0.1, size * 0.1, size * 0.8, size * 0.8));
  p.end();
  return pm;
}

// --- UI / marker colors (entity-independent) -------------------------------
// Centralized so the game view, legend, help, and editor share one palette.
inline QColor gridLineColor() { return QColor(0, 0, 0, 40); }             // tile borders
inline QColor spawnMarkerColor() { return QColor(30, 220, 30); }          // route entrance
inline QColor exitMarkerColor() { return QColor(220, 30, 30); }           // route exit
inline QColor pathTileColor() { return QColor(141, 110, 70); }            // editor path fill
inline QColor pathLineColor() { return Qt::white; }                       // editor path connector
inline QColor hoverHighlightColor() { return QColor(255, 255, 0, 200); }  // editor hover box
inline QColor enemyGlyphColor() { return QColor(200, 200, 200); }  // neutral enemy body in icons
inline QColor hpBarBackgroundColor() { return QColor(0, 0, 0, 160); }
/// Health-bar fill: green (full) -> red (empty), via HSV hue.
inline QColor hpBarColor(float pct) { return QColor::fromHsvF(pct * 0.33f, 1.0f, 0.8f); }
/// Placement ghost: translucent white when valid, red when not.
inline QColor placementGhostColor(bool ok) {
  return ok ? QColor(255, 255, 255, 80) : QColor(255, 0, 0, 80);
}
/// Status-effect ring drawn around an affected enemy.
inline QColor slowRingColor() { return QColor(52, 152, 219); }
inline QColor poisonRingColor() { return QColor(39, 174, 96); }
inline QColor regenRingColor() { return QColor(241, 196, 15); }
/// Warning text (e.g. "Not enough resource!").
inline QColor warningTextColor() { return QColor(192, 57, 43); }
/// Return a copy of `c` with the alpha channel replaced (for derived tints).
inline QColor withAlpha(QColor c, int alpha) { return QColor(c.red(), c.green(), c.blue(), alpha); }

}  // namespace theme
