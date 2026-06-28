// Various UI components and systems.

#pragma once

#include "ecs.h"
#include "input.h"
#include "resource.h"

namespace zindex {
constexpr int BG = 0;
constexpr int GAME = 10;
constexpr int UI = 500;
constexpr int OVERLAY = 1000;
}  // namespace zindex

enum class Alignment { Left, Center, Right };

class Coordinate : public Component {
public:
  int x;
  int y;
  Coordinate(int x = 0, int y = 0) : x(x), y(y) { }
};

class ImageTransform : public Component {
public:
  float scale;
  bool mirror;
  ImageTransform(float scale = 1.0f, bool mirror = false) : scale(scale), mirror(mirror) { }
};

class Image : public Component {
public:
  ImageId id;
  int zidx;  // z-index that determines rendering order.
  Image(ImageId id = 0, int zidx = 0) : id(id), zidx(zidx) { }
};

// Text is special images - each character is an image from the font map.
class Text : public Component {
public:
  std::string text;
  std::unordered_map<char, ImageId> font;  // font map
  int fontw;
  int fonth;
  Alignment align;
  Text(
    const std::string& text = "",
    const std::unordered_map<char, ImageId>& font = {},
    int fontw = 0,
    int fonth = 0,
    Alignment align = Alignment::Left
  )
      : text(text), font(font), fontw(fontw), fonth(fonth), align(align) { }
};

class ProgressBar : public Component {
public:
  float percent;    // in [0.0, 1.0]
  int width;        // bar width
  int height;       // bar height
  COLORREF draw;    // border color
  COLORREF fill;    // fill color
  Alignment align;  // alignment
  ProgressBar(
    float percent = 0.0f,
    int width = 0,
    int height = 0,
    COLORREF draw = RGB(0, 0, 0),
    COLORREF fill = RGB(0, 0, 0),
    Alignment align = Alignment::Left
  )
      : percent(percent), width(width), height(height), draw(draw), fill(fill), align(align) { }
};

class Animation : public Component {
public:
  std::vector<ImageId> imgs;  // frames
  size_t frame;               // current frame
  float elapse;               // elapsed time per frame
  bool once;                  // whether to play once
  float cur;                  // current elapsed time
  Animation() : frame(0), elapse(0.0f), once(false), cur(0.0f) { }
  Animation(
    const std::vector<ImageId>& imgs,
    size_t frame = 0,
    float elapse = 0.0f,
    bool once = false,
    float cur = 0.0f
  )
      : imgs(imgs), frame(frame), elapse(elapse), once(once), cur(cur) { }
};

class Flash : public Component {
public:
  float elapse;   // elapsed time per flash
  bool reusable;  // whether it can be flashed again after done.
  float cur;      // current elapsed time
  Flash(float elapse = 0.0f, bool reusable = false, float cur = 0.0f)
      : elapse(elapse), reusable(reusable), cur(cur) { }
};

class Button : public Component {
public:
  ImageId normal;  // image on normal state
  ImageId hover;   // image on hover state
  int width;
  int height;
  std::function<void(std::shared_ptr<World>)> on_click;  // click callback
  Button() : normal(0), hover(0), width(0), height(0) { }
  Button(
    ImageId normal,
    ImageId hover,
    int width,
    int height,
    const std::function<void(std::shared_ptr<World>)>& on_click
  )
      : normal(normal), hover(hover), width(width), height(height), on_click(on_click) { }
};

class Shown : public Component { };

class AnimationDone : public Event {
public:
  Entity ent;
  AnimationDone(Entity ent) : ent(ent) { }
};

class FlashDone : public Event {
public:
  Entity ent;
  FlashDone(Entity ent) : ent(ent) { }
};

class AnimationSystem : public System {
private:
  std::shared_ptr<World> world;

public:
  AnimationSystem(std::shared_ptr<World> world) : world(world) { }

  void update(float dt) override;
};

class FlashSystem : public System {
private:
  std::shared_ptr<World> world;

public:
  FlashSystem(std::shared_ptr<World> world) : world(world) { }

  void update(float dt) override;
};

class ButtonSystem : public System {
private:
  std::shared_ptr<World> world;
  SubscriptionId mm_id, lmd_id;

  std::shared_ptr<SoundManager> sm;
  SoundId hover_sound, click_sound;

  void on_hover(const MouseMoved& ev);
  void on_click(const LeftMouseDown& ev);

public:
  ButtonSystem(std::shared_ptr<World> world) : world(world), sm(world->get_ctx<SoundManager>()) {
    mm_id = world->sub<MouseMoved>([this](const MouseMoved& ev) { on_hover(ev); });
    lmd_id = world->sub<LeftMouseDown>([this](const LeftMouseDown& ev) { on_click(ev); });

    hover_sound = sm->load(L".\\assets\\sounds\\buttonhover.wav");
    click_sound = sm->load(L".\\assets\\sounds\\buttonclick.wav");
  }
  ~ButtonSystem() {
    world->unsub<LeftMouseDown>(lmd_id);
    world->unsub<MouseMoved>(mm_id);
  }

  void update(float) override { /* No operations. */ };
};

Entity make_image(
  std::shared_ptr<World> world,
  ImageId id,
  int x,
  int y,
  int zidx,
  float scale = 1.0f,
  bool mirror = false
);

Entity make_text(
  std::shared_ptr<World> world,
  const std::string& text,
  const Font& font,
  int fontw,
  int fonth,
  int x,
  int y,
  int zidx,
  Alignment align = Alignment::Left,
  float scale = 1.0f
);

Entity make_progress_bar(
  std::shared_ptr<World> world,
  float percent,
  int width,
  int height,
  COLORREF draw,
  COLORREF fill,
  int x,
  int y,
  int zidx,
  Alignment align = Alignment::Left
);

Entity make_animation(
  std::shared_ptr<World> world,
  const std::vector<ImageId>& ids,
  int x,
  int y,
  int zidx,
  float elapse,
  bool once = false,
  float scale = 1.0f,
  bool mirror = false,
  size_t frame = 0
);

Entity make_flash(
  std::shared_ptr<World> world,
  ImageId id,
  int x,
  int y,
  int zidx,
  float elapse,
  bool reusable = false,
  float scale = 1.0f,
  bool mirror = false
);

Entity make_button(
  std::shared_ptr<World> world,
  ImageId id_normal,
  ImageId id_hover,
  int x,
  int y,
  int zidx,
  int width,
  int height,
  std::function<void(std::shared_ptr<World>)> on_click,
  float scale = 1.0f,
  bool mirror = false
);
