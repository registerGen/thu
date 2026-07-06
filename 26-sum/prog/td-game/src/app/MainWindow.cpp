#include "MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QStackedWidget>
#include <QTimer>

#include "GameController.h"
#include "LevelEditor.h"
#include "Screens.h"
#include "Theme.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  QString config_dir = QCoreApplication::applicationDirPath() + "/config";
  controller_ = new GameController(config_dir, this);
  progress_.resize(static_cast<int>(controller_->level_count()));

  stack_ = new QStackedWidget;
  setCentralWidget(stack_);

  // --- start menu ---
  start_menu_ = new StartMenu(controller_->official_level_infos());
  stack_->addWidget(start_menu_);

  // --- game screen ---
  game_screen_ = new GameScreen(controller_);
  stack_->addWidget(game_screen_);

  // --- victory / defeat ---
  victory_ = new VictoryScreen;
  defeat_ = new DefeatScreen;
  stack_->addWidget(victory_);
  stack_->addWidget(defeat_);

  // --- level editor ---
  editor_ = new LevelEditor(controller_);
  stack_->addWidget(editor_);

  setWindowTitle("Tower Defense");
  setWindowIcon(QIcon(theme::makeTowerPixmap("normal", 64)));
  resize(900, 500);

  // --- wiring ---
  connect(start_menu_, &StartMenu::startRequested, this, &MainWindow::startGame);
  connect(start_menu_, &StartMenu::editorRequested, this, [this] {
    stack_->setCurrentWidget(editor_);
  });
  connect(start_menu_, &StartMenu::quitRequested, this, &QMainWindow::close);

  connect(editor_, &LevelEditor::playRequested, this, &MainWindow::playCustomLevel);
  // The editor's "Back" always goes to the start menu (not toMenu, which would
  // loop back to the editor if the game still holds a custom level).
  connect(editor_, &LevelEditor::backRequested, this, [this] {
    start_menu_->setProgress(progress_);
    stack_->setCurrentWidget(start_menu_);
  });

  connect(game_screen_, &GameScreen::quitRequested, this, &MainWindow::toMenu);

  connect(controller_, &GameController::stateChanged, this, [this](Game::State s) {
    auto const& r = controller_->game().last_result();
    int slot = controller_->current_level_index();
    bool official = controller_->game().level().index >= 1;
    // Record progress for official levels only (custom levels don't track it).
    if (official && !r.cheated && slot >= 0 && slot < progress_.size()) {
      if (s == Game::State::Victory) progress_[slot].cleared = true;
      progress_[slot].max_score = std::max(progress_[slot].max_score, r.score);
    }
    // Delay the screen switch so the game-over banner is visible on the board.
    QTimer::singleShot(2000, this, [this, s, r, official] {
      game_screen_->stopLoop();      // stop the effect animation timer
      bool from_editor = !official;  // custom level -> "Back to Editor"
      if (s == Game::State::Victory) {
        victory_->showResult(r.score, r.time, controller_->has_next_level(), from_editor);
        stack_->setCurrentWidget(victory_);
      } else {
        defeat_->showResult(r.score, r.time, from_editor);
        stack_->setCurrentWidget(defeat_);
      }
    });
  });

  connect(victory_, &VictoryScreen::nextRequested, this, [this] {
    controller_->nextLevel();
    game_screen_->startLoop();
    stack_->setCurrentWidget(game_screen_);
  });
  connect(victory_, &VictoryScreen::menuRequested, this, &MainWindow::toMenu);
  connect(defeat_, &DefeatScreen::retryRequested, this, [this] {
    controller_->restartLevel();
    game_screen_->startLoop();
    stack_->setCurrentWidget(game_screen_);
  });
  connect(defeat_, &DefeatScreen::menuRequested, this, &MainWindow::toMenu);

  stack_->setCurrentWidget(start_menu_);
}

void MainWindow::startGame(int level_index) {
  controller_->selectLevel(level_index);
  game_screen_->startLoop();
  stack_->setCurrentWidget(game_screen_);
}

void MainWindow::toMenu() {
  game_screen_->stopLoop();
  // If the current level is custom (played from the editor), return there
  // instead of the start menu.
  if (controller_->game().level().index < 1) {
    stack_->setCurrentWidget(editor_);
  } else {
    start_menu_->setProgress(progress_);
    stack_->setCurrentWidget(start_menu_);
  }
}

void MainWindow::playCustomLevel() {
  Level level = editor_->buildLevel();
  controller_->playCustomLevel(level);
  game_screen_->startLoop();
  stack_->setCurrentWidget(game_screen_);
}
