#include "render.h"

#include <algorithm>

#pragma comment(lib, "msimg32.lib")

// NOTE: pos is the center of the image.
void RenderSystem::render_image(
  std::shared_ptr<Image> img,
  std::shared_ptr<Coordinate> pos,
  std::shared_ptr<ImageTransform> transform
) {
  if (!img || !pos) return;
  auto [hbmp, width, height] = bm->get(img->id);
  if (!hbmp || width <= 0 || height <= 0) return;

  HBITMAP hbmp_old = (HBITMAP)SelectObject(hdc_load, hbmp);

  float scale = (transform ? transform->scale : 1.0f);
  bool mirror = (transform ? transform->mirror : false);

  int new_width = static_cast<int>(static_cast<float>(width) * scale);
  int new_height = static_cast<int>(static_cast<float>(height) * scale);
  int draw_x = pos->x - new_width / 2;
  int draw_y = pos->y - new_height / 2;

  if (scale == 1.0f && mirror == false) {
    // No scaling or mirroring.
    TransparentBlt(
      hdc_mem,
      draw_x,
      draw_y,
      width,
      height,
      hdc_load,
      0,
      0,
      width,
      height,
      RGB(0, 0, 0)
    );
  } else {
    if (mirror) {
      // TransparentBlt does not support mirroring,
      // so we use StretchBlt to mirror and TransparentBlt to scale and remove the background.
      HBITMAP hbmp_temp = CreateCompatibleBitmap(hdc_mem, width, height);
      HBITMAP hbmp_old2 = (HBITMAP)SelectObject(hdc_temp, hbmp_temp);

      StretchBlt(hdc_temp, 0, 0, width, height, hdc_load, width - 1, 0, -width, height, SRCCOPY);

      TransparentBlt(
        hdc_mem,
        draw_x,
        draw_y,
        new_width,
        new_height,
        hdc_temp,
        0,
        0,
        width,
        height,
        RGB(0, 0, 0)
      );

      SelectObject(hdc_temp, hbmp_old2);
      DeleteObject(hbmp_temp);

    } else {
      // Scaling only.
      TransparentBlt(
        hdc_mem,
        draw_x,
        draw_y,
        new_width,
        new_height,
        hdc_load,
        0,
        0,
        width,
        height,
        RGB(0, 0, 0)
      );
    }
  }

  SelectObject(hdc_load, hbmp_old);
}

// Render text by rendering each character image in sequence.
void RenderSystem::render_text(
  std::shared_ptr<Image> img,
  std::shared_ptr<Text> text,
  std::shared_ptr<Coordinate> pos,
  std::shared_ptr<ImageTransform> transform
) {
  int x = pos->x, y = pos->y;
  int width = text->fontw;
  if (transform && transform->scale != 1.0f)
    width = static_cast<int>(static_cast<float>(width) * transform->scale);

  // Calculate starting x based on alignment.
  switch (text->align) {
  case Alignment::Left:
    x += width / 2;
    break;

  case Alignment::Center:
    x -= (static_cast<int>(text->text.length()) - 1) * width / 2;
    break;

  case Alignment::Right:
    x -= static_cast<int>(text->text.length()) * width;
    x += width / 2;
    break;
  }

  for (size_t i = 0; i != text->text.length(); ++i) {
    render_image(
      std::make_shared<Image>(text->font[text->text[i]], img->zidx),
      std::make_shared<Coordinate>(x, y),
      transform
    );
    x += width;
  }
}

// Render a progress bar at the given position.
void RenderSystem::render_progress_bar(
  std::shared_ptr<Image> img,
  std::shared_ptr<ProgressBar> pbar,
  std::shared_ptr<Coordinate> pos
) {
  if (!img || !pos || !pbar) return;

  int draw_x = pos->x - pbar->width / 2;
  int draw_y = pos->y - pbar->height / 2;

  int filled_width = std::clamp(
    static_cast<int>(std::round(static_cast<float>(pbar->width) * pbar->percent)),
    0,
    pbar->width
  );

  // Draw border.
  HBRUSH draw_brush = CreateSolidBrush(pbar->draw);
  if (draw_brush) {
    SelectObject(hdc_mem, draw_brush);
    RECT border_rect = {draw_x, draw_y, draw_x + pbar->width, draw_y + pbar->height};
    FrameRect(hdc_mem, &border_rect, draw_brush);
    DeleteObject(draw_brush);
  }

  // Fill.
  if (filled_width > 0) {
    HBRUSH fill_brush = CreateSolidBrush(pbar->fill);
    if (fill_brush) {
      SelectObject(hdc_mem, fill_brush);
      RECT fill_rect;
      if (pbar->align == Alignment::Left)
        fill_rect = {draw_x + 1, draw_y + 1, draw_x + filled_width - 1, draw_y + pbar->height - 1};
      else if (pbar->align == Alignment::Center) {
        int center_x = draw_x + pbar->width / 2;
        fill_rect = {
          center_x - filled_width / 2 + 1,
          draw_y + 1,
          center_x + filled_width / 2 - 1,
          draw_y + pbar->height - 1
        };
      } else {  // Alignment::Right
        fill_rect = {
          draw_x + pbar->width - filled_width + 1,
          draw_y + 1,
          draw_x + pbar->width - 1,
          draw_y + pbar->height - 1
        };
      }
      FillRect(hdc_mem, &fill_rect, fill_brush);
      DeleteObject(fill_brush);
    }
  }
}

void RenderSystem::render() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  if (!hdc) return;

  // Clear background
  RECT rect = {0, 0, win_width, win_height};
  HBRUSH bg_brush = CreateSolidBrush(RGB(0, 0, 0));
  if (bg_brush) {
    FillRect(hdc_mem, &rect, bg_brush);
    DeleteObject(bg_brush);
  }

  using CoordImg =
    std::tuple<Entity, std::shared_ptr<Image>, std::shared_ptr<Coordinate>, std::shared_ptr<Shown>>;

  std::vector<CoordImg> ents = world->get<Image, Coordinate, Shown>();
  std::ranges::sort(ents, [](const CoordImg& lhs, const CoordImg& rhs) {
    return std::get<1>(lhs)->zidx < std::get<1>(rhs)->zidx;
  });

  // Render all images, texts, and progress bars in order of z-index.
  for (auto& [ent, img, pos, _] : ents) {
    if (img->id == ImageType::IMG_TEXT)
      render_text(img, world->get<Text>(ent), pos, world->get<ImageTransform>(ent));
    else if (img->id == ImageType::IMG_PROGBAR)
      render_progress_bar(img, world->get<ProgressBar>(ent), pos);
    else  // Regular image.
      render_image(img, pos, world->get<ImageTransform>(ent));
  }

  // Blit to window.
  BitBlt(hdc, 0, 0, win_width, win_height, hdc_mem, 0, 0, SRCCOPY);
  EndPaint(hwnd, &ps);
}

void RenderSystem::update(float) { InvalidateRect(hwnd, nullptr, FALSE); }
