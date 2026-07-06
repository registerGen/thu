#pragma once

#include <QMainWindow>
#include <QVector>

#include "Screens.h"

class GameController;
class LevelEditor;
class QStackedWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);

private:
  void startGame(int level_index);
  void toMenu();
  void playCustomLevel();

  GameController* controller_;
  QStackedWidget* stack_;
  StartMenu* start_menu_;
  GameScreen* game_screen_;
  LevelEditor* editor_;
  VictoryScreen* victory_;
  DefeatScreen* defeat_;

  QVector<LevelProgress> progress_;
};
