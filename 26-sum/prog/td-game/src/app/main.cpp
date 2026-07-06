#include <QApplication>
#include <QMessageBox>
#include <exception>
#include <memory>

#include "MainWindow.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  // The level registry (tower/enemy/level config) is loaded while constructing
  // MainWindow. A malformed or missing config dir makes that throw; catch it
  // here and report it instead of crashing with an uncaught exception.
  std::unique_ptr<MainWindow> window;
  try {
    window = std::make_unique<MainWindow>();
  } catch (std::exception const& e) {
    QMessageBox::critical(
      nullptr,
      "Startup failed",
      QString("Failed to load game configuration:\n%1").arg(e.what())
    );
    return 1;
  } catch (...) {
    QMessageBox::critical(nullptr, "Startup failed", "Failed to load game configuration.");
    return 1;
  }

  window->show();
  return app.exec();
}
