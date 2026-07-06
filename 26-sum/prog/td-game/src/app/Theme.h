#pragma once

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <cmath>
#include <string>

#include "Views.h"  // Vec2, TowerView, EnemyView, BulletView, TileView

/// Procedural visual theme: colors for terrain/towers/enemies/bullets plus
/// shared draw helpers used by both the game view and the tower palette.
/// No image assets — everything is drawn with QPainter.
/// All entity-color functions take flat POD views (no model class hierarchy).
namespace theme {

constexpr double kPi = 3.14159265358979;

// --- string-keyed colors (by type/terrain name) -----------------------------
// These are the source of truth; the view-driven wrappers below delegate to
// them. The palette/editor/help also call them directly by type name.

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

inline QColor enemyColorForType(std::string const& type) {
  if (type == "fast") return QColor(243, 156, 18);
  if (type == "armored") return QColor(127, 140, 141);
  if (type == "resistant") return QColor(22, 160, 133);
  if (type == "splitter") return QColor(142, 68, 173);
  if (type == "boss") return QColor(192, 57, 43);
  return QColor(231, 76, 60);  // normal
}

inline QColor bulletColorForType(std::string const& type) {
  if (type == "slow") return QColor(52, 152, 219);
  if (type == "poison") return QColor(39, 174, 96);
  if (type == "splash") return QColor(230, 126, 34);
  if (type == "laser") return QColor(155, 89, 182);
  return QColor(231, 76, 60);  // normal
}

inline bool isRectTower(std::string const& type) { return type == "resource" || type == "wall"; }

// --- view-driven colors (delegate to the string-keyed helpers above) --------

inline QColor terrainColor(TileView const& c) { return terrainColorForName(c.terrain); }
inline QColor towerColor(TowerView const& t) { return towerColorForType(t.kind); }
inline QColor enemyColor(EnemyView const& e) { return enemyColorForType(e.kind); }
inline QColor bulletColor(BulletView const& b) { return bulletColorForType(b.kind); }

// --- draw helpers -----------------------------------------------------------

/// Draw a rectangular "muzzle"/barrel from the center of `r` in direction `dir`.
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

/// Draw a tower from its view (kind determines shape/color; aim draws the muzzle).
inline void drawTower(QPainter& p, TowerView const& t, QRectF const& r) {
  bool is_rect = isRectTower(t.kind);     // resource/wall
  if (!is_rect) drawMuzzle(p, r, t.aim);  // attack towers
  drawTowerBody(p, towerColor(t), is_rect, r, t.kind == "wall" ? 3 : 2);
}

/// Draw a tower preview by type string (default muzzle direction). For palette icons.
inline void drawTowerPreview(QPainter& p, std::string const& type, QRectF const& r) {
  bool is_rect = isRectTower(type);
  if (!is_rect) drawMuzzle(p, r, Vec2{1.0f, 0.0f});
  drawTowerBody(p, towerColorForType(type), is_rect, r, type == "wall" ? 3 : 2);
}

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
inline QColor gridLineColor() { return QColor(0, 0, 0, 40); }
inline QColor spawnMarkerColor() { return QColor(30, 220, 30); }
inline QColor exitMarkerColor() { return QColor(220, 30, 30); }
inline QColor pathTileColor() { return QColor(141, 110, 70); }
inline QColor pathLineColor() { return Qt::white; }
inline QColor hoverHighlightColor() { return QColor(255, 255, 0, 200); }
inline QColor enemyGlyphColor() { return QColor(200, 200, 200); }
inline QColor hpBarBackgroundColor() { return QColor(0, 0, 0, 160); }
inline QColor hpBarColor(float pct) { return QColor::fromHsvF(pct * 0.33f, 1.0f, 0.8f); }
inline QColor placementGhostColor(bool ok) {
  return ok ? QColor(255, 255, 255, 80) : QColor(255, 0, 0, 80);
}
inline QColor slowRingColor() { return QColor(52, 152, 219); }
inline QColor poisonRingColor() { return QColor(39, 174, 96); }
inline QColor regenRingColor() { return QColor(241, 196, 15); }
inline QColor warningTextColor() { return QColor(192, 57, 43); }
inline QColor withAlpha(QColor c, int alpha) { return QColor(c.red(), c.green(), c.blue(), alpha); }

}  // namespace theme
