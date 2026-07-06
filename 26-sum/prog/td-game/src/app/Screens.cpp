#include "Screens.h"

#include <QApplication>
#include <QBrush>
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

#include "GameController.h"
#include "GameWidget.h"
#include "Theme.h"

namespace {
QLabel* makeIconLabel(std::function<void(QPainter&, QRectF)> draw_fn, int size = 32) {
  auto* lbl = new QLabel;
  QPixmap pm(size, size);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  int inset = size / 8;
  draw_fn(p, QRectF(inset, inset, size - 2 * inset, size - 2 * inset));
  p.end();
  lbl->setPixmap(pm);
  lbl->setAlignment(Qt::AlignCenter);
  return lbl;
}

struct Info {
  const char* type;
  const char* desc;
};

/// Draw a status-effect ring around an enemy glyph: a colored ring with a
/// neutral enemy body inside, matching the in-game status-ring rendering.
void drawStatusIcon(QPainter& p, QRectF r, QColor ring) {
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(ring, std::max(1.0, r.width() * 0.1)));
  p.drawEllipse(r);
  QPointF c = r.center();
  float rad = r.width() * 0.35f;
  p.setBrush(QBrush(theme::enemyGlyphColor()));
  p.setPen(QPen(Qt::black, 1));
  p.drawEllipse(c, rad, rad);
}

/// A gold coin (HUD resource icon): filled disc with a darker inner ring.
void drawCoinIcon(QPainter& p, QRectF r) {
  p.setBrush(QColor(241, 196, 15));
  p.setPen(QPen(QColor(120, 88, 0), 1));
  p.drawEllipse(r);
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(120, 88, 0, 200), 1));
  double inset = r.width() * 0.18;
  p.drawEllipse(r.adjusted(inset, inset, -inset, -inset));
}

/// A five-point star (HUD score icon).
void drawStarIcon(QPainter& p, QRectF r) {
  QPolygonF star;
  QPointF c = r.center();
  double r_out = std::min(r.width(), r.height()) * 0.5;
  double r_in = r_out * 0.4;
  for (int i = 0; i < 10; ++i) {
    double rad = (i % 2 == 0) ? r_out : r_in;
    double ang = (i * 36.0 - 90.0) * theme::kPi / 180.0;  // start pointing up
    star << QPointF(c.x() + rad * std::cos(ang), c.y() + rad * std::sin(ang));
  }
  p.setBrush(QColor(87, 184, 255));
  p.setPen(QPen(QColor(20, 80, 130), 1));
  p.drawPolygon(star);
}

// Shared button styling — applied to both the HUD command buttons and the tower
// palette buttons so the two panels read as one UI. The :checked state
// highlights the selected tower in the palette (harmless on the HUD buttons).
constexpr char const* const kButtonStyle = R"(
  QPushButton {
    background: #dfe1e4; color: #5b6168; border: none;
    padding: 5px 14px; border-radius: 5px; font-weight: bold;
  }
  QPushButton:hover { background: #e8eaed; }
  QPushButton:pressed { background: #cdd0d4; }
  QPushButton:checked { background: #7289da; color: #ffffff; }
)";

// HUD panel: dark toolbar, muted captions, soft-gray values, and colored
// gold/score "chips". Button rules are layered on separately via kButtonStyle.
constexpr char const* const kHudPanelStyle = R"(
  Hud { background: #2f3136; }
  QLabel { color: #8e9297; }
  QLabel#value { color: #a8abb0; font-weight: bold; }
  QLabel#goldValue {
    color: #e5ba0d; background: #efebd9; font-weight: bold; font-size: 13px;
    padding: 1px 8px; border-radius: 6px;
  }
  QLabel#scoreValue {
    color: #57b8ff; background: #e1eaf2; font-weight: bold; font-size: 13px;
    padding: 1px 8px; border-radius: 6px;
  }
)";

// Result screens (victory / defeat): dark backdrop with a centered card panel.
// The card background + title color are layered on per screen (green / red).
constexpr char const* const kResultScreenStyle = R"(
  VictoryScreen, DefeatScreen { background: #2f3136; }
  QLabel { color: #dcddde; }
  QLabel#title { font-size: 26px; font-weight: bold; }
  QLabel#summary { color: #3b3e45; font-size: 14px; }
  QFrame#card { border-radius: 14px; }
)";

QWidget* makeHelp() {
  auto* help = new QGroupBox("How to Play");
  auto* help_lay = new QVBoxLayout(help);
  auto* hg = new QGridLayout;
  int const cols = 3;  // items per row
  int r = 0, c = 0;

  auto addHeader = [&](QString const& text) {
    if (c > 0) {
      r++;
      c = 0;
    }
    hg->addWidget(new QLabel("<b>" + text + "</b>"), r, 0, 1, cols * 2);
    r++;
  };
  auto addItem = [&](QLabel* icon, QString const& desc) {
    hg->addWidget(icon, r, c * 2);
    hg->addWidget(new QLabel(desc), r, c * 2 + 1);
    c++;
    if (c >= cols) {
      c = 0;
      r++;
    }
  };

  // Towers
  addHeader("Towers (build with palette, then click map)");
  for (auto const& [type, desc] : {
         Info{"normal", "single-target damage"},
         Info{"slow", "slows enemies"},
         Info{"poison", "damages + roots"},
         Info{"splash", "area damage"},
         Info{"laser", "piercing ray"},
         Info{"resource", "generates gold"},
         Info{"wall", "blocks enemies (high HP)"},
       }) {
    addItem(
      makeIconLabel([&, type](QPainter& p, QRectF rect) {
        theme::drawTowerPreview(p, type, rect);
      }),
      QString(type) + " — " + desc
    );
  }

  // Enemies
  addHeader("Enemies");
  for (auto const& [type, desc] : {
         Info{"normal", "standard"},
         Info{"fast", "low HP, high speed"},
         Info{"armored", "high HP, slow"},
         Info{"resistant", "shrugs slow & splash"},
         Info{"splitter", "spawns children on death"},
         Info{"boss", "shield + regen"},
       }) {
    QColor color = theme::enemyColorForType(type);
    addItem(
      makeIconLabel([color](QPainter& p, QRectF rect) {
        p.setBrush(QBrush(color));
        p.setPen(QPen(Qt::black, 1));
        p.drawEllipse(rect);
      }),
      QString(type) + " — " + desc
    );
  }

  // Cells
  addHeader("Tiles");
  for (auto const& [type, desc] : {
         Info{"grass", "buildable"},
         Info{"fertile", "cheaper towers"},
         Info{"rock", "blocked"},
         Info{"ice", "enemies faster, slow stronger"},
         Info{"portal", "teleports"},
       }) {
    QColor color = theme::terrainColorForName(type);
    addItem(
      makeIconLabel([color](QPainter& p, QRectF rect) {
        p.fillRect(rect, color);
        p.setPen(QPen(Qt::black, 1));
        p.drawRect(rect);
      }),
      QString(type) + " — " + desc
    );
  }

  // Markers
  addHeader("Markers");
  addItem(
    makeIconLabel([](QPainter& p, QRectF rect) {
      p.setPen(QPen(theme::spawnMarkerColor(), 3));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(rect);
    }),
    "spawn — where enemies appear"
  );
  addItem(
    makeIconLabel([](QPainter& p, QRectF rect) {
      p.setPen(QPen(theme::exitMarkerColor(), 3));
      QPointF c = rect.center();
      float h = rect.width() * 0.4f;
      p.drawLine(QPointF(c.x() - h, c.y() - h), QPointF(c.x() + h, c.y() + h));
      p.drawLine(QPointF(c.x() - h, c.y() + h), QPointF(c.x() + h, c.y() - h));
    }),
    "exit — enemies reaching it defeat you"
  );

  // Status effects (colored ring drawn around an affected enemy)
  addHeader("Status effects");
  auto add_status = [&](QColor color, char const* name, char const* desc) {
    addItem(
      makeIconLabel([color](QPainter& p, QRectF rect) { drawStatusIcon(p, rect, color); }),
      QString(name) + " — " + desc
    );
  };
  add_status(theme::slowRingColor(), "slow", "moving slower");
  add_status(theme::poisonRingColor(), "poison", "taking damage");
  add_status(theme::regenRingColor(), "regen", "healing over time");

  if (c > 0) r++;  // flush partial row
  help_lay->addLayout(hg);
  help_lay->addWidget(new QLabel(
    "The enemy path is hidden — a green circle marks each spawn, "
    "a red cross marks each exit. A colored ring around an enemy marks a "
    "status effect. Watch the enemies to learn the route.<br>"
    "<b>Controls</b>: click a tower button then click the map to build. "
    "Space = pause/resume, R = restart, Quit = back to menu.<br>"
    "<b>Cheats</b>: G = +1000 gold, K = kill all enemies, W = instant win."
  ));

  return help;
}

/// Sidebar legend: colored swatch + name for tiles, enemies, markers, and
/// status effects. Two entries per row. Shared by the game screen sidebar.
QWidget* makeLegend() {
  auto* box = new QGroupBox("Legend");
  auto* grid = new QGridLayout(box);
  grid->setSpacing(2);
  grid->setContentsMargins(6, 6, 6, 6);
  int row = 0;
  int col = 0;  // 0 or 1: which of the two entry-columns

  auto flush = [&] {
    if (col != 0) {
      row++;
      col = 0;
    }
  };
  auto add_header = [&](QString const& text) {
    flush();
    grid->addWidget(new QLabel("<b>" + text + "</b>"), row, 0, 1, 4);
    row++;
  };
  auto add_swatch = [&](char const* name, QColor c, bool round) {
    auto* sw = new QLabel;
    sw->setFixedSize(14, 14);
    QString style = QString("background-color:%1; border:1px solid black;").arg(c.name());
    if (round) style += " border-radius:7px;";
    sw->setStyleSheet(style);
    grid->addWidget(sw, row, col * 2);
    grid->addWidget(new QLabel(name), row, col * 2 + 1);
    col++;
    if (col >= 2) {
      col = 0;
      row++;
    }
  };
  auto add_pixmap = [&](char const* name, QPixmap pm) {
    auto* sw = new QLabel;
    sw->setFixedSize(14, 14);
    sw->setPixmap(pm);
    grid->addWidget(sw, row, col * 2);
    grid->addWidget(new QLabel(name), row, col * 2 + 1);
    col++;
    if (col >= 2) {
      col = 0;
      row++;
    }
  };
  // Render a 14x14 pixmap with `draw_fn` for marker/status entries.
  auto make_pixmap = [](std::function<void(QPainter&, QRectF)> draw_fn) {
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    draw_fn(painter, QRectF(1, 1, 12, 12));
    painter.end();
    return pm;
  };

  add_header("Tiles");
  for (auto const* t : {"grass", "fertile", "rock", "ice", "portal"}) {
    add_swatch(t, theme::terrainColorForName(t), false);
  }

  add_header("Enemies");
  for (auto const* t : {"normal", "fast", "armored", "resistant", "splitter", "boss"}) {
    add_swatch(t, theme::enemyColorForType(t), true);
  }

  add_header("Markers");
  add_pixmap("spawn", make_pixmap([](QPainter& p, QRectF r) {
               p.setPen(QPen(theme::spawnMarkerColor(), 2));
               p.setBrush(Qt::NoBrush);
               p.drawEllipse(r);
             }));
  add_pixmap("exit", make_pixmap([](QPainter& p, QRectF r) {
               p.setPen(QPen(theme::exitMarkerColor(), 2));
               QPointF c = r.center();
               float h = r.width() * 0.4f;
               p.drawLine(QPointF(c.x() - h, c.y() - h), QPointF(c.x() + h, c.y() + h));
               p.drawLine(QPointF(c.x() - h, c.y() + h), QPointF(c.x() + h, c.y() - h));
             }));

  add_header("Status");
  add_pixmap("slow", make_pixmap([](QPainter& p, QRectF r) {
               drawStatusIcon(p, r, theme::slowRingColor());
             }));
  add_pixmap("poison", make_pixmap([](QPainter& p, QRectF r) {
               drawStatusIcon(p, r, theme::poisonRingColor());
             }));
  add_pixmap("regen", make_pixmap([](QPainter& p, QRectF r) {
               drawStatusIcon(p, r, theme::regenRingColor());
             }));

  flush();
  return box;
}
}  // namespace

StartMenu::StartMenu(std::vector<LevelRegistry::Info> infos, QWidget* parent) : QWidget(parent) {
  auto* lay = new QVBoxLayout(this);
  auto* title = new QLabel("<h1>Tower Defense</h1>");
  title->setAlignment(Qt::AlignCenter);
  lay->addWidget(title);

  // --- level rows ---
  level_grid_ = new QGridLayout;
  auto* hdr1 = new QLabel("<b>Level</b>");
  auto* hdr2 = new QLabel("<b>Cleared?</b>");
  auto* hdr3 = new QLabel("<b>Max Score</b>");
  hdr1->setAlignment(Qt::AlignCenter);
  hdr2->setAlignment(Qt::AlignCenter);
  hdr3->setAlignment(Qt::AlignCenter);
  level_grid_->addWidget(hdr1, 0, 0);
  level_grid_->addWidget(hdr2, 0, 1);
  level_grid_->addWidget(hdr3, 0, 2);
  buildLevelRows(infos);
  lay->addLayout(level_grid_);

  lay->addWidget(makeHelp());

  auto* editor_btn_row = new QHBoxLayout;
  editor_btn_row->addStretch();
  auto* editor_btn = new QPushButton("Level Editor");
  editor_btn->setFixedWidth(150);
  connect(editor_btn, &QPushButton::clicked, this, &StartMenu::editorRequested);
  editor_btn_row->addWidget(editor_btn);
  editor_btn_row->addStretch();
  lay->addLayout(editor_btn_row);

  auto* quit_btn_row = new QHBoxLayout;
  quit_btn_row->addStretch();
  auto* quit_btn = new QPushButton("Quit");
  quit_btn->setFixedWidth(150);
  connect(quit_btn, &QPushButton::clicked, this, &StartMenu::quitRequested);
  quit_btn_row->addWidget(quit_btn);
  quit_btn_row->addStretch();
  lay->addLayout(quit_btn_row);

  lay->addStretch();
}

void StartMenu::buildLevelRows(std::vector<LevelRegistry::Info> const& infos) {
  int display_row = 0;
  for (std::size_t i = 0; i < infos.size(); ++i) {
    QString label =
      QString("Level %1: %2").arg(infos[i].index).arg(QString::fromStdString(infos[i].name));
    auto* btn = new QPushButton(label);
    connect(btn, &QPushButton::clicked, this, [this, i] {
      emit startRequested(static_cast<int>(i));
    });
    auto* cleared = new QLabel("No");
    auto* score = new QLabel("—");
    cleared->setAlignment(Qt::AlignCenter);
    score->setAlignment(Qt::AlignCenter);
    level_grid_->addWidget(btn, display_row + 1, 0);
    level_grid_->addWidget(cleared, display_row + 1, 1);
    level_grid_->addWidget(score, display_row + 1, 2);
    level_buttons_.append(btn);
    cleared_labels_.append(cleared);
    score_labels_.append(score);
    ++display_row;
  }
}

void StartMenu::setProgress(QVector<LevelProgress> const& progress) {
  for (int i = 0; i < progress.size() && i < cleared_labels_.size(); ++i) {
    cleared_labels_[i]->setText(progress[i].cleared ? "Yes" : "No");
    score_labels_[i]->setText(
      progress[i].max_score > 0 ? QString::number(progress[i].max_score) : "—"
    );
  }
}

TowerPalette::TowerPalette(GameController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller), group_(new QButtonGroup(this)) {
  group_->setExclusive(false);  // manual exclusivity so the selected tower can be unselected
  // Match the HUD's button styling so the palette feels like part of the same UI.
  setStyleSheet(kButtonStyle);

  // Rebuild on (re)start.
  connect(controller_, &GameController::levelStarted, this, &TowerPalette::rebuild);

  auto* lay = new QVBoxLayout(this);

  // Status label lives at the bottom of the palette, under the buttons.
  status_label_ = new QLabel("Not enough resource!", this);
  status_label_->setStyleSheet(
    QString("color: %1; font-weight: bold; font-size: 14px").arg(theme::warningTextColor().name())
  );
  status_label_->setAlignment(Qt::AlignCenter);
  status_label_->hide();
  lay->addWidget(status_label_);
  lay->addStretch();

  status_timer_ = new QTimer(this);
  status_timer_->setSingleShot(true);
  connect(status_timer_, &QTimer::timeout, this, [this] { status_label_->hide(); });

  rebuild();
}

void TowerPalette::rebuild() {
  auto* lay = qobject_cast<QVBoxLayout*>(layout());
  for (auto* btn : group_->buttons()) {
    lay->removeWidget(btn);
    delete btn;
  }

  auto const& g = controller_->game();
  // Insert buttons at the top so the status label + stretch stay below them.
  int idx = 0;
  for (auto const& type : g.level().available_towers) {
    auto it = g.level().tower_stats.find(type);
    int cost = (it != g.level().tower_stats.end()) ? it->second.cost : 0;

    // Preview icon drawn the same way the tower appears in-game.
    QPixmap icon(40, 40);
    icon.fill(Qt::transparent);
    QPainter ip(&icon);
    ip.setRenderHint(QPainter::Antialiasing);
    theme::drawTowerPreview(ip, type, QRectF(4, 4, 32, 32));
    ip.end();

    auto* btn = new QPushButton;
    btn->setIcon(icon);
    btn->setIconSize(QSize(32, 32));
    btn->setText(QString::fromStdString(type) + " (" + QString::number(cost) + ")");
    btn->setCheckable(true);
    btn->setFocusPolicy(Qt::NoFocus);  // keep keyboard focus on the board
    group_->addButton(btn);
    lay->insertWidget(idx, btn);
    // Clicking a selected tower again unselects it (manual exclusivity, since a
    // checkable button in a non-exclusive group toggles off on re-click).
    connect(btn, &QPushButton::clicked, this, [this, type, btn] {
      if (btn->isChecked()) {
        for (auto* b : group_->buttons())
          if (b != btn) static_cast<QPushButton*>(b)->setChecked(false);
        emit typeSelected(QString::fromStdString(type));
      } else {
        emit typeSelected(QString());  // unselect
      }
    });
    ++idx;
  }
}

void TowerPalette::showInsufficientResource() {
  status_label_->show();
  status_timer_->start(1500);  // visible for 1.5 seconds
}

Hud::Hud(GameController* controller, QWidget* parent) : QWidget(parent), controller_(controller) {
  level_ = new QLabel;
  level_->setObjectName("value");
  wave_ = new QLabel;
  wave_->setObjectName("value");
  resources_ = new QLabel;
  resources_->setMinimumWidth(60);
  resources_->setAlignment(Qt::AlignCenter);
  resources_->setObjectName("goldValue");
  score_ = new QLabel;
  score_->setMinimumWidth(60);
  score_->setAlignment(Qt::AlignCenter);
  score_->setObjectName("scoreValue");
  time_ = new QLabel;
  time_->setObjectName("value");
  fps_ = new QLabel;
  fps_->setObjectName("value");
  pause_btn_ = new QPushButton("Pause");
  pause_btn_->setMinimumWidth(80);
  pause_btn_->setFocusPolicy(Qt::NoFocus);  // keep keyboard focus on the board

  // Dark toolbar panel with muted captions, soft-gray values, and colored
  // gold/score chips; button style is shared with the tower palette.
  setStyleSheet(QString::fromLatin1(kHudPanelStyle) + QString::fromLatin1(kButtonStyle));

  auto* lay = new QHBoxLayout(this);
  lay->setContentsMargins(8, 6, 8, 6);
  lay->setSpacing(6);
  lay->addWidget(new QLabel("Level:"));
  lay->addWidget(level_);
  lay->addSpacing(16);
  lay->addWidget(new QLabel("Wave:"));
  lay->addWidget(wave_);
  lay->addSpacing(16);
  lay->addWidget(makeIconLabel([](QPainter& p, QRectF r) { drawCoinIcon(p, r); }, 18));
  lay->addWidget(new QLabel("Gold:"));
  lay->addWidget(resources_);
  lay->addSpacing(16);
  lay->addWidget(makeIconLabel([](QPainter& p, QRectF r) { drawStarIcon(p, r); }, 18));
  lay->addWidget(new QLabel("Score:"));
  lay->addWidget(score_);
  lay->addSpacing(16);
  lay->addWidget(new QLabel("Time:"));
  lay->addWidget(time_);
  lay->addSpacing(16);
  lay->addWidget(new QLabel("FPS:"));
  lay->addWidget(fps_);
  lay->addStretch();
  lay->addWidget(pause_btn_);

  auto* restart_btn = new QPushButton("Restart");
  restart_btn->setMinimumWidth(80);
  restart_btn->setFocusPolicy(Qt::NoFocus);
  lay->addWidget(restart_btn);
  auto* quit_btn = new QPushButton("Quit");
  quit_btn->setMinimumWidth(80);
  quit_btn->setFocusPolicy(Qt::NoFocus);
  lay->addWidget(quit_btn);
  restart_btn_ = restart_btn;
  quit_btn_ = quit_btn;

  connect(pause_btn_, &QPushButton::clicked, controller_, &GameController::togglePause);
  connect(restart_btn_, &QPushButton::clicked, controller_, &GameController::restartLevel);
  connect(quit_btn_, &QPushButton::clicked, this, &Hud::quitRequested);
  connect(controller_, &GameController::ticked, this, &Hud::refresh);
  // Hide buttons when the game ends; show them again on restart.
  connect(controller_, &GameController::stateChanged, this, [this] {
    if (controller_->game().state() != Game::State::Playing) setControlsVisible(false);
  });
  connect(controller_, &GameController::levelStarted, this, [this] { setControlsVisible(true); });

  refresh();
}

void Hud::refresh() {
  auto const& g = controller_->game();
  resources_->setText(QString::number(g.resource().amount()));
  wave_->setText(QString::number(g.current_wave()));
  auto const& lvl = g.level();
  level_->setText(
    lvl.index >= 1 ? QString("%1. %2").arg(lvl.index).arg(QString::fromStdString(lvl.name))
                   : QString("Custom: %1").arg(QString::fromStdString(lvl.name))
  );
  score_->setText(QString::number(g.score()));
  time_->setText(QString::number(g.elapsed_time(), 'f', 1) + "s");
  fps_->setText(QString::number(controller_->fps(), 'f', 0));
  pause_btn_->setText(g.paused() ? "Resume" : "Pause");
}

void Hud::setControlsVisible(bool visible) {
  if (!visible) {
    // Lock the HUD height so hiding buttons doesn't shrink it and push
    // the game board upward.
    int h = height();
    pause_btn_->setVisible(false);
    restart_btn_->setVisible(false);
    quit_btn_->setVisible(false);
    setFixedHeight(h);
  } else {
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    pause_btn_->setVisible(true);
    restart_btn_->setVisible(true);
    quit_btn_->setVisible(true);
  }
}

GameScreen::GameScreen(GameController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
  widget_ = new GameWidget(controller_);
  hud_ = new Hud(controller_);
  palette_ = new TowerPalette(controller_);
  palette_->setFixedWidth(180);

  auto* lay = new QVBoxLayout(this);
  lay->addWidget(hud_);
  auto* mid = new QHBoxLayout;
  mid->addWidget(widget_, 1);
  auto* sidebar = new QVBoxLayout;
  sidebar->addWidget(palette_);
  sidebar->addWidget(makeLegend());
  mid->addLayout(sidebar);
  lay->addLayout(mid, 1);

  // Wire the screen's widgets to the controller.
  connect(palette_, &TowerPalette::typeSelected, controller_, &GameController::selectTowerType);
  connect(controller_, &GameController::placementFailed, this, [] { QApplication::beep(); });
  connect(
    controller_,
    &GameController::insufficientResource,
    palette_,
    &TowerPalette::showInsufficientResource
  );
  // HUD "Quit" propagates up so MainWindow can return to the menu.
  connect(hud_, &Hud::quitRequested, this, &GameScreen::quitRequested);
}

void GameScreen::startLoop() {
  widget_->startLoop();
  hud_->setControlsVisible(true);
}
void GameScreen::stopLoop() { widget_->stopLoop(); }

VictoryScreen::VictoryScreen(QWidget* parent) : QWidget(parent) {
  setStyleSheet(
    QString::fromLatin1(kResultScreenStyle) + QString::fromLatin1(kButtonStyle) +
    "QFrame#card { background: #d7f0db; }"
    " QLabel#title { color: #1e8449; }"
    " QPushButton { background: #27ae60; color: #ffffff; }"
    " QPushButton:hover { background: #2ecc71; }"
    " QPushButton:pressed { background: #1e8449; }"
  );

  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addStretch();

  auto* center = new QHBoxLayout;
  center->addStretch();
  auto* card = new QFrame;
  card->setObjectName("card");
  card->setFixedWidth(360);
  auto* card_lay = new QVBoxLayout(card);
  card_lay->setContentsMargins(28, 28, 28, 24);
  card_lay->setSpacing(10);

  auto* title = new QLabel("Level Cleared!");
  title->setObjectName("title");
  title->setAlignment(Qt::AlignCenter);
  card_lay->addWidget(title);

  summary_ = new QLabel;
  summary_->setObjectName("summary");
  summary_->setAlignment(Qt::AlignCenter);
  card_lay->addWidget(summary_);

  card_lay->addSpacing(14);

  auto* btn_row = new QHBoxLayout;
  btn_row->setSpacing(10);
  next_btn_ = new QPushButton("Next Level");
  next_btn_->setFixedWidth(140);
  next_btn_->setFocusPolicy(Qt::NoFocus);
  connect(next_btn_, &QPushButton::clicked, this, &VictoryScreen::nextRequested);
  btn_row->addWidget(next_btn_);
  auto* menu = new QPushButton("Main Menu");
  menu->setFixedWidth(140);
  menu->setFocusPolicy(Qt::NoFocus);
  connect(menu, &QPushButton::clicked, this, &VictoryScreen::menuRequested);
  btn_row->addWidget(menu);
  menu_btn_ = menu;
  card_lay->addLayout(btn_row);

  center->addWidget(card);
  center->addStretch();
  outer->addLayout(center);
  outer->addStretch();
}

void VictoryScreen::showResult(int score, float time, bool has_next, bool from_editor) {
  summary_->setText(QString("Score: %1   Time: %2s").arg(score).arg(time, 0, 'f', 1));
  next_btn_->setVisible(has_next && !from_editor);
  menu_btn_->setText(from_editor ? "Back to Editor" : "Main Menu");
}

DefeatScreen::DefeatScreen(QWidget* parent) : QWidget(parent) {
  setStyleSheet(
    QString::fromLatin1(kResultScreenStyle) + QString::fromLatin1(kButtonStyle) +
    "QFrame#card { background: #f4d7d7; }"
    " QLabel#title { color: #c0392b; }"
    " QPushButton { background: #c0392b; color: #ffffff; }"
    " QPushButton:hover { background: #e74c3c; }"
    " QPushButton:pressed { background: #962d22; }"
  );

  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addStretch();

  auto* center = new QHBoxLayout;
  center->addStretch();
  auto* card = new QFrame;
  card->setObjectName("card");
  card->setFixedWidth(360);
  auto* card_lay = new QVBoxLayout(card);
  card_lay->setContentsMargins(28, 28, 28, 24);
  card_lay->setSpacing(10);

  auto* title = new QLabel("Defeated");
  title->setObjectName("title");
  title->setAlignment(Qt::AlignCenter);
  card_lay->addWidget(title);

  summary_ = new QLabel;
  summary_->setObjectName("summary");
  summary_->setAlignment(Qt::AlignCenter);
  card_lay->addWidget(summary_);

  card_lay->addSpacing(14);

  auto* btn_row = new QHBoxLayout;
  btn_row->setSpacing(10);
  auto* retry = new QPushButton("Retry Level");
  retry->setFixedWidth(140);
  retry->setFocusPolicy(Qt::NoFocus);
  connect(retry, &QPushButton::clicked, this, &DefeatScreen::retryRequested);
  btn_row->addWidget(retry);
  auto* menu = new QPushButton("Main Menu");
  menu->setFixedWidth(140);
  menu->setFocusPolicy(Qt::NoFocus);
  connect(menu, &QPushButton::clicked, this, &DefeatScreen::menuRequested);
  btn_row->addWidget(menu);
  menu_btn_ = menu;
  card_lay->addLayout(btn_row);

  center->addWidget(card);
  center->addStretch();
  outer->addLayout(center);
  outer->addStretch();
}

void DefeatScreen::showResult(int score, float time, bool from_editor) {
  summary_->setText(QString("Score: %1   Time: %2s").arg(score).arg(time, 0, 'f', 1));
  menu_btn_->setText(from_editor ? "Back to Editor" : "Main Menu");
}
