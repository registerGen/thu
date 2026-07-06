#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QPointF>
#include <QString>
#include <QWidget>
#include <vector>

#include "game/geometry.h"

class GameController;
class QTimer;

/// The in-game board: renders the map and entities (terrain, towers, enemies,
/// bullets, placement preview) with QPainter — tiles scale to fill the widget —
/// and serves as the single input hub for gameplay.
///
/// Almost every input (except HUD and TowerPalette) flows through here
/// into the GameController:
///   - mouse click -> clicked()      -> controller->placeAt
///   - keyboard    -> keyPressEvent  -> controller (pause/restart/cheats)
///   - timer tick  -> onTick         -> controller->tick(dt)
/// The widget owns the simulation QTimer, computes `dt` each frame, and stops
/// itself when the controller signals the level has ended. The controller stays
/// a view-agnostic wrapper over the Game model.
class GameWidget : public QWidget {
  Q_OBJECT

public:
  explicit GameWidget(GameController* controller, QWidget* parent = nullptr);

  QSize sizeHint() const override;

  /// Start/stop the simulation timer. Starting also grabs keyboard focus so
  /// shortcuts reach this widget.
  void startLoop();
  void stopLoop();

signals:
  /// A left-click on the board, in map-space coordinates.
  void clicked(Vec2 map_pos);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  /// Gameplay shortcuts: Space = pause, R = restart, G/K/W = cheats.
  void keyPressEvent(QKeyEvent* event) override;

private slots:
  /// Timer callback: compute `dt` and advance the model via the controller.
  void onTick();

private:
  Vec2 toMapCoord(QPointF pixel) const;
  QPointF toPixelCoord(Vec2 map_pos) const;
  void drawHpBar(QPainter& p, int x, int y, int w, int hp, int max_hp);
  /// Recompute tile_size_ and offsets to center the map in the widget.
  void recomputeLayout();

  GameController* controller_;
  Vec2 hover_{-1.0f, -1.0f};

  // Simulation timer (the model heartbeat) + dt baseline.
  QTimer* timer_;
  QElapsedTimer elapsed_;
  bool first_tick_ = true;

  // Dynamic layout: computed in recomputeLayout() from widget + map size.
  float tile_size_ = 64.0f;
  float offset_x_ = 0.0f;
  float offset_y_ = 0.0f;

  // --- transient UI effects (floating text + banners) ---
  struct FloatText {
    QPointF pos;
    QString text;
    QColor color;
    float age = 0.0f;
  };
  std::vector<FloatText> float_texts_;
  static constexpr float kFloatLifetime = 0.9f;

  QString banner_text_;
  float banner_age_ = 0.0f;
  static constexpr float kBannerLifetime = 2.0f;

  bool game_over_ = false;  // when true, skip model ticks but keep animating effects
};
