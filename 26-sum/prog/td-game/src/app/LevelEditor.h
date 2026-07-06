#pragma once

#include <QPointF>
#include <QString>
#include <QWidget>
#include <random>
#include <string>
#include <vector>

#include "game/geometry.h"
#include "game/level.h"

class GameController;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

/// Level editor: paint terrain, define paths (entrance → exit), pair portals,
/// set economy/towers/waves, then save/load/play. Supports multiple routes.
class LevelEditor : public QWidget {
  Q_OBJECT

public:
  explicit LevelEditor(GameController* controller, QWidget* parent = nullptr);

signals:
  void playRequested();
  void backRequested();

public:
  /// Build a Level from the current editor state.
  Level buildLevel() const;

private slots:
  void onPlay();
  void onSave();
  void onLoad();
  void onClearPath();
  void onResize();
  void onAddPath();
  void onRemovePath();
  void onSelectPath();

private:
  struct EditorPath {
    std::vector<std::pair<int, int>> tiles;
    std::vector<std::pair<int, int>> portal_pairs;
  };

  struct EditorGrid {
    int rows = 7;
    int cols = 12;
    std::vector<std::string> terrain;
    std::vector<EditorPath> paths;
    int active_path = 0;

    std::string& at(int col, int row) {
      return terrain[static_cast<std::size_t>(row) * cols + col];
    }
    std::string const& at(int col, int row) const {
      return terrain[static_cast<std::size_t>(row) * cols + col];
    }
    EditorPath& currentPath() { return paths[static_cast<std::size_t>(active_path)]; }
    EditorPath const& currentPath() const { return paths[static_cast<std::size_t>(active_path)]; }
  };

  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;

  void recomputeLayout();
  Vec2 toMap(QPointF pixel) const;
  void generateWaves(std::vector<Wave>& waves, std::vector<float>& gaps, int difficulty) const;
  /// Regenerate waves for the current preset (called when the route count
  /// changes, since spawns reference routes by index).
  void regenerateWaves();
  /// Serialize waves + gaps into the JSON shown in the waves editor.
  QString wavesToJson(std::vector<Wave> const& waves, std::vector<float> const& gaps) const;
  /// Parse waves + gaps from the waves editor. Returns false and sets `err` on failure.
  bool extractWaves(std::vector<Wave>& waves, std::vector<float>& gaps, QString& err) const;
  void loadFromLevel(Level const& level);
  void refreshPathSelector();
  bool isPortalTile(int col, int row) const;

  GameController* controller_;
  EditorGrid grid_;
  std::string current_file_;  // config/levels path being edited (empty = new/unsaved)
  QString selected_terrain_{"grass"};
  int tool_ = 0;  // 0=terrain, 1=path, 2=portal
  int portal_first_ = -1;
  int portal_click_stage_ = 0;

  float tile_size_ = 48.0f;
  float offset_x_ = 0.0f;
  float offset_y_ = 0.0f;
  Vec2 hover_{-1, -1};

  /// RNG for assigning spawns to routes; mutable so generateWaves (const) can use it.
  mutable std::mt19937 rng_{std::random_device{}()};

  // Controls
  QLineEdit* name_edit_;
  QSpinBox* rows_spin_;
  QSpinBox* cols_spin_;
  QSpinBox* gold_spin_;
  QSpinBox* auto_amt_spin_;
  QSpinBox* auto_int_spin_;
  QComboBox* wave_preset_;
  QPlainTextEdit* waves_edit_;
  QComboBox* path_selector_;
  std::vector<QCheckBox*> tower_checks_;
  QLabel* status_label_;
};
