#pragma once

#include <algorithm>
#include <cmath>

// Axis-aligned rect vs circle (ball) collision.
inline bool circleIntersectsRect(float cx,
                                 float cy,
                                 float radius,
                                 float rx,
                                 float ry,
                                 float rw,
                                 float rh) {
  const float nearest_x = std::clamp(cx, rx, rx + rw);
  const float nearest_y = std::clamp(cy, ry, ry + rh);
  const float dx = cx - nearest_x;
  const float dy = cy - nearest_y;
  return (dx * dx + dy * dy) <= (radius * radius);
}
