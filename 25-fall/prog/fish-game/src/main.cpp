#include "main.h"

#include "ai.h"
#include "collision.h"
#include "config.h"
#include "ecs.h"
#include "effect.h"
#include "game.h"
#include "health.h"
#include "movement.h"
#include "player.h"
#include "render.h"
#include "role.h"
#include "scene.h"
#include "score.h"
#include "spawn.h"
#include "timer.h"
#include "ui.h"

static std::shared_ptr<World> world;
static constexpr UINT_PTR timer_id = 1;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  static auto last_tp = std::chrono::steady_clock::now();

  switch (msg) {
  case WM_CREATE:
    init_game(hwnd);
    SetTimer(hwnd, timer_id, 16, NULL);
    return 0;

  case WM_DESTROY:
    world->emit_now(AppQuit());
    return 0;

  case WM_PAINT:
    world->sys<RenderSystem>()->render();
    return 0;

  case WM_TIMER: {
    auto tp = std::chrono::steady_clock::now();
    if (world && wparam == timer_id) {
      world->update(std::chrono::duration<float>(tp - last_tp).count());
      last_tp = tp;
    }
    return 0;
  }

  case WM_MOUSEMOVE:
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
  case WM_KEYDOWN:
  case WM_KEYUP:
    world->sys<InputSystem>()->handle_msg(msg, wparam, lparam);
    return 0;

  case WM_KILLFOCUS:
    world->emit(GamePaused());
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
  const wchar_t CLASS_NAME[] = L"GameWindow";

  WNDCLASSEX wc = {};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(MAIN_ICON));
  wc.lpszClassName = CLASS_NAME;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

  RegisterClassEx(&wc);

  HWND hwnd = CreateWindowEx(
    0,
    CLASS_NAME,
    L"Fish Game",
    WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    1024,
    768,
    nullptr,
    nullptr,
    hInstance,
    nullptr
  );
  if (!hwnd) return 0;

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}

void init_game(HWND hwnd) {
  world = std::make_shared<World>();

  init_contexts(hwnd);
  init_systems(hwnd);
  init_scenes();

  world->emit_now(SceneChanged(Scene::StartMenu));
}

void init_contexts(HWND hwnd) {
  world->add_ctx<HWND>(hwnd);
  world->add_ctx<WindowConfig>(1024, 768);
  world->add_ctx<VisualConfig>(load_json<VisualConfig>(L".\\assets\\visuals.json"));
  world->add_ctx<BitmapManager>();
  world->add_ctx<SoundManager>();
  world->add_ctx<std::mt19937>(std::random_device()());
}

void init_systems(HWND hwnd) {
  world->reg(std::make_shared<InputSystem>(world));
  world->reg(std::make_shared<SceneSystem>(world));
  world->reg(std::make_shared<TimerSystem>(world));
  world->reg(std::make_shared<GameSystem>(world));
  world->reg(std::make_shared<SpawnSystem>(world));
  world->reg(std::make_shared<PlayerControlSystem>(world));
  world->reg(std::make_shared<AISystem>(world));
  world->reg(std::make_shared<MovementSystem>(world));
  world->reg(std::make_shared<CollisionSystem>(world));
  world->reg(std::make_shared<EffectSystem>(world));
  world->reg(std::make_shared<ScoreSystem>(world));
  world->reg(std::make_shared<HealthSystem>(world));
  world->reg(std::make_shared<ButtonSystem>(world));
  world->reg(std::make_shared<AnimationSystem>(world));
  world->reg(std::make_shared<FlashSystem>(world));
  world->reg(std::make_shared<FishRenderSystem>(world));
  world->reg(std::make_shared<RenderSystem>(world));

  world->sub_once<AppQuit>([hwnd](const AppQuit&) {
    KillTimer(hwnd, timer_id);
    PostQuitMessage(0);
    return true;
  });
}

void init_scenes() {
  auto bm = world->get_ctx<BitmapManager>();
  ImageId loading_bg = bm->load(L".\\assets\\backgrounds\\loading.bmp");
  ImageId menu_bg = bm->load(L".\\assets\\backgrounds\\menu.bmp");

  ImageId title = bm->load(L".\\assets\\foregrounds\\title.bmp");
  ImageId help_text = bm->load(L".\\assets\\foregrounds\\help.bmp");
  ImageId levelselect = bm->load(L".\\assets\\foregrounds\\levelselect.bmp");
  ImageId locked = bm->load(L".\\assets\\foregrounds\\locked.bmp");
  ImageId paused = bm->load(L".\\assets\\foregrounds\\paused.bmp");

  ImageId start = bm->load(L".\\assets\\buttons\\start.bmp");
  ImageId start_hover = bm->load(L".\\assets\\buttons\\start_hover.bmp");
  ImageId help = bm->load(L".\\assets\\buttons\\help.bmp");
  ImageId help_hover = bm->load(L".\\assets\\buttons\\help_hover.bmp");
  ImageId exit = bm->load(L".\\assets\\buttons\\exit.bmp");
  ImageId exit_hover = bm->load(L".\\assets\\buttons\\exit_hover.bmp");
  ImageId back = bm->load(L".\\assets\\buttons\\back.bmp");
  ImageId back_hover = bm->load(L".\\assets\\buttons\\back_hover.bmp");
  ImageId pause = bm->load(L".\\assets\\buttons\\pause.bmp");
  ImageId pause_hover = bm->load(L".\\assets\\buttons\\pause_hover.bmp");
  ImageId resume = bm->load(L".\\assets\\buttons\\resume.bmp");
  ImageId resume_hover = bm->load(L".\\assets\\buttons\\resume_hover.bmp");
  ImageId restart = bm->load(L".\\assets\\buttons\\restart.bmp");
  ImageId restart_hover = bm->load(L".\\assets\\buttons\\restart_hover.bmp");
  ImageId quit = bm->load(L".\\assets\\buttons\\quit.bmp");
  ImageId quit_hover = bm->load(L".\\assets\\buttons\\quit_hover.bmp");

  const int win_width = world->get_ctx<WindowConfig>()->width;
  const int win_height = world->get_ctx<WindowConfig>()->height;

  using enum Scene;
  auto scene = world->sys<SceneSystem>();

  scene->reg(Loading, make_image(world, loading_bg, win_width / 2, win_height / 2, zindex::BG));

  scene->reg(StartMenu, make_image(world, menu_bg, win_width / 2, win_height / 2, zindex::BG));
  scene->reg(StartMenu, make_image(world, title, win_width / 2, 200, zindex::UI));
  scene->reg(
    StartMenu,
    make_button(
      world,
      start,
      start_hover,
      win_width / 2,
      400,
      zindex::UI,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit(SceneChanged(LevelSelect)); }
    )
  );
  scene->reg(
    StartMenu,
    make_button(
      world,
      help,
      help_hover,
      win_width / 2,
      500,
      zindex::UI,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit(SceneChanged(Help)); }
    )
  );

  scene->reg(
    StartMenu,
    make_button(
      world,
      exit,
      exit_hover,
      win_width / 2,
      600,
      zindex::UI,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit_now(AppQuit()); }
    )
  );

  scene->reg(Help, make_image(world, menu_bg, win_width / 2, win_height / 2, zindex::BG));
  scene->reg(Help, make_image(world, help_text, win_width / 2, win_height / 2, zindex::UI));
  scene->reg(
    Help,
    make_button(
      world,
      back,
      back_hover,
      win_width / 2,
      600,
      zindex::UI,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit(SceneChanged(StartMenu)); }
    )
  );

  scene->reg(LevelSelect, make_image(world, menu_bg, win_width / 2, win_height / 2, zindex::BG));
  scene->reg(LevelSelect, make_image(world, levelselect, win_width / 2, 100, zindex::UI));
  scene->reg(
    LevelSelect,
    make_button(
      world,
      back,
      back_hover,
      win_width / 2,
      600,
      zindex::UI,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit(SceneChanged(StartMenu)); }
    )
  );

  for (int i = 1; i <= 3; i++)
    for (int j = 1; j <= 4; j++) {
      int level = (i - 1) * 4 + j;
      std::wstring config = std::format(L".\\assets\\levels\\level{}.json", level);
      if (std::ifstream fin(config); !fin) continue;

      int x = win_width / 2 - 300 + (j - 1) * 200;
      int y = i * 150 + 50;
      ImageId button = bm->load(std::format(L".\\assets\\buttons\\level{}.bmp", level));
      ImageId button_hover = bm->load(std::format(L".\\assets\\buttons\\level{}_hover.bmp", level));

      scene->reg(
        LevelSelect,
        make_button(
          world,
          button,
          button_hover,
          x,
          y,
          zindex::UI,
          65,
          65,
          [config = std::move(config)](std::shared_ptr<World> w) { w->emit(GamePrepared(config)); }
        )
      );
    }

  scene->reg(LevelLocked, make_image(world, menu_bg, win_width / 2, win_height / 2, zindex::BG));
  scene->reg(LevelLocked, make_image(world, locked, win_width / 2, win_height / 2, zindex::UI));
  scene->reg(
    LevelLocked,
    make_button(
      world,
      back,
      back_hover,
      win_width / 2,
      600,
      zindex::UI,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit(SceneChanged(LevelSelect)); }
    )
  );

  scene->reg(
    LevelDesc,
    make_button(
      world,
      start,
      start_hover,
      win_width / 2,
      500,
      zindex::UI + 1,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit(GameStarted()); }
    )
  );
  scene->reg(
    LevelDesc,
    make_button(
      world,
      back,
      back_hover,
      win_width / 2,
      600,
      zindex::UI + 1,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit(GameQuit()); }
    )
  );

  scene->reg(
    Game,
    make_button(
      world,
      pause,
      pause_hover,
      win_width - 50,
      50,
      zindex::UI,
      50,
      50,
      [](std::shared_ptr<World> w) { w->emit(GamePaused()); }
    )
  );

  scene->reg(GamePause, make_image(world, paused, win_width / 2, 100, zindex::UI));
  scene->reg(
    GamePause,
    make_button(
      world,
      resume,
      resume_hover,
      win_width / 2,
      600,
      zindex::OVERLAY,
      50,
      50,
      [](std::shared_ptr<World> w) { w->emit(GameResumed()); }
    )
  );
  scene->reg(
    GamePause,
    make_button(
      world,
      restart,
      restart_hover,
      win_width / 2 + 100,
      600,
      zindex::OVERLAY,
      50,
      50,
      [](std::shared_ptr<World> w) { w->emit(GameRestarted()); }
    )
  );
  scene->reg(
    GamePause,
    make_button(
      world,
      quit,
      quit_hover,
      win_width / 2 - 100,
      600,
      zindex::OVERLAY,
      50,
      50,
      [](std::shared_ptr<World> w) { w->emit(GameQuit()); }
    )
  );

  scene->reg(GameWin, make_image(world, menu_bg, win_width / 2, win_height / 2, zindex::BG));
  scene->reg(
    GameWin,
    make_button(
      world,
      back,
      back_hover,
      win_width / 2,
      500,
      zindex::UI,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit(GameQuit(true)); }
    )
  );

  scene->reg(GameLose, make_image(world, menu_bg, win_width / 2, win_height / 2, zindex::BG));
  scene->reg(
    GameLose,
    make_button(
      world,
      back,
      back_hover,
      win_width / 2,
      500,
      zindex::UI,
      200,
      60,
      [](std::shared_ptr<World> w) { w->emit(GameQuit(false)); }
    )
  );
}
