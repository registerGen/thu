#include "LevelEditor.h"

#include <QApplication>
#include <QBrush>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <filesystem>

#include "../third_party/crjson.h"
#include "GameController.h"
#include "Theme.h"
#include "game/config.h"
#include "game/game.h"

LevelEditor::LevelEditor(GameController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
  setMouseTracking(true);
  setMinimumSize(500, 500);

  // Initialize grid with one default path.
  grid_.rows = 7;
  grid_.cols = 12;
  grid_.terrain.resize(static_cast<std::size_t>(grid_.rows * grid_.cols), "grass");
  grid_.paths.push_back(EditorPath{});

  auto* lay = new QHBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);
  auto* panel = new QWidget;
  panel->setFixedWidth(225);
  auto* panel_lay = new QVBoxLayout(panel);
  panel_lay->setSpacing(2);
  panel_lay->setContentsMargins(6, 6, 6, 6);

  // Level name.
  panel_lay->addWidget(new QLabel("<b>Level</b>"));
  name_edit_ = new QLineEdit("Custom Level");
  panel_lay->addWidget(name_edit_);

  // Terrain palette + tools share one exclusive group so only one is active.
  panel_lay->addWidget(new QLabel("<b>Terrain</b>"));
  auto* tool_group = new QButtonGroup(this);
  tool_group->setExclusive(true);
  for (auto const& [name, label] : {
         std::pair<QString, QString>{"grass", "Grass"},
         {"fertile", "Fertile"},
         {"rock", "Rock"},
         {"ice", "Ice"},
       }) {
    auto* btn = new QPushButton(label);
    btn->setCheckable(true);
    if (name == "grass") btn->setChecked(true);
    tool_group->addButton(btn);
    panel_lay->addWidget(btn);
    connect(btn, &QPushButton::clicked, this, [this, name] {
      selected_terrain_ = name;
      tool_ = 0;
    });
  }

  // Tool buttons
  panel_lay->addWidget(new QLabel("<b>Tools</b>"));
  auto* path_btn = new QPushButton("Path Mode");
  path_btn->setCheckable(true);
  path_btn->setChecked(false);
  tool_group->addButton(path_btn);
  panel_lay->addWidget(path_btn);
  connect(path_btn, &QPushButton::clicked, this, [this] {
    tool_ = 1;
    portal_click_stage_ = 0;
  });

  auto* portal_btn = new QPushButton("Portal Mode");
  portal_btn->setCheckable(true);
  tool_group->addButton(portal_btn);
  panel_lay->addWidget(portal_btn);
  connect(portal_btn, &QPushButton::clicked, this, [this] {
    tool_ = 2;
    portal_click_stage_ = 0;
  });

  auto* clear_path_btn = new QPushButton("Clear Path");
  panel_lay->addWidget(clear_path_btn);
  connect(clear_path_btn, &QPushButton::clicked, this, &LevelEditor::onClearPath);

  // Path selector (multi-path support)
  panel_lay->addWidget(new QLabel("<b>Route</b>"));
  path_selector_ = new QComboBox;
  panel_lay->addWidget(path_selector_);
  connect(
    path_selector_,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    &LevelEditor::onSelectPath
  );
  auto* path_btns = new QHBoxLayout;
  auto* add_path_btn = new QPushButton("Add");
  path_btns->addWidget(add_path_btn);
  connect(add_path_btn, &QPushButton::clicked, this, &LevelEditor::onAddPath);
  auto* del_path_btn = new QPushButton("Del");
  path_btns->addWidget(del_path_btn);
  connect(del_path_btn, &QPushButton::clicked, this, &LevelEditor::onRemovePath);
  panel_lay->addLayout(path_btns);
  refreshPathSelector();

  // Map size
  panel_lay->addWidget(new QLabel("<b>Map Size</b>"));
  auto* size_lay = new QHBoxLayout;
  rows_spin_ = new QSpinBox;
  rows_spin_->setRange(5, 10);
  rows_spin_->setValue(grid_.rows);
  cols_spin_ = new QSpinBox;
  cols_spin_->setRange(9, 16);
  cols_spin_->setValue(grid_.cols);
  size_lay->addWidget(new QLabel("Rows:"));
  size_lay->addWidget(rows_spin_);
  size_lay->addWidget(new QLabel("Cols:"));
  size_lay->addWidget(cols_spin_);
  panel_lay->addLayout(size_lay);
  connect(rows_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &LevelEditor::onResize);
  connect(cols_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &LevelEditor::onResize);

  // Economy
  panel_lay->addWidget(new QLabel("<b>Economy</b>"));
  auto* gold_lay = new QHBoxLayout;
  gold_lay->addWidget(new QLabel("Gold:"));
  gold_spin_ = new QSpinBox;
  gold_spin_->setRange(50, 999);
  gold_spin_->setValue(150);
  gold_lay->addWidget(gold_spin_);
  panel_lay->addLayout(gold_lay);

  auto* auto_lay = new QHBoxLayout;
  auto_lay->addWidget(new QLabel("Auto +:"));
  auto_amt_spin_ = new QSpinBox;
  auto_amt_spin_->setRange(0, 20);
  auto_amt_spin_->setValue(4);
  auto_lay->addWidget(auto_amt_spin_);
  auto_lay->addWidget(new QLabel("every"));
  auto_int_spin_ = new QSpinBox;
  auto_int_spin_->setRange(1, 30);
  auto_int_spin_->setValue(1);
  auto_int_spin_->setSuffix("s");
  auto_lay->addWidget(auto_int_spin_);
  panel_lay->addLayout(auto_lay);

  // Tower availability
  panel_lay->addWidget(new QLabel("<b>Towers</b>"));
  for (auto const& name : {"normal", "slow", "poison", "splash", "laser", "resource", "wall"}) {
    auto* cb = new QCheckBox(QString(name));
    cb->setChecked(true);
    panel_lay->addWidget(cb);
    tower_checks_.push_back(cb);
  }

  // Waves: pick a difficulty, click Generate, or edit the JSON directly.
  panel_lay->addWidget(new QLabel("<b>Waves</b>"));
  auto* wave_row = new QHBoxLayout;
  wave_preset_ = new QComboBox;
  wave_preset_->addItems({"Easy", "Medium", "Hard"});
  wave_preset_->setCurrentIndex(1);  // Medium
  wave_row->addWidget(wave_preset_);
  auto* gen_waves_btn = new QPushButton("Generate");
  gen_waves_btn->setFocusPolicy(Qt::NoFocus);
  connect(gen_waves_btn, &QPushButton::clicked, this, [this] {
    auto ret = QMessageBox::question(
      this,
      "Generate Waves",
      "This will overwrite the current wave configuration. Continue?",
      QMessageBox::Yes | QMessageBox::No
    );
    if (ret == QMessageBox::Yes) regenerateWaves();
  });
  wave_row->addWidget(gen_waves_btn);
  panel_lay->addLayout(wave_row);
  waves_edit_ = new QPlainTextEdit;
  waves_edit_->setMinimumHeight(110);
  waves_edit_->setStyleSheet("font-family: monospace;");
  panel_lay->addWidget(waves_edit_);
  regenerateWaves();  // fill the JSON with a default wave set

  panel_lay->addSpacing(8);

  // Action buttons

  auto* play_btn = new QPushButton("Play");
  panel_lay->addWidget(play_btn);
  connect(play_btn, &QPushButton::clicked, this, &LevelEditor::onPlay);

  auto* save_load_lay = new QHBoxLayout;
  auto* save_btn = new QPushButton("Save");
  save_load_lay->addWidget(save_btn);
  connect(save_btn, &QPushButton::clicked, this, &LevelEditor::onSave);
  auto* load_btn = new QPushButton("Load");
  save_load_lay->addWidget(load_btn);
  connect(load_btn, &QPushButton::clicked, this, &LevelEditor::onLoad);
  panel_lay->addLayout(save_load_lay);

  auto* back_btn = new QPushButton("Back to Menu");
  panel_lay->addWidget(back_btn);
  connect(back_btn, &QPushButton::clicked, this, &LevelEditor::backRequested);

  panel_lay->addSpacing(8);

  // Status label (fixed height for reliable word-wrap rendering).
  status_label_ = new QLabel("Ready. Paint terrain or define a path.");
  status_label_->setWordWrap(true);
  status_label_->setFixedHeight(60);
  status_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  panel_lay->addWidget(status_label_);
  panel_lay->addStretch();

  lay->addStretch(1);  // push panel to the right
  auto* scroll = new QScrollArea;
  scroll->setWidget(panel);
  scroll->setWidgetResizable(true);
  scroll->setFixedWidth(245);
  lay->addWidget(scroll, 0);

  setLayout(lay);
}

void LevelEditor::recomputeLayout() {
  constexpr float margin = 16.0f;  // left/top/bottom padding
  constexpr float gap = 20.0f;     // space between the map and the toolbox
  float panel_w = 245.0f;          // toolbox (scroll area) width
  float avail_w = static_cast<float>(width()) - panel_w - gap - margin;
  float avail_h = static_cast<float>(height()) - margin * 2.0f;
  float mw = static_cast<float>(grid_.cols);
  float mh = static_cast<float>(grid_.rows);
  if (mw <= 0 || mh <= 0) return;
  tile_size_ = std::min(avail_w / mw, avail_h / mh);
  if (tile_size_ < 10) tile_size_ = 10;
  // Center the map in the canvas area (left of the gap + toolbox).
  float canvas_w = static_cast<float>(width()) - panel_w - gap;
  offset_x_ = margin + (canvas_w - margin - tile_size_ * mw) * 0.5f;
  offset_y_ = margin + (static_cast<float>(height()) - margin * 2.0f - tile_size_ * mh) * 0.5f;
}

Vec2 LevelEditor::toMap(QPointF pixel) const {
  return Vec2{
    (static_cast<float>(pixel.x()) - offset_x_) / tile_size_,
    (static_cast<float>(pixel.y()) - offset_y_) / tile_size_
  };
}

void LevelEditor::paintEvent(QPaintEvent*) {
  recomputeLayout();
  float cs = tile_size_;
  float ox = offset_x_;
  float oy = offset_y_;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // --- terrain ---
  for (int r = 0; r < grid_.rows; ++r) {
    for (int c = 0; c < grid_.cols; ++c) {
      QRectF rect(ox + c * cs, oy + r * cs, cs, cs);
      auto const& name = grid_.at(c, r);
      p.fillRect(rect, theme::terrainColorForName(name));
      p.setPen(QPen(theme::gridLineColor()));
      p.drawRect(rect);
    }
  }

  // --- paths (draw all; active path brighter) ---
  for (std::size_t pi = 0; pi < grid_.paths.size(); ++pi) {
    auto const& path = grid_.paths[pi];
    bool active = static_cast<int>(pi) == grid_.active_path;
    int alpha_tile = active ? 160 : 60;
    int alpha_line = active ? 200 : 80;
    for (auto const& [pc, pr] : path.tiles) {
      p.fillRect(
        QRectF(ox + pc * cs, oy + pr * cs, cs, cs),
        theme::withAlpha(theme::pathTileColor(), alpha_tile)
      );
    }
    p.setPen(QPen(theme::withAlpha(theme::pathLineColor(), alpha_line), 3));
    for (std::size_t i = 1; i < path.tiles.size(); ++i) {
      p.drawLine(
        QPointF(
          ox + (path.tiles[i - 1].first + 0.5f) * cs,
          oy + (path.tiles[i - 1].second + 0.5f) * cs
        ),
        QPointF(ox + (path.tiles[i].first + 0.5f) * cs, oy + (path.tiles[i].second + 0.5f) * cs)
      );
    }
    if (path.tiles.size() >= 2) {
      p.setBrush(QBrush(theme::withAlpha(theme::spawnMarkerColor(), alpha_line)));
      p.setPen(QPen(Qt::black, 1));
      p.drawEllipse(
        QPointF(
          ox + (path.tiles.front().first + 0.5f) * cs,
          oy + (path.tiles.front().second + 0.5f) * cs
        ),
        cs * 0.2f,
        cs * 0.2f
      );
      p.setBrush(QBrush(theme::withAlpha(theme::exitMarkerColor(), alpha_line)));
      p.drawEllipse(
        QPointF(
          ox + (path.tiles.back().first + 0.5f) * cs,
          oy + (path.tiles.back().second + 0.5f) * cs
        ),
        cs * 0.2f,
        cs * 0.2f
      );
    }
    p.setBrush(QBrush(theme::withAlpha(theme::terrainColorForName("portal"), active ? 180 : 80)));
    for (std::size_t i = 0; i + 1 < path.portal_pairs.size(); i += 2) {
      for (int j = 0; j < 2; ++j) {
        auto const& [pc, pr] = path.portal_pairs[i + j];
        p.drawEllipse(
          QPointF(ox + (pc + 0.5f) * cs, oy + (pr + 0.5f) * cs),
          cs * 0.15f,
          cs * 0.15f
        );
      }
    }
  }

  // --- hover ghost ---
  if (hover_.x >= 0 && hover_.x < grid_.cols && hover_.y >= 0 && hover_.y < grid_.rows) {
    int col = static_cast<int>(hover_.x);
    int row = static_cast<int>(hover_.y);
    p.setPen(QPen(theme::hoverHighlightColor(), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(ox + col * cs, oy + row * cs, cs, cs));
  }
}

void LevelEditor::mousePressEvent(QMouseEvent* event) {
  Vec2 mp = toMap(event->position());
  int col = static_cast<int>(mp.x);
  int row = static_cast<int>(mp.y);
  if (col < 0 || col >= grid_.cols || row < 0 || row >= grid_.rows) return;

  if (tool_ == 0) {
    // Terrain painting.
    grid_.at(col, row) = selected_terrain_.toStdString();
    update();
  } else if (tool_ == 1) {
    // Path mode: add waypoint to the active route.
    auto& path = grid_.currentPath();
    path.tiles.emplace_back(col, row);
    status_label_->setText(
      QString("Route %1: %2 waypoints").arg(grid_.active_path + 1).arg(path.tiles.size())
    );
    update();
  } else if (tool_ == 2) {
    // Portal mode: click two path tiles to pair (must be unique).
    auto& path = grid_.currentPath();
    int idx = -1;
    for (std::size_t i = 0; i < path.tiles.size(); ++i) {
      if (path.tiles[i].first == col && path.tiles[i].second == row) {
        idx = static_cast<int>(i);
        break;
      }
    }
    if (idx < 0) {
      status_label_->setText("Portal: click a path tile.");
      return;
    }
    if (portal_click_stage_ == 0) {
      portal_first_ = idx;
      portal_click_stage_ = 1;
      status_label_->setText("Portal: click second path tile.");
    } else {
      auto const& a = path.tiles[portal_first_];
      auto const& b = path.tiles[idx];
      if (isPortalTile(a.first, a.second) || isPortalTile(b.first, b.second)) {
        status_label_->setText("Portal: tile already used by another portal.");
      } else {
        path.portal_pairs.push_back(a);
        path.portal_pairs.push_back(b);
        status_label_->setText("Portal paired.");
        update();
      }
      portal_click_stage_ = 0;
    }
  }
}

void LevelEditor::mouseMoveEvent(QMouseEvent* event) {
  hover_ = toMap(event->position());
  // Drag-paint in terrain mode.
  if (tool_ == 0 && (event->buttons() & Qt::LeftButton)) {
    int col = static_cast<int>(hover_.x);
    int row = static_cast<int>(hover_.y);
    if (col >= 0 && col < grid_.cols && row >= 0 && row < grid_.rows) {
      grid_.at(col, row) = selected_terrain_.toStdString();
    }
  }
  update();
}

void LevelEditor::leaveEvent(QEvent*) {
  hover_ = {-1, -1};
  update();
}

void LevelEditor::onClearPath() {
  auto& path = grid_.currentPath();
  path.tiles.clear();
  path.portal_pairs.clear();
  portal_click_stage_ = 0;
  status_label_->setText("Route cleared.");
  update();
}

bool LevelEditor::isPortalTile(int col, int row) const {
  for (auto const& path : grid_.paths) {
    for (auto const& [pc, pr] : path.portal_pairs) {
      if (pc == col && pr == row) return true;
    }
  }
  return false;
}

void LevelEditor::refreshPathSelector() {
  path_selector_->blockSignals(true);
  path_selector_->clear();
  for (int i = 0; i < static_cast<int>(grid_.paths.size()); ++i) {
    path_selector_->addItem(QString("Route %1").arg(i + 1));
  }
  path_selector_->setCurrentIndex(grid_.active_path);
  path_selector_->blockSignals(false);
}

void LevelEditor::onAddPath() {
  grid_.paths.push_back(EditorPath{});
  grid_.active_path = static_cast<int>(grid_.paths.size()) - 1;
  refreshPathSelector();
  regenerateWaves();
  status_label_->setText(QString("Added route %1.").arg(grid_.active_path + 1));
  update();
}

void LevelEditor::onRemovePath() {
  if (grid_.paths.size() <= 1) {
    status_label_->setText("Cannot delete the last route.");
    return;
  }
  grid_.paths.erase(grid_.paths.begin() + grid_.active_path);
  if (grid_.active_path >= static_cast<int>(grid_.paths.size())) {
    grid_.active_path = static_cast<int>(grid_.paths.size()) - 1;
  }
  refreshPathSelector();
  regenerateWaves();
  status_label_->setText("Route deleted.");
  update();
}

void LevelEditor::onSelectPath() {
  grid_.active_path = path_selector_->currentIndex();
  status_label_->setText(QString("Editing route %1.").arg(grid_.active_path + 1));
  update();
}

void LevelEditor::onResize() {
  int new_rows = rows_spin_->value();
  int new_cols = cols_spin_->value();
  // Preserve existing terrain where possible.
  std::vector<std::string> new_terrain(static_cast<std::size_t>(new_rows * new_cols), "grass");
  for (int r = 0; r < std::min(new_rows, grid_.rows); ++r) {
    for (int c = 0; c < std::min(new_cols, grid_.cols); ++c) {
      new_terrain[static_cast<std::size_t>(r * new_cols + c)] = grid_.at(c, r);
    }
  }
  grid_.rows = new_rows;
  grid_.cols = new_cols;
  grid_.terrain = std::move(new_terrain);
  // Remove out-of-bounds tiles from all paths.
  for (auto& path : grid_.paths) {
    path.tiles.erase(
      std::remove_if(
        path.tiles.begin(),
        path.tiles.end(),
        [&](auto const& p) { return p.first >= new_cols || p.second >= new_rows; }
      ),
      path.tiles.end()
    );
    path.portal_pairs.erase(
      std::remove_if(
        path.portal_pairs.begin(),
        path.portal_pairs.end(),
        [&](auto const& p) { return p.first >= new_cols || p.second >= new_rows; }
      ),
      path.portal_pairs.end()
    );
  }
  update();
}

void LevelEditor::generateWaves(
  std::vector<Wave>& waves,
  std::vector<float>& gaps,
  int difficulty
) const {
  // Count usable routes (paths with at least two waypoints). Each spawn is
  // assigned a uniformly random route, so multi-route levels spread enemies
  // across all routes instead of piling them on route 0.
  int num_routes = 0;
  for (auto const& p : grid_.paths) {
    if (p.tiles.size() >= 2) ++num_routes;
  }
  if (num_routes < 1) num_routes = 1;

  auto rand_route = [&]() { return std::uniform_int_distribution<int>(0, num_routes - 1)(rng_); };
  // Build a wave from (type, time) pairs, assigning each spawn a random route.
  auto make_wave = [&](std::initializer_list<std::pair<char const*, float>> specs) {
    Wave w;
    for (auto const& [type, time] : specs) {
      w.spawns.push_back(EnemySpawn{type, time, rand_route()});
    }
    return w;
  };

  waves.clear();
  gaps.clear();
  if (difficulty == 0) {
    // Easy: 4 waves
    waves.push_back(
      make_wave({{"normal", 0.0f}, {"normal", 1.0f}, {"normal", 2.0f}, {"normal", 3.0f}})
    );
    waves.push_back(
      make_wave({{"fast", 0.0f}, {"fast", 0.8f}, {"normal", 1.6f}, {"normal", 2.4f}})
    );
    waves.push_back(make_wave({{"armored", 0.0f}, {"armored", 2.5f}}));
    waves.push_back(make_wave({{"normal", 0.0f}, {"normal", 1.0f}, {"boss", 3.0f}}));
    gaps = {4.0f, 4.0f, 5.0f, 5.0f};
  } else if (difficulty == 1) {
    // Medium: 5 waves
    waves.push_back(make_wave(
      {{"normal", 0.0f}, {"normal", 0.8f}, {"normal", 1.6f}, {"fast", 2.4f}, {"fast", 3.0f}}
    ));
    waves.push_back(make_wave({{"armored", 0.0f}, {"normal", 1.0f}, {"armored", 2.5f}}));
    waves.push_back(make_wave({{"splitter", 0.0f}, {"splitter", 2.5f}}));
    waves.push_back(
      make_wave({{"resistant", 0.0f}, {"fast", 0.8f}, {"resistant", 2.0f}, {"normal", 3.0f}})
    );
    waves.push_back(make_wave({{"normal", 0.0f}, {"fast", 1.0f}, {"boss", 3.0f}}));
    gaps = {3.0f, 3.0f, 4.0f, 4.0f, 5.0f};
  } else {
    // Hard: 6 waves
    waves.push_back(make_wave(
      {{"fast", 0.0f},
       {"fast", 0.4f},
       {"fast", 0.8f},
       {"fast", 1.2f},
       {"normal", 2.0f},
       {"normal", 2.6f}}
    ));
    waves.push_back(make_wave({{"armored", 0.0f}, {"resistant", 2.0f}, {"armored", 4.0f}}));
    waves.push_back(make_wave({{"splitter", 0.0f}, {"splitter", 2.0f}, {"fast", 4.0f}}));
    waves.push_back(
      make_wave({{"resistant", 0.0f}, {"armored", 1.0f}, {"resistant", 2.5f}, {"armored", 3.5f}})
    );
    waves.push_back(make_wave({{"boss", 0.0f}, {"splitter", 2.5f}, {"fast", 4.5f}}));
    waves.push_back(make_wave({{"armored", 0.0f}, {"resistant", 2.0f}, {"boss", 4.0f}}));
    gaps = {2.0f, 3.0f, 3.0f, 4.0f, 4.0f, 5.0f};
  }
}

void LevelEditor::regenerateWaves() {
  // Generate a wave set at the selected difficulty, with routes assigned to the
  // current path count. Called on "Generate" button click and when paths change.
  int difficulty = wave_preset_->currentIndex();
  if (difficulty < 0 || difficulty > 2) difficulty = 1;
  std::vector<Wave> waves;
  std::vector<float> gaps;
  generateWaves(waves, gaps, difficulty);
  waves_edit_->setPlainText(wavesToJson(waves, gaps));
}

QString
LevelEditor::wavesToJson(std::vector<Wave> const& waves, std::vector<float> const& gaps) const {
  QString s = "{\n  \"gaps\": [";
  for (std::size_t i = 0; i < gaps.size(); ++i) {
    if (i > 0) s += ", ";
    s += QString::number(gaps[i]);
  }
  s += "],\n  \"waves\": [\n";
  for (std::size_t wi = 0; wi < waves.size(); ++wi) {
    s += "    {\"spawns\": [";
    for (std::size_t si = 0; si < waves[wi].spawns.size(); ++si) {
      if (si > 0) s += ", ";
      auto const& sp = waves[wi].spawns[si];
      s += QString("{\"type\": \"%1\", \"time\": %2, \"route\": %3}")
             .arg(QString::fromStdString(sp.type))
             .arg(sp.time)
             .arg(sp.route);
    }
    s += "]}";
    if (wi + 1 < waves.size()) s += ",";
    s += "\n";
  }
  s += "  ]\n}";
  return s;
}

bool LevelEditor::extractWaves(
  std::vector<Wave>& waves,
  std::vector<float>& gaps,
  QString& err
) const {
  waves.clear();
  gaps.clear();
  std::string text = waves_edit_->toPlainText().toStdString();
  try {
    crjson::parse doc{text};
    auto r = doc.root();
    auto gaps_arr = r["gaps"];
    for (std::size_t i = 0; i < gaps_arr.size(); ++i) {
      gaps.push_back(static_cast<float>(gaps_arr[i].as_num()));
    }
    auto waves_arr = r["waves"];
    for (std::size_t i = 0; i < waves_arr.size(); ++i) {
      Wave w;
      auto spawns = waves_arr[i]["spawns"];
      for (std::size_t j = 0; j < spawns.size(); ++j) {
        int route = static_cast<int>(spawns[j]["route"].as_num());
        if (route < 0 || route >= static_cast<int>(grid_.paths.size())) {
          err = QString("wave %1 spawn %2: route %3 is out of range (0..%4)")
                  .arg(i)
                  .arg(j)
                  .arg(route)
                  .arg(static_cast<int>(grid_.paths.size()) - 1);
          return false;
        }
        w.spawns.push_back(
          EnemySpawn{
            std::string(spawns[j]["type"].as_str()),
            static_cast<float>(spawns[j]["time"].as_num()),
            route,
          }
        );
      }
      waves.push_back(std::move(w));
    }
  } catch (std::exception const& e) {
    err = QString("Wave JSON error: %1").arg(e.what());
    return false;
  }
  if (waves.empty()) {
    err = "waves must not be empty";
    return false;
  }
  if (gaps.size() != waves.size()) {
    err = QString("gaps count (%1) must match waves count (%2)").arg(gaps.size()).arg(waves.size());
    return false;
  }
  return true;
}

Level LevelEditor::buildLevel() const {
  // Build tiles from terrain grid.
  std::vector<Tile> tiles;
  tiles.reserve(static_cast<std::size_t>(grid_.rows * grid_.cols));
  for (int r = 0; r < grid_.rows; ++r) {
    for (int c = 0; c < grid_.cols; ++c) {
      tiles.push_back(config::tile_from_terrain(grid_.at(c, r), Vec2(c + 0.5f, r + 0.5f)));
    }
  }

  // Build routes from all editor paths.
  std::vector<LevelRoute> routes;
  for (auto const& epath : grid_.paths) {
    if (epath.tiles.size() < 2) continue;
    std::vector<Vec2> waypoints;
    for (auto const& [c, r] : epath.tiles) {
      waypoints.push_back(Vec2(c + 0.5f, r + 0.5f));
    }
    Path path(std::move(waypoints));

    // Portal pairs for this route.
    std::vector<std::pair<Vec2, Vec2>> portal_pairs;
    std::vector<std::pair<Vec2, float>> portal_dest;
    for (std::size_t i = 0; i + 1 < epath.portal_pairs.size(); i += 2) {
      int idxA = -1, idxB = -1;
      for (std::size_t j = 0; j < epath.tiles.size(); ++j) {
        if (epath.tiles[j] == epath.portal_pairs[i]) idxA = static_cast<int>(j);
        if (epath.tiles[j] == epath.portal_pairs[i + 1]) idxB = static_cast<int>(j);
      }
      if (idxA < 0 || idxB < 0) continue;
      auto const& [ca, ra] = epath.portal_pairs[i];
      auto const& [cb, rb] = epath.portal_pairs[i + 1];
      tiles[static_cast<std::size_t>(ra) * grid_.cols + ca].set_is_portal(true);
      tiles[static_cast<std::size_t>(rb) * grid_.cols + cb].set_is_portal(true);
      Vec2 posA(ca + 0.5f, ra + 0.5f);
      Vec2 posB(cb + 0.5f, rb + 0.5f);
      portal_pairs.emplace_back(posA, posB);
      float dA = path.cumulative_at(static_cast<std::size_t>(idxA));
      float dB = path.cumulative_at(static_cast<std::size_t>(idxB));
      if (dB > dA) {
        portal_dest.emplace_back(posA, dB);
      } else {
        portal_dest.emplace_back(posB, dA);
      }
    }
    routes.push_back(LevelRoute{std::move(path), std::move(portal_pairs), std::move(portal_dest)});
  }

  Map map(static_cast<float>(grid_.cols), static_cast<float>(grid_.rows), tiles);

  // Economy.
  int gold = gold_spin_->value();
  int auto_amt = auto_amt_spin_->value();
  float auto_int = static_cast<float>(auto_int_spin_->value());

  // Available towers.
  std::vector<std::string> towers;
  for (int i = 0; i < static_cast<int>(tower_checks_.size()); ++i) {
    if (tower_checks_[i]->isChecked()) {
      static const char* names[] =
        {"normal", "slow", "poison", "splash", "laser", "resource", "wall"};
      towers.emplace_back(names[i]);
    }
  }

  // Waves: parsed from the JSON editor (validated by callers before build).
  std::vector<Wave> waves;
  std::vector<float> gaps;
  QString wave_err;
  if (!extractWaves(waves, gaps, wave_err)) {
    status_label_->setText(wave_err);
  }

  std::string name = name_edit_->text().trimmed().toStdString();
  if (name.empty()) name = "Custom Level";

  auto const& src = controller_->game().level();

  return Level{
    std::move(name),
    -1,  // custom level
    std::move(map),
    std::move(routes),
    std::move(waves),
    std::move(gaps),
    gold,
    Resource::AutoIncrease(auto_amt, auto_int),
    std::move(towers),
    src.tower_stats,
    src.enemy_stats,
  };
}

void LevelEditor::onPlay() {
  if (grid_.currentPath().tiles.size() < 2) {
    status_label_->setText("Need at least 2 path tiles (entrance + exit).");
    return;
  }
  std::vector<Wave> waves;
  std::vector<float> gaps;
  QString err;
  if (!extractWaves(waves, gaps, err)) {
    status_label_->setText(err);
    return;
  }
  emit playRequested();
  status_label_->setText("Playing custom level...");
}

namespace {
/// Lowercase the name, replace non-alphanumeric runs with '-', trim; fallback
/// "custom". Used to derive a stable filename for a saved custom level.
std::string sanitizeFilename(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
    else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
      c = '-';
  }
  s.erase(
    std::unique(s.begin(), s.end(), [](char a, char b) { return a == '-' && b == '-'; }),
    s.end()
  );
  if (!s.empty() && s.front() == '-') s.erase(s.begin());
  if (!s.empty() && s.back() == '-') s.pop_back();
  if (s.empty()) s = "custom";
  return s;
}
}  // namespace

void LevelEditor::onSave() {
  if (grid_.currentPath().tiles.size() < 2) {
    status_label_->setText("Need at least 2 path tiles to save.");
    return;
  }
  std::vector<Wave> waves;
  std::vector<float> gaps;
  QString err;
  if (!extractWaves(waves, gaps, err)) {
    status_label_->setText(err);
    return;
  }
  Level level = buildLevel();
  std::string dir = QCoreApplication::applicationDirPath().toStdString() + "/config/levels";
  namespace fs = std::filesystem;
  fs::create_directories(dir);
  std::string filename = sanitizeFilename(level.name) + ".json";
  std::string path = dir + "/" + filename;
  // If a *different* level's file already occupies this name, disambiguate
  // instead of silently overwriting it.
  if (path != current_file_ && fs::exists(path)) {
    for (int i = 2; fs::exists(path); ++i) {
      filename = sanitizeFilename(level.name) + "-" + std::to_string(i) + ".json";
      path = dir + "/" + filename;
    }
  }
  try {
    config::save_level(path, level);
    current_file_ = path;
    controller_->reloadLevels();
    status_label_->setText(
      "Saved custom level to config/levels/" + QString::fromStdString(filename)
    );
  } catch (std::exception const& e) {
    status_label_->setText(QString("Save error: %1").arg(e.what()));
  }
}

void LevelEditor::onLoad() {
  // Only custom levels (index < 1) are editable — offer them in a picker.
  auto infos = controller_->level_infos();
  QStringList custom_names;
  std::vector<int> slot_indices;
  for (int i = 0; i < static_cast<int>(infos.size()); ++i) {
    if (infos[static_cast<std::size_t>(i)].index < 1) {
      custom_names << QString::fromStdString(infos[static_cast<std::size_t>(i)].name);
      slot_indices.push_back(i);
    }
  }
  if (custom_names.isEmpty()) {
    QMessageBox::information(this, "Load Custom Level", "No custom levels found. Save one first.");
    return;
  }
  bool ok = false;
  QString choice = QInputDialog::getItem(
    this,
    "Load Custom Level",
    "Choose a custom level:",
    custom_names,
    0,
    false,
    &ok
  );
  if (!ok) return;
  int slot = slot_indices[static_cast<std::size_t>(custom_names.indexOf(choice))];
  try {
    Level lvl = controller_->level_at(slot);  // copy
    loadFromLevel(lvl);
    current_file_ = QCoreApplication::applicationDirPath().toStdString() + "/config/levels/" +
                    sanitizeFilename(lvl.name) + ".json";
    status_label_->setText("Loaded custom level: " + QString::fromStdString(lvl.name));
  } catch (std::exception const& e) {
    status_label_->setText(QString("Load error: %1").arg(e.what()));
  }
}

void LevelEditor::loadFromLevel(Level const& level) {
  if (level.index >= 1) {
    QMessageBox::warning(this, "Read-only", "Official levels cannot be edited.");
    return;
  }
  grid_.rows = static_cast<int>(level.map.height());
  grid_.cols = static_cast<int>(level.map.width());
  grid_.terrain.resize(static_cast<std::size_t>(grid_.rows * grid_.cols), "grass");
  for (int r = 0; r < grid_.rows; ++r) {
    for (int c = 0; c < grid_.cols; ++c) {
      Tile const* tile = level.map.tile_at(Vec2(c + 0.5f, r + 0.5f));
      if (tile) {
        grid_.at(c, r) = config::terrain_name(*tile);
      }
    }
  }
  // Load all routes (waypoints + portal pairs).
  grid_.paths.clear();
  for (auto const& route : level.routes) {
    EditorPath epath;
    for (auto const& wp : route.path.waypoints()) {
      epath.tiles.emplace_back(static_cast<int>(wp.x), static_cast<int>(wp.y));
    }
    for (auto const& [pa, pb] : route.portal_pairs) {
      epath.portal_pairs.emplace_back(static_cast<int>(pa.x), static_cast<int>(pa.y));
      epath.portal_pairs.emplace_back(static_cast<int>(pb.x), static_cast<int>(pb.y));
    }
    grid_.paths.push_back(std::move(epath));
  }
  grid_.active_path = 0;
  refreshPathSelector();

  // Sync the size spinboxes without triggering onResize: the grid was loaded
  // directly above, so letting onResize fire mid-sync would re-resize using the
  // other spinbox's stale value and truncate the map (losing columns + path).
  rows_spin_->blockSignals(true);
  cols_spin_->blockSignals(true);
  rows_spin_->setValue(grid_.rows);
  cols_spin_->setValue(grid_.cols);
  rows_spin_->blockSignals(false);
  cols_spin_->blockSignals(false);
  gold_spin_->setValue(level.starting_resources);
  auto_amt_spin_->setValue(level.auto_increase.amount);
  auto_int_spin_->setValue(static_cast<int>(level.auto_increase.duration()));

  // Available towers.
  static const char* tower_names[] =
    {"normal", "slow", "poison", "splash", "laser", "resource", "wall"};
  for (auto* cb : tower_checks_) cb->setChecked(false);
  for (auto const& t : level.available_towers) {
    for (int i = 0; i < 7; ++i) {
      if (t == tower_names[i]) {
        tower_checks_[static_cast<std::size_t>(i)]->setChecked(true);
        break;
      }
    }
  }

  // Name + waves (the loaded JSON is preserved as-is in the text editor).
  name_edit_->setText(QString::fromStdString(level.name));
  waves_edit_->setPlainText(wavesToJson(level.waves, level.gaps));

  update();
}
