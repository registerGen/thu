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
  connect(controller_, &GameController::stateChanged, this, [this](Game::State s) {
    controller_->clearEntities();
    banner_text_ = (s == Game::State::Victory) ? "Game cleared!" : "You lose!";
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

  bool paused = controller_->game().paused();
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
  auto const& g = controller_->game();
  // Natural size at 48px/tile; layout will scale to fill available space.
  return QSize(static_cast<int>(g.map().width()) * 48, static_cast<int>(g.map().height()) * 48);
}

void GameWidget::recomputeLayout() {
  auto const& g = controller_->game();
  float mw = g.map().width();
  float mh = g.map().height();
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
  auto const& g = controller_->game();
  recomputeLayout();

  float cs = tile_size_;
  float ox = offset_x_;
  float oy = offset_y_;
  QRectF map_rect(ox, oy, cs * g.map().width(), cs * g.map().height());

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // --- terrain ---
  for (auto const& tile : g.map().tiles()) {
    int col = static_cast<int>(tile.position().x);
    int row = static_cast<int>(tile.position().y);
    QRectF r(ox + col * cs, oy + row * cs, cs, cs);
    p.fillRect(r, theme::terrainColor(tile));
    p.setPen(QPen(theme::gridLineColor()));
    p.drawRect(r);
  }

  // --- spawn markers: a green circle at the start of each enemy route ---
  float radius = cs * 0.40f;
  p.setPen(QPen(theme::spawnMarkerColor(), std::max(2.0f, cs * 0.12f)));
  for (auto const& route : g.level().routes) {
    auto const& wps = route.path.waypoints();
    if (wps.empty()) continue;
    QPointF c = toPixelCoord(wps.front());
    p.drawEllipse(c, radius, radius);
  }

  // --- exit markers: a red cross at the end of each enemy route ---
  float arm = cs * 0.30f;
  p.setPen(QPen(theme::exitMarkerColor(), std::max(2.0f, cs * 0.12f)));
  for (auto const& route : g.level().routes) {
    auto const& wps = route.path.waypoints();
    if (wps.empty()) continue;
    QPointF c = toPixelCoord(wps.back());
    p.drawLine(QPointF(c.x() - arm, c.y() - arm), QPointF(c.x() + arm, c.y() + arm));
    p.drawLine(QPointF(c.x() - arm, c.y() + arm), QPointF(c.x() + arm, c.y() - arm));
  }

  // --- towers ---
  float inset = cs * 0.12f;
  for (auto const& tower : g.towers()) {
    int col = static_cast<int>(tower->tile()->position().x);
    int row = static_cast<int>(tower->tile()->position().y);
    QRectF r(ox + col * cs + inset, oy + row * cs + inset, cs - 2 * inset, cs - 2 * inset);
    theme::drawTower(p, *tower, r);
    if (tower->health() < tower->max_health()) {
      drawHpBar(
        p,
        static_cast<int>(ox + col * cs + cs * 0.1f),
        static_cast<int>(oy + row * cs + cs * 0.03f),
        static_cast<int>(cs * 0.8f),
        tower->health(),
        tower->max_health()
      );
    }
  }

  // --- enemies ---
  for (auto const& enemy : g.enemies()) {
    if (enemy->is_destroyed()) continue;
    QPointF ep = toPixelCoord(enemy->position());
    float r = enemy->bounds().width * cs * 0.5f;
    if (r < 4) r = 4;
    QColor c = theme::enemyColor(*enemy);

    auto hint = enemy->status_hint();
    // Render every active effect as its own arc, splitting the ring into equal
    // slices so simultaneous effects are all visible.
    std::vector<QColor> rings;
    if (hint.slow) rings.push_back(theme::slowRingColor());
    if (hint.poison) rings.push_back(theme::poisonRingColor());
    if (hint.regen) rings.push_back(theme::regenRingColor());
    if (!rings.empty()) {
      float rr = r + 4;
      p.setBrush(Qt::NoBrush);
      if (rings.size() == 1) {
        p.setPen(QPen(rings[0], 3));
        p.drawEllipse(ep, rr, rr);
      } else {
        QRectF ring_rect(ep.x() - rr, ep.y() - rr, rr * 2, rr * 2);
        QPen pen;
        pen.setWidthF(3);
        pen.setCapStyle(Qt::FlatCap);  // adjacent arcs tile flush, no gaps/overlap
        // Angles are in 1/16° (a full circle is 5760). Start at 12 o'clock and
        // walk counter-clockwise so the first effect begins at the top.
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
    p.drawEllipse(ep, r, r);

    drawHpBar(
      p,
      static_cast<int>(ep.x() - r),
      static_cast<int>(ep.y() - r - 8),
      static_cast<int>(r * 2),
      enemy->health(),
      enemy->max_health()
    );
  }

  // --- bullets ---
  float map_pw = g.map().width() * cs;
  float map_ph = g.map().height() * cs;
  std::vector<std::pair<Bullet const*, QPointF>> cur_explosive;

  for (auto const& bullet : g.bullets()) {
    QPointF bp = toPixelCoord(bullet->position());
    QColor c = theme::bulletColor(*bullet);
    p.setBrush(QBrush(c));
    p.setPen(Qt::NoPen);

    if (dynamic_cast<LaserBullet const*>(bullet.get())) {
      Vec2 dir = bullet->velocity().normalized();
      float tx = (std::abs(dir.x) < 1e-6f)
                   ? 1e30f
                   : (dir.x > 0 ? (map_pw + ox - bp.x()) / dir.x : (ox - bp.x()) / dir.x);
      float ty = (std::abs(dir.y) < 1e-6f)
                   ? 1e30f
                   : (dir.y > 0 ? (map_ph + oy - bp.y()) / dir.y : (oy - bp.y()) / dir.y);
      float t = std::min(tx, ty);
      p.setPen(QPen(c, 3));
      p.drawLine(bp, QPointF(bp.x() + dir.x * t, bp.y() + dir.y * t));
    } else if (auto explosive = dynamic_cast<ExplosiveBullet const*>(bullet.get())) {
      float r = explosive->radius() * cs;
      p.setBrush(QBrush(theme::withAlpha(c, 40)));
      p.setPen(QPen(theme::withAlpha(c, 100), 1));
      p.drawEllipse(bp, r, r);
      p.setBrush(QBrush(c));
      p.setPen(Qt::NoPen);
      p.drawEllipse(bp, 4.0, 4.0);
      cur_explosive.push_back({bullet.get(), bp});
    } else {
      p.drawEllipse(bp, 3.0, 3.0);
    }
  }

  // --- hover ghost ---
  // Only show a ghost when the cursor is actually over a map tile. Without this
  // guard, a position above/left of the map gives a negative hover coord that
  // truncates to 0, snapping the ghost onto the top/left tile (below the cursor).
  if (g.state() == Game::State::Playing && !controller_->selectedTowerType().isEmpty()) {
    Tile const* tile = g.map().tile_at(hover_);
    if (tile) {
      int col = static_cast<int>(hover_.x);
      int row = static_cast<int>(hover_.y);
      bool ok = tile->can_place_tower() && !tile->occupied_by_tower();
      p.fillRect(QRectF(ox + col * cs, oy + row * cs, cs, cs), theme::placementGhostColor(ok));
    }
  }

  // --- floating text (fade + drift upward) ---
  p.setFont(QFont("sans-serif", 12, QFont::Bold));
  for (auto const& ft : float_texts_) {
    float t = ft.age / kFloatLifetime;
    int alpha = static_cast<int>(255 * (1.0f - t));
    int yoff = static_cast<int>(20.0f * t);
    p.setPen(QPen(theme::withAlpha(ft.color, alpha)));
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
  // The loop keeps repainting while paused, so checking g.paused() here tracks
  // the state live. Drawn last so it sits above every gameplay layer.
  if (g.paused()) {
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
