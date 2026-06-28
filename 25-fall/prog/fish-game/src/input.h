// Input handling system for keyboard and mouse events.

#pragma once

#include <windows.h>

#include <chrono>

#include "ecs.h"

namespace keycodes {
constexpr int SPACE = VK_SPACE;
constexpr int ENTER = VK_RETURN;
constexpr int ESC = VK_ESCAPE;
constexpr int SHIFT = VK_SHIFT;
constexpr int LSHIFT = VK_LSHIFT;
constexpr int RSHIFT = VK_RSHIFT;
constexpr int CTRL = VK_CONTROL;
constexpr int LEFT = VK_LEFT;
constexpr int RIGHT = VK_RIGHT;
constexpr int UP = VK_UP;
constexpr int DOWN = VK_DOWN;
constexpr int A = 'A';
constexpr int B = 'B';
constexpr int C = 'C';
constexpr int D = 'D';
constexpr int E = 'E';
constexpr int F = 'F';
constexpr int G = 'G';
constexpr int H = 'H';
constexpr int I = 'I';
constexpr int J = 'J';
constexpr int K = 'K';
constexpr int L = 'L';
constexpr int M = 'M';
constexpr int N = 'N';
constexpr int O = 'O';
constexpr int P = 'P';
constexpr int Q = 'Q';
constexpr int R = 'R';
constexpr int S = 'S';
constexpr int T = 'T';
constexpr int U = 'U';
constexpr int V = 'V';
constexpr int W = 'W';
constexpr int X = 'X';
constexpr int Y = 'Y';
constexpr int Z = 'Z';
constexpr int NUM_0 = '0';
constexpr int NUM_1 = '1';
constexpr int NUM_2 = '2';
constexpr int NUM_3 = '3';
constexpr int NUM_4 = '4';
constexpr int NUM_5 = '5';
constexpr int NUM_6 = '6';
constexpr int NUM_7 = '7';
constexpr int NUM_8 = '8';
constexpr int NUM_9 = '9';
}  // namespace keycodes

class MouseMoved : public Event {
public:
  int x;
  int y;
  MouseMoved(int x, int y) : x(x), y(y) { }
};

class LeftMouseDown : public Event {
public:
  int x;
  int y;
  LeftMouseDown(int x, int y) : x(x), y(y) { }
};

class LeftMouseUp : public Event {
public:
  int x;
  int y;
  LeftMouseUp(int x, int y) : x(x), y(y) { }
};

class KeyDown : public Event {
public:
  int keycode;
  KeyDown(int keycode) : keycode(keycode) { }
};

class KeyUp : public Event {
public:
  int keycode;
  KeyUp(int keycode) : keycode(keycode) { }
};

class InputSystem : public System {
private:
  static constexpr auto max_gap = std::chrono::seconds(2);

  std::shared_ptr<World> world;
  std::string cur;
  std::chrono::time_point<std::chrono::steady_clock> last;

public:
  InputSystem(std::shared_ptr<World> world) : world(world) { }

  // Handle input message.
  void handle_msg(UINT msg, WPARAM wParam, LPARAM lParam);

  // Get current input string.
  std::string current_input();

  // Get current cursor position.
  static std::pair<int, int> cursor_pos(HWND hwnd);

  // Lock the cursor inside the game window. Unlock if hwnd is nullptr.
  static void clip_cursor(HWND hwnd);

  void update(float) override { /* No operations. */ };
};
