#pragma once

#include <QStringList>
#include <QVector>
#include <QWidget>
#include <vector>

#include "game/level.h"  // LevelRegistry::Info

class GameController;
class GameWidget;
class Hud;
class QButtonGroup;
class QGridLayout;
class QLabel;
class QPushButton;

/// In-memory per-level progress (not persisted across sessions).
struct LevelProgress {
  bool cleared = false;
  int max_score = 0;
};

/// Start menu: per-level rows (start button + cleared? + max score), a help
/// section (towers/enemies/tiles/controls), and quit.
class StartMenu : public QWidget {
  Q_OBJECT

public:
  explicit StartMenu(std::vector<LevelRegistry::Info> infos, QWidget* parent = nullptr);

  /// Update the cleared/max-score columns for each level row.
  void setProgress(QVector<LevelProgress> const& progress);

signals:
  void startRequested(int level_index);
  void editorRequested();
  void quitRequested();

private:
  void buildLevelRows(std::vector<LevelRegistry::Info> const& infos);
  QGridLayout* level_grid_;
  QVector<QPushButton*> level_buttons_;
  QVector<QLabel*> cleared_labels_;
  QVector<QLabel*> score_labels_;
};

/// Buttons for the current level's available towers (name + cost) in the game screen.
/// Checkable; selecting one emits typeSelected. Rebuild when the level changes.
class TowerPalette : public QWidget {
  Q_OBJECT

public:
  explicit TowerPalette(GameController* controller, QWidget* parent = nullptr);

  void rebuild();

public slots:
  /// Flash "Not enough resource!" under the buttons for 1 second.
  void showInsufficientResource();

signals:
  void typeSelected(QString type);

private:
  GameController* controller_;
  QButtonGroup* group_;
  QLabel* status_label_;
  QTimer* status_timer_;
};

/// Top bar in the game screen: resources, wave, level, score, time + pause/restart buttons.
class Hud : public QWidget {
  Q_OBJECT

public:
  explicit Hud(GameController* controller, QWidget* parent = nullptr);

public slots:
  void refresh();
  /// Show/hide the pause, restart, and quit buttons.
  void setControlsVisible(bool visible);

signals:
  /// "Quit" abandons the current level and returns to the start menu.
  void quitRequested();

private:
  GameController* controller_;
  QLabel* resources_;
  QLabel* wave_;
  QLabel* level_;
  QLabel* score_;
  QLabel* time_;
  QLabel* fps_;
  QPushButton* pause_btn_;
  QPushButton* restart_btn_;
  QPushButton* quit_btn_;
};

/// The in-game screen: HUD on top, the game view + tower palette/legend sidebar.
/// Owns the view, HUD, and palette, and wires their internal signals to the
/// controller. Emits quitRequested when the player abandons the level.
class GameScreen : public QWidget {
  Q_OBJECT

public:
  explicit GameScreen(GameController* controller, QWidget* parent = nullptr);

  /// Start/stop the simulation timer (delegated to the view).
  void startLoop();
  void stopLoop();

signals:
  void quitRequested();

private:
  GameController* controller_;
  GameWidget* widget_;
  Hud* hud_;
  TowerPalette* palette_;
};

/// Victory overlay: score/time + Next Level (if available) / Main Menu.
class VictoryScreen : public QWidget {
  Q_OBJECT

public:
  explicit VictoryScreen(QWidget* parent = nullptr);
  void showResult(int score, float time, bool has_next, bool from_editor);

signals:
  void nextRequested();
  void menuRequested();

private:
  QLabel* summary_;
  QPushButton* next_btn_;
  QPushButton* menu_btn_;
};

/// Defeat overlay: score/time + Retry / Main Menu.
class DefeatScreen : public QWidget {
  Q_OBJECT

public:
  explicit DefeatScreen(QWidget* parent = nullptr);
  void showResult(int score, float time, bool from_editor);

signals:
  void retryRequested();
  void menuRequested();

private:
  QLabel* summary_;
  QPushButton* menu_btn_;
};
