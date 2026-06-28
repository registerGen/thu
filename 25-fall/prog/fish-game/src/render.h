// Render system, using Win32 GDI for rendering.

#pragma once

#include <windows.h>

#include "config.h"
#include "ecs.h"
#include "resource.h"
#include "ui.h"

class RenderSystem : public System {
private:
  std::shared_ptr<World> world;
  HWND hwnd;
  int win_width;
  int win_height;
  std::shared_ptr<BitmapManager> bm;

  HDC hdc_mem;
  HDC hdc_load;
  HDC hdc_temp;
  HBITMAP hblank;
  HBITMAP hblank_old;

  void render_image(
    std::shared_ptr<Image> img,
    std::shared_ptr<Coordinate> pos,
    std::shared_ptr<ImageTransform> transform
  );
  void render_text(
    std::shared_ptr<Image> img,
    std::shared_ptr<Text> text,
    std::shared_ptr<Coordinate> pos,
    std::shared_ptr<ImageTransform> transform
  );
  void render_progress_bar(
    std::shared_ptr<Image> img,
    std::shared_ptr<ProgressBar> pbar,
    std::shared_ptr<Coordinate> pos
  );

public:
  RenderSystem(std::shared_ptr<World> world)
      : world(world),
        hwnd(*world->get_ctx<HWND>()),
        win_width(world->get_ctx<WindowConfig>()->width),
        win_height(world->get_ctx<WindowConfig>()->height),
        bm(world->get_ctx<BitmapManager>()) {
    HDC hdc = GetDC(hwnd);
    if (!hdc) {
      hdc_mem = nullptr;
      hdc_load = nullptr;
      hdc_temp = nullptr;
      hblank = nullptr;
      hblank_old = nullptr;
      return;
    }
    hdc_mem = CreateCompatibleDC(hdc);
    hdc_load = CreateCompatibleDC(hdc);
    hdc_temp = CreateCompatibleDC(hdc_mem);
    hblank = CreateCompatibleBitmap(hdc, win_width, win_height);
    hblank_old = (HBITMAP)SelectObject(hdc_mem, hblank);
    ReleaseDC(hwnd, hdc);
  }

  ~RenderSystem() {
    if (hdc_mem) {
      if (hblank_old) {
        SelectObject(hdc_mem, hblank_old);
        hblank_old = nullptr;
      }
      DeleteDC(hdc_mem);
      hdc_mem = nullptr;
    }
    if (hblank) {
      DeleteObject(hblank);
      hblank = nullptr;
    }
    if (hdc_load) {
      DeleteDC(hdc_load);
      hdc_load = nullptr;
    }
    if (hdc_temp) {
      DeleteDC(hdc_temp);
      hdc_temp = nullptr;
    }
  }

  void render();

  void update(float dt) override;
};
