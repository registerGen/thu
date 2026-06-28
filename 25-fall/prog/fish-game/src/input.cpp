#include "input.h"

#include <cctype>

void InputSystem::handle_msg(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_MOUSEMOVE: {
    int x = LOWORD(lparam);
    int y = HIWORD(lparam);
    world->emit(MouseMoved(x, y));
    break;
  }

  case WM_LBUTTONDOWN: {
    int x = LOWORD(lparam);
    int y = HIWORD(lparam);
    world->emit(LeftMouseDown(x, y));
    break;
  }

  case WM_LBUTTONUP: {
    int x = LOWORD(lparam);
    int y = HIWORD(lparam);
    world->emit(LeftMouseUp(x, y));
    break;
  }

  case WM_KEYDOWN: {
    int keycode = static_cast<int>(wparam);

    // Distinguish left shift and right shift.
    if (wparam == keycodes::SHIFT) {
      UINT scancode = (lparam >> 16) & 0xFF;
      world->emit(KeyDown(static_cast<int>(MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX))));
    }

    // Reset the current input string if there is no typing after 2 seconds.
    if (std::isalnum(keycode)) {
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(now - last) > max_gap) cur = "";
      cur += static_cast<char>(keycode);
      last = now;
    } else
      cur = "";

    world->emit(KeyDown(keycode));
    break;
  }

  case WM_KEYUP: {
    int keycode = static_cast<int>(wparam);

    if (wparam == keycodes::SHIFT) {
      UINT scancode = (lparam >> 16) & 0xFF;
      world->emit(KeyUp(static_cast<int>(MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX))));
    }

    world->emit(KeyUp(keycode));
    break;
  }

  default:
    break;
  }
}

std::string InputSystem::current_input() {
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::seconds>(now - last) > max_gap) cur = "";
  return cur;
}

std::pair<int, int> InputSystem::cursor_pos(HWND hwnd) {
  POINT pt;
  GetCursorPos(&pt);
  ScreenToClient(hwnd, &pt);
  return {pt.x, pt.y};
}

void InputSystem::clip_cursor(HWND hwnd) {
  if (!hwnd) {
    ClipCursor(nullptr);
    return;
  }

  RECT rect;
  GetClientRect(hwnd, &rect);
  POINT lt = {rect.left, rect.top};
  POINT rb = {rect.right, rect.bottom};
  ClientToScreen(hwnd, &lt);
  ClientToScreen(hwnd, &rb);

  RECT clip_rect = {lt.x, lt.y, rb.x, rb.y};
  ClipCursor(&clip_rect);
}
