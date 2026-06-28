#pragma once

#include <windows.h>

#include "ecs.h"

class AppQuit : public Event { };

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow);

void init_game(HWND hwnd);

void init_contexts(HWND hwnd);
void init_systems(HWND hwnd);
void init_scenes();
