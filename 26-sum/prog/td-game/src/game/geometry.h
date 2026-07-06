#pragma once

/// geometry.h - Basic 2D vector and rectangle structs.

#include <cmath>

struct Vec2 {
  float x;
  float y;

  Vec2() : x(0.0), y(0.0) { }
  Vec2(float x, float y) : x(x), y(y) { }

  Vec2 operator+(Vec2 other) const { return Vec2(x + other.x, y + other.y); }
  Vec2 operator-(Vec2 other) const { return Vec2(x - other.x, y - other.y); }
  Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
  Vec2 operator/(float scalar) const { return Vec2(x / scalar, y / scalar); }
  Vec2& operator+=(Vec2 other) {
    x += other.x;
    y += other.y;
    return *this;
  }
  Vec2& operator-=(Vec2 other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }
  Vec2& operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
  }
  Vec2& operator/=(float scalar) {
    x /= scalar;
    y /= scalar;
    return *this;
  }

  float length() const { return std::sqrt(x * x + y * y); }
  float length_sq() const { return x * x + y * y; }
  Vec2 normalized() const {
    float len = length();
    return len > 0.0f ? Vec2(x / len, y / len) : Vec2(0.0f, 0.0f);
  }
  float distance(Vec2 other) const { return (*this - other).length(); }

  float dot(Vec2 other) const { return x * other.x + y * other.y; }
  float cross(Vec2 other) const { return x * other.y - y * other.x; }

  Vec2 rotated(float angle_rad) const {
    float cos_a = std::cos(angle_rad);
    float sin_a = std::sin(angle_rad);
    return Vec2(x * cos_a - y * sin_a, x * sin_a + y * cos_a);
  }
};

struct Rect {
  Vec2 center;
  float width;
  float height;

  Rect() : center(0.0f, 0.0f), width(0.0f), height(0.0f) { }
  Rect(Vec2 center, float width, float height) : center(center), width(width), height(height) { }

  Rect operator+(Vec2 offset) const { return Rect(center + offset, width, height); }
  Rect operator-(Vec2 offset) const { return Rect(center - offset, width, height); }
  Rect& operator+=(Vec2 offset) {
    center += offset;
    return *this;
  }
  Rect& operator-=(Vec2 offset) {
    center -= offset;
    return *this;
  }

  bool contains(Vec2 point) const {
    float half_width = width * 0.5f;
    float half_height = height * 0.5f;
    return point.x >= center.x - half_width && point.x <= center.x + half_width &&
           point.y >= center.y - half_height && point.y <= center.y + half_height;
  }
};
