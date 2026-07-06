#include "GameWidget.h"

#include <QBrush>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QTimer>
#include <algorithm>

#include "GameController.h"
#include "Theme.h"

GameWidget::GameWidget(GameController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
  setMouseTracking(true);
  setAutoFillBackground(false);
  setMinimumSize(400, 300);
  setFocusPolicy(Qt::StrongFocus);  // receive keyboard shortcuts

  timer_ = new QTimer(this);
  timer_->setTimerType(Qt::PreciseTimer);
  connect(timer_, &QTimer::timeout, this, &GameWidget::onTick);
  connect(controller_, &GameController::ticked, this, [this] { this->update(); });
  // On game over: show a banner; keep the timer running so effects animate
  // during the delay before MainWindow switches screens.
  connect(controller_, &GameController::stateChanged, this, [this](GameState s) {
    banner_text_ = (s == GameState::Victory) ? "Game cleared!" : "You lose!";
    banner_age_ = 0.0f;
    game_over_ = true;
    update();
  });
  connect(this, &GameWidget::clicked, controller_, &GameController::placeAt);

  // --- UI effects ---
  // Floating "-cost" / "+score" text on tower placement / enemy kill.
  connect(controller_, &GameController::towerPlaced, this, [this](Vec2 pos, int cost, QColor c) {
    QPointF p = toPixelCoord(pos);
    int j = static_cast<int>((float_texts_.size() * 7 + 3) % 17) - 8;  // perturb
    float_texts_.push_back({p + QPointF(j, j), "-" + QString::number(cost), c, 0.0f});
  });
  connect(controller_, &GameController::enemyKilled, this, [this](Vec2 pos, int score, QColor c) {
    QPointF p = toPixelCoord(pos);
    int j = static_cast<int>((float_texts_.size() * 7 + 3) % 17) - 8;
    float_texts_.push_back({p + QPointF(j, j), "+" + QString::number(score), c, 0.0f});
  });
  // Wave-start / game-over banner.
  connect(controller_, &GameController::waveStarted, this, [this](int wave, bool boss, bool last) {
    banner_text_ = QString("%1 wave! (No. %2)%3")
                     .arg(last ? "LAST" : "NEW")
                     .arg(wave)
                     .arg(boss ? "  [BOSS!]" : "");
    banner_age_ = 0.0f;
  });
  // Clear transient UI whenever a level (re)starts (stale text/banners must go).
  connect(controller_, &GameController::levelStarted, this, [this] {
    float_texts_.clear();
    banner_text_.clear();
    game_over_ = false;
    first_tick_ = true;  // reset dt baseline
    elapsed_.start();
  });
}

void GameWidget::startLoop() {
  first_tick_ = true;
  elapsed_.start();
  timer_->start(16);  // ~60 fps
  setFocus();         // ensure keyboard shortcuts reach this widget
  float_texts_.clear();
  banner_text_.clear();
  game_over_ = false;
}

void GameWidget::stopLoop() { timer_->stop(); }

void GameWidget::onTick() {
  float dt = first_tick_ ? 0.0f : elapsed_.restart() / 1000.0f;
  first_tick_ = false;
  if (dt > 0.1f) dt = 0.1f;  // clamp after stalls / debugger pauses

  if (game_over_) {
    // Game is over — don't advance the model, but keep animating float texts
    // and repainting so the game-over banner and lingering effects behave correctly.
    for (auto& ft : float_texts_) ft.age += dt;
    float_texts_.erase(
      std::remove_if(
        float_texts_.begin(),
        float_texts_.end(),
        [](FloatText const& ft) { return ft.age >= kFloatLifetime; }
      ),
      float_texts_.end()
    );
    update();
    return;
  }

  bool paused = controller_->paused();
  controller_->tick(dt);

  // Age transient effects only when the game is running.
  if (!paused) {
    for (auto& ft : float_texts_) ft.age += dt;
    float_texts_.erase(
      std::remove_if(
        float_texts_.begin(),
        float_texts_.end(),
        [](FloatText const& ft) { return ft.age >= kFloatLifetime; }
      ),
      float_texts_.end()
    );
    if (!banner_text_.isEmpty()) {
      banner_age_ += dt;
      if (banner_age_ >= kBannerLifetime) banner_text_.clear();
    }
  }
}

QSize GameWidget::sizeHint() const {
  // Natural size at 48px/tile; layout will scale to fill available space.
  return QSize(
    static_cast<int>(controller_->mapWidth()) * 48,
    static_cast<int>(controller_->mapHeight()) * 48
  );
}

void GameWidget::recomputeLayout() {
  float mw = controller_->mapWidth();
  float mh = controller_->mapHeight();
  if (mw <= 0 || mh <= 0) return;
  // Reserve space at the top for the wave/game-over banner.
  constexpr float banner_h = 30.0f;
  float sx = static_cast<float>(width()) / mw;
  float sy = (static_cast<float>(height()) - banner_h) / mh;
  tile_size_ = std::min(sx, sy);
  // Center the map in the area below the banner.
  offset_x_ = (static_cast<float>(width()) - tile_size_ * mw) * 0.5f;
  offset_y_ = banner_h + (static_cast<float>(height()) - banner_h - tile_size_ * mh) * 0.5f;
}

Vec2 GameWidget::toMapCoord(QPointF pixel) const {
  return Vec2{
    (static_cast<float>(pixel.x()) - offset_x_) / tile_size_,
    (static_cast<float>(pixel.y()) - offset_y_) / tile_size_
  };
}

QPointF GameWidget::toPixelCoord(Vec2 map_pos) const {
  return QPointF(offset_x_ + map_pos.x * tile_size_, offset_y_ + map_pos.y * tile_size_);
}

void GameWidget::paintEvent(QPaintEvent*) {
  auto lv = controller_->levelView();
  recomputeLayout();

  float cs = tile_size_;
  float ox = offset_x_;
  float oy = offset_y_;
  int cols = static_cast<int>(lv.map_width);
  QRectF map_rect(ox, oy, cs * lv.map_width, cs * lv.map_height);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // --- terrain ---
  for (std::size_t i = 0; i < lv.terrain.size(); ++i) {
    int col = static_cast<int>(i) % cols;
    int row = static_cast<int>(i) / cols;
    QRectF r(ox + col * cs, oy + row * cs, cs, cs);
    p.fillRect(r, theme::terrainColor(lv.terrain[i]));
    p.setPen(QPen(theme::gridLineColor()));
    p.drawRect(r);
  }

  // --- spawn markers: a green circle at the start of each enemy route ---
  float radius = cs * 0.40f;
  p.setPen(QPen(theme::spawnMarkerColor(), std::max(2.0f, cs * 0.12f)));
  for (auto const& wps : lv.path_waypoints) {
    if (wps.empty()) continue;
    QPointF c = toPixelCoord(wps.front());
    p.drawEllipse(c, radius, radius);
  }

  // --- exit markers: a red cross at the end of each enemy route ---
  float arm = cs * 0.30f;
  p.setPen(QPen(theme::exitMarkerColor(), std::max(2.0f, cs * 0.12f)));
  for (auto const& wps : lv.path_waypoints) {
    if (wps.empty()) continue;
    QPointF c = toPixelCoord(wps.back());
    p.drawLine(QPointF(c.x() - arm, c.y() - arm), QPointF(c.x() + arm, c.y() + arm));
    p.drawLine(QPointF(c.x() - arm, c.y() + arm), QPointF(c.x() + arm, c.y() - arm));
  }

  // --- towers ---
  float inset = cs * 0.12f;
  for (auto const& tv : controller_->towerViews()) {
    int col = static_cast<int>(tv.pos.x);
    int row = static_cast<int>(tv.pos.y);
    QRectF r(ox + col * cs + inset, oy + row * cs + inset, cs - 2 * inset, cs - 2 * inset);
    theme::drawTower(p, tv, r);
    if (tv.health < tv.max_health) {
      drawHpBar(
        p,
        static_cast<int>(ox + col * cs + cs * 0.1f),
        static_cast<int>(oy + row * cs + cs * 0.03f),
        static_cast<int>(cs * 0.8f),
        tv.health,
        tv.max_health
      );
    }
  }

  // --- enemies ---
  for (auto const& ev : controller_->enemyViews()) {
    QPointF ep = toPixelCoord(ev.pos);
    float rx = ev.half_width * cs;
    float ry = ev.half_height * cs;
    if (rx < 4) rx = 4;
    if (ry < 4) ry = 4;
    QColor c = theme::enemyColor(ev);

    // Status-effect rings from the status flags.
    std::vector<QColor> rings;
    if (ev.status_hint.slow) rings.push_back(theme::slowRingColor());
    if (ev.status_hint.poison) rings.push_back(theme::poisonRingColor());
    if (ev.status_hint.regen) rings.push_back(theme::regenRingColor());
    if (!rings.empty()) {
      float rrx = rx + 4;
      float rry = ry + 4;
      p.setBrush(Qt::NoBrush);
      if (rings.size() == 1) {
        p.setPen(QPen(rings[0], 3));
        p.drawEllipse(ep, rrx, rry);
      } else {
        QRectF ring_rect(ep.x() - rrx, ep.y() - rry, rrx * 2, rry * 2);
        QPen pen;
        pen.setWidthF(3);
        pen.setCapStyle(Qt::FlatCap);
        constexpr int kFullCircle = 5760;
        int span = kFullCircle / static_cast<int>(rings.size());
        for (std::size_t i = 0; i < rings.size(); ++i) {
          pen.setColor(rings[i]);
          p.setPen(pen);
          p.drawArc(ring_rect, 1440 + static_cast<int>(i) * span, span);
        }
      }
    }

    p.setBrush(QBrush(c));
    p.setPen(QPen(Qt::black, 1));
    p.drawEllipse(ep, rx, ry);

    drawHpBar(
      p,
      static_cast<int>(ep.x() - rx),
      static_cast<int>(ep.y() - ry - 8),
      static_cast<int>(rx * 2),
      ev.health,
      ev.max_health
    );
  }

  // --- bullets ---
  float map_pw = lv.map_width * cs;
  float map_ph = lv.map_height * cs;

  for (auto const& bv : controller_->bulletViews()) {
    QPointF bp = toPixelCoord(bv.pos);
    QColor c = theme::bulletColor(bv);
    p.setBrush(QBrush(c));
    p.setPen(Qt::NoPen);

    if (bv.kind == "laser") {
      Vec2 dir = bv.vel.normalized();
      float tx = (std::abs(dir.x) < 1e-6f)
                   ? 1e30f
                   : (dir.x > 0 ? (map_pw + ox - bp.x()) / dir.x : (ox - bp.x()) / dir.x);
      float ty = (std::abs(dir.y) < 1e-6f)
                   ? 1e30f
                   : (dir.y > 0 ? (map_ph + oy - bp.y()) / dir.y : (oy - bp.y()) / dir.y);
      float t = std::min(tx, ty);
      p.setPen(QPen(c, 3));
      p.drawLine(bp, QPointF(bp.x() + dir.x * t, bp.y() + dir.y * t));
    } else if (bv.kind == "splash") {
      float r = bv.radius * cs;
      p.setBrush(QBrush(theme::withAlpha(c, 40)));
      p.setPen(QPen(theme::withAlpha(c, 100), 1));
      p.drawEllipse(bp, r, r);
      p.setBrush(QBrush(c));
      p.setPen(Qt::NoPen);
      p.drawEllipse(bp, 4.0, 4.0);
    } else {
      p.drawEllipse(bp, 3.0, 3.0);
    }
  }

  // --- hover ghost ---
  if (controller_->state() == GameState::Playing && !controller_->selectedTowerType().isEmpty()) {
    if (controller_->canPlaceAt(hover_)) {
      int col = static_cast<int>(hover_.x);
      int row = static_cast<int>(hover_.y);
      p.fillRect(QRectF(ox + col * cs, oy + row * cs, cs, cs), theme::placementGhostColor(true));
    } else if (hover_.x >= 0 && hover_.y >= 0) {
      int col = static_cast<int>(hover_.x);
      int row = static_cast<int>(hover_.y);
      p.fillRect(QRectF(ox + col * cs, oy + row * cs, cs, cs), theme::placementGhostColor(false));
    }
  }

  // --- floating text (fade + drift upward) ---
  p.setFont(QFont("sans-serif", 12, QFont::Bold));
  for (auto const& ft : float_texts_) {
    float t = ft.age / kFloatLifetime;
    int alpha = static_cast<int>(255 * (1.0f - t));
    int yoff = static_cast<int>(20.0f * t);
    // Darken the shade so the text stands apart from a same-colored tower
    // body (notably the bright red normal tower) without a background or
    // outline. Moderate factor: enough to separate, not so much that it
    // disappears on dark terrain.
    p.setPen(QPen(theme::withAlpha(ft.color.darker(150), alpha)));
    p.drawText(QPointF(ft.pos.x(), ft.pos.y() - yoff), ft.text);
  }

  // --- banner (wave start / game over) ---
  if (!banner_text_.isEmpty()) {
    p.setFont(QFont("sans-serif", 16, QFont::Bold));
    QColor bc = banner_text_.startsWith("Game")
                  ? (banner_text_.contains("lose") ? Qt::red : Qt::darkGreen)
                  : theme::warningTextColor();
    p.setPen(QPen(bc));
    p.drawText(
      QRectF(map_rect.left(), map_rect.top() - 30, map_rect.width(), 24),
      Qt::AlignCenter,
      banner_text_
    );
  }

  // --- paused overlay ---
  // The loop keeps repainting while paused, so checking controller_->paused() here tracks
  // the state live. Drawn last so it sits above every gameplay layer.
  if (controller_->paused()) {
    QRectF
      box(0, 0, std::max(180.0, map_rect.width() * 0.45), std::max(80.0, map_rect.height() * 0.22));
    box.moveCenter(map_rect.center());

    p.setBrush(QColor(0, 0, 0, 127));
    p.setPen(QPen(QColor(255, 255, 255, 220), 2));
    p.drawRoundedRect(box, 14, 14);

    int title_pt = std::clamp(static_cast<int>(cs * 0.5), 18, 40);
    QRectF top = box, bottom = box;
    top.setBottom(box.center().y());
    bottom.setTop(box.center().y());

    p.setPen(QPen(Qt::white));
    p.setFont(QFont("sans-serif", title_pt, QFont::Bold));
    p.drawText(top, Qt::AlignCenter, "Paused");

    p.setPen(QPen(QColor(220, 220, 220)));
    p.setFont(QFont("sans-serif", std::max(9, title_pt / 3)));
    p.drawText(bottom, Qt::AlignCenter, "Press Space to resume");
  }
}

void GameWidget::drawHpBar(QPainter& p, int x, int y, int w, int hp, int max_hp) {
  if (max_hp <= 0) return;
  p.fillRect(x, y, w, 5, theme::hpBarBackgroundColor());
  float health_pct = std::clamp(float(hp) / float(max_hp), 0.0f, 1.0f);
  int filled = static_cast<int>(w * health_pct);
  p.fillRect(x, y, filled, 5, theme::hpBarColor(health_pct));
}

void GameWidget::mousePressEvent(QMouseEvent* event) {
  emit clicked(toMapCoord(event->position()));
}

void GameWidget::mouseMoveEvent(QMouseEvent* event) {
  hover_ = toMapCoord(event->position());
  update();
}

void GameWidget::leaveEvent(QEvent*) {
  hover_ = {-1.0f, -1.0f};
  update();
}

void GameWidget::keyPressEvent(QKeyEvent* event) {
  // Gameplay shortcuts; unhandled keys fall through to the default handler.
  switch (event->key()) {
  case Qt::Key_Space:
    controller_->togglePause();
    break;
  case Qt::Key_R:
    controller_->restartLevel();
    break;
  case Qt::Key_G:
    controller_->applyCheat("gold");
    break;
  case Qt::Key_K:
    controller_->applyCheat("killall");
    break;
  case Qt::Key_W:
    controller_->applyCheat("win");
    break;
  default:
    QWidget::keyPressEvent(event);
  }
}
