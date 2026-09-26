// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

// One visual character per instrument, drawn flat.
//
// The drawing language is Rill's: opaque shapes with hard edges on a coloured
// ground, no additive blending, no dithering, no soft falloff, and colour at
// an ink or a step of that ink let down toward the ground. What differs is
// what drives it. Rill's families answer a smoothed output level; these
// answer individual strikes, with the pitch of each one, because that is what
// a mallet instrument gives you and a level meter throws away.
//
// The character is chosen independently of the instrument. Pairing them one
// to one was tried and is the wrong idea: it turns seven instruments and
// seven characters into seven fixed pieces, when the point of a generative
// instrument is the combinations it finds. A new generation draws a new
// instrument and a new character separately, and a shake draws a character
// again without touching the sound, so the vibraphone can arrive under the
// ripple field or a kumiko panel and neither of them is its.
//
// Shared by firmware and the host preview tools.
namespace resonance {
class Painting {
 public:
  static constexpr unsigned width = 240, height = 135;
  static constexpr unsigned characterCount = 8;
  enum Character : unsigned { Asanoha = 0, Uroko, Ring, Kikko, Ripple, Bloom, Snap, Moons };

 private:
  struct Color { float r, g, b; };
  struct Mark { float x, y, size, age; unsigned tint, shape; };
  // Bloom: one petal, and how far it has grown out of the centre.
  struct Petal { float angle, length, width, grown; unsigned tint; };

  std::array<uint16_t, width * height> frame{};
  std::array<float, 257> wave{};

  uint32_t rng = 1, evolutionRng = 1;
  unsigned character = 0, palette = 0, count = 0;
  float phase = 0, breath = 0;
  std::array<float, 8> parameters{};
  std::array<float, 8> evolving{}, goals{};
  float evolutionAt = 0;
  Color ink[3]{};
  Color groundColor{232, 220, 192};
  uint16_t ground = 0;

  std::array<Mark, 40> marks{};
  unsigned markCount = 0, nextMark = 0;
  // Kumiko: how far each piece of the lattice has grown in, and its colour.
  // The geometry is worked out when it is needed, not stored.
  std::array<float, 360> pieces{};
  std::array<uint8_t, 360> pieceTints{};
  unsigned piecesFilled = 0, piecesShown = 0;
  int heardLow = 60, heardHigh = 72;
  // Ripple: each disc's swell, the knock waiting for it, and how much of the
  // playing it still remembers.
  static constexpr unsigned discCount = 9;
  std::array<float, discCount> swell{}, knock{}, memory{};
  // Moons: how far each moon has turned through its phases.
  static constexpr unsigned moonCount = 5;
  std::array<float, moonCount> moonPhase{}, moonGoal{};
  std::array<Petal, 20> petals{};
  unsigned petalCount = 0;
  std::array<float, 12> rings{};
  unsigned ringCount = 0, nextRing = 0;
  float clearAt = 0, bloomTurn = 0, petalShape = 0;
  int registerLow = 48, registerHigh = 84;

  unsigned random() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
  float unit() { return float(random() >> 8) / 16777216.0f; }
  float range(float low, float high) { return low + unit() * (high - low); }
  float evolveUnit() {
    evolutionRng ^= evolutionRng << 13; evolutionRng ^= evolutionRng >> 17; evolutionRng ^= evolutionRng << 5;
    return float(evolutionRng >> 8) / 16777216.0f;
  }
  float fsin(float turns) const {
    float t = turns - std::floor(turns);
    float f = t * 256.0f;
    unsigned i = unsigned(f);
    if (i > 255) i = 255;
    return wave[i] + (wave[i + 1] - wave[i]) * (f - float(i));
  }
  float fcos(float turns) const { return fsin(turns + 0.25f); }

  static uint16_t color(Color c, float shade = 1) {
    unsigned r = unsigned(std::max(0.0f, std::min(255.0f, c.r * shade)));
    unsigned g = unsigned(std::max(0.0f, std::min(255.0f, c.g * shade)));
    unsigned b = unsigned(std::max(0.0f, std::min(255.0f, c.b * shade)));
    return uint16_t(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
  }
  // An ink let down toward the ground rather than stepped toward black: on a
  // daylight palette, darkening is what turns a warm colour to mud.
  uint16_t wash(const Color& c, float mix) const {
    return color({c.r + (groundColor.r - c.r) * mix,
                  c.g + (groundColor.g - c.g) * mix,
                  c.b + (groundColor.b - c.b) * mix});
  }
  void span(int y, int left, int right, uint16_t c) {
    if (y < 0 || y >= int(height)) return;
    left = std::max(0, left); right = std::min(int(width) - 1, right);
    for (int x = left; x <= right; ++x) frame[unsigned(y) * width + unsigned(x)] = c;
  }
  void plot(int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= int(width) || y >= int(height)) return;
    frame[unsigned(y) * width + unsigned(x)] = c;
  }
  void rect(float x0, float y0, float x1, float y1, uint16_t c) {
    int top = std::max(0, int(y0)), bottom = std::min(int(height) - 1, int(y1));
    for (int y = top; y <= bottom; ++y) span(y, int(x0), int(x1), c);
  }
  void disc(float cx, float cy, float r, uint16_t c) {
    if (r < 0.5f) return;
    int top = std::max(0, int(std::floor(cy - r))), bottom = std::min(int(height) - 1, int(std::ceil(cy + r)));
    for (int y = top; y <= bottom; ++y) {
      float v = (float(y) + 0.5f - cy) / r;
      if (std::abs(v) > 1) continue;
      float extent = r * std::sqrt(1 - v * v);
      span(y, int(std::ceil(cx - extent)), int(std::floor(cx + extent)), c);
    }
  }
  void ring(float cx, float cy, float r, float thickness, uint16_t c) {
    disc(cx, cy, r, c);
    if (r > thickness) disc(cx, cy, r - thickness, ground);
  }
  // A rectangle with round ends: the shape of a bar, and the only curve in
  // the set that reads as warm rather than mechanical.
  void lozenge(float cx, float cy, float w, float h, uint16_t c) {
    float r = std::min(w, h) * 0.5f;
    rect(cx - w * 0.5f + r, cy - h * 0.5f, cx + w * 0.5f - r, cy + h * 0.5f, c);
    disc(cx - w * 0.5f + r, cy, r, c);
    disc(cx + w * 0.5f - r, cy, r, c);
  }
  void line(float x0, float y0, float x1, float y1, uint16_t c) {
    int steps = int(std::max(std::abs(x1 - x0), std::abs(y1 - y0))) + 1;
    steps = std::min(steps, 400);
    float ix = (x1 - x0) / float(steps), iy = (y1 - y0) / float(steps);
    float x = x0, y = y0;
    for (int i = 0; i <= steps; ++i) { plot(int(x), int(y), c); x += ix; y += iy; }
  }
  void triangle(float ax, float ay, float bx, float by, float cx, float cy, uint16_t c) {
    int top = std::max(0, int(std::floor(std::min(ay, std::min(by, cy)))));
    int bottom = std::min(int(height) - 1, int(std::ceil(std::max(ay, std::max(by, cy)))));
    const float xs[3] = {ax, bx, cx}, ys[3] = {ay, by, cy};
    for (int y = top; y <= bottom; ++y) {
      float scan = float(y) + 0.5f, crossings[3];
      unsigned n = 0;
      for (unsigned i = 0; i < 3; ++i) {
        unsigned j = (i + 1) % 3;
        if ((ys[i] <= scan && ys[j] > scan) || (ys[j] <= scan && ys[i] > scan))
          crossings[n++] = xs[i] + (scan - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]);
      }
      if (n >= 2) span(y, int(std::ceil(std::min(crossings[0], crossings[1]))),
                          int(std::floor(std::max(crossings[0], crossings[1]))), c);
    }
  }
  void clear() { frame.fill(ground); }

  void evolve(float dt) {
    evolutionAt -= dt;
    if (evolutionAt <= 0) {
      evolutionAt = 5 + evolveUnit() * 8;
      for (auto& g : goals) g = evolveUnit();
    }
    for (unsigned i = 0; i < goals.size(); ++i) evolving[i] += (goals[i] - evolving[i]) * dt * 0.4f;
  }

  // Pitch drives placement in every character that places anything, so it is
  // normalized against the register of the instrument playing rather than
  // against MIDI's 128 notes. Normalized globally, a glockenspiel only ever
  // reaches the right-hand half of the frame and a marimba the left.
  float place(uint8_t note) const {
    float t = (float(note) - float(registerLow)) / float(std::max(6, registerHigh - registerLow));
    return std::max(0.0f, std::min(1.0f, t));
  }

  // Every strike pushes a ring out from the core, and the rings stay until
  // they leave the frame.
  void renderRing(float dt, uint8_t note, float weight) {
    if (note) {
      rings[nextRing] = 3.0f + weight * 5.0f;
      nextRing = (nextRing + 1) % rings.size();
      if (ringCount < rings.size()) ++ringCount;
    }
    float cx = 120 + (evolving[0] - 0.5f) * 90, cy = 67 + (evolving[1] - 0.5f) * 50;
    float speed = 8.0f + evolving[2] * 9.0f;
    clear();
    // Oldest first, which means largest first. A ring punches its own hole
    // back to the ground, so drawing the newest first has every later,
    // larger ring erase everything already inside it.
    for (unsigned step = 0; step < ringCount; ++step) {
      unsigned i = ringCount - 1 - step;
      unsigned slot = (nextRing + rings.size() - 1 - i) % rings.size();
      rings[slot] += speed * dt;
      float r = rings[slot];
      if (r > 200) continue;
      // Older rings are further out and let down toward the ground, so the
      // near ones read as the ones just played.
      float distance = std::min(1.0f, r / 130.0f);
      ring(cx, cy, r, 4.0f + (1.0f - distance) * 7.0f + breath * 2.0f,
           wash(ink[i % 3], distance * 0.6f));
    }
  }

  // A row of discs on a horizon, one to each stretch of the register. A
  // strike swells its disc and knocks its neighbours after it; the swell
  // settles back and the row goes still when the music does. A disc keeps a
  // little of its size for a while, so the row at rest still shows where the
  // melody has been.
  void renderRipple(float dt, uint8_t note, float weight) {
    if (note) {
      unsigned i = std::min(discCount - 1, unsigned(place(note) * float(discCount)));
      swell[i] = std::min(1.2f, swell[i] + 0.5f + weight * 0.5f);
      memory[i] = std::min(1.0f, memory[i] + 0.35f);
      float push = 0.35f * weight + 0.15f;
      if (i > 0) knock[i - 1] = std::max(knock[i - 1], push);
      if (i + 1 < discCount) knock[i + 1] = std::max(knock[i + 1], push);
    }
    clear();
    float horizon = 72 + (evolving[1] - 0.5f) * 16;
    span(int(horizon), 0, width - 1, wash(ink[2], 0.45f));
    const float spacing = 216.0f / float(discCount - 1);
    float radius[discCount];
    for (unsigned i = 0; i < discCount; ++i) {
      swell[i] = std::max(0.0f, swell[i] - dt * (0.9f + swell[i]));
      memory[i] = std::max(0.0f, memory[i] - dt * 0.05f);
      if (knock[i] > 0) { swell[i] = std::max(swell[i], knock[i]); knock[i] = 0; }
      radius[i] = 4 + memory[i] * 7 + swell[i] * 17 + breath * 1.5f;
    }
    // Echo rings first, then the discs largest first, so a small neighbour
    // is drawn over a swollen one rather than swallowed by it.
    for (unsigned i = 0; i < discCount; ++i)
      if (swell[i] > 0.45f)
        ring(12 + float(i) * spacing, horizon, radius[i] + 5 + swell[i] * 6, 2.0f,
             wash(ink[(i + 1) % 3], 0.35f));
    unsigned order[discCount];
    for (unsigned i = 0; i < discCount; ++i) order[i] = i;
    std::sort(order, order + discCount, [&](unsigned a, unsigned b) { return radius[a] > radius[b]; });
    for (unsigned i : order)
      disc(12 + float(i) * spacing, horizon, radius[i], wash(ink[i % 3], swell[i] > 0.2f ? 0.0f : 0.3f));
  }

  // Hard angular marks that arrive on a strike and are gone by the next. A
  // strike bursts rather than lands: the shards carry the attack, which is
  // what a single flat triangle could not.
  void renderSnap(float dt, uint8_t note, float weight) {
    if (note) {
      if (markCount < marks.size()) markCount += 5;
      for (unsigned i = markCount - 1; i >= 5; --i) marks[i] = marks[i - 5];
      float x = 34 + place(note) * 172, y = 34 + range(0.0f, 68.0f);
      float spread = 26.0f + weight * 44.0f;
      // Five shards from one point, thrown outward. A strike used to put down
      // a single flat triangle, which read as a shape appearing rather than
      // as something being struck.
      for (unsigned k = 0; k < 5; ++k) {
        Mark& m = marks[k];
        float a = unit();
        float reach = spread * range(0.35f, 1.25f);
        m.x = x + fcos(a) * reach;
        m.y = y + fsin(a) * reach * 0.7f;
        m.size = (15.0f + weight * 22.0f) * range(0.55f, 1.25f);
        m.tint = (unsigned(place(note) * 3.0f) + k) % 3;
        m.shape = k % 3;
        m.age = 1;
      }
    }
    clear();
    for (unsigned i = 0; i < markCount; ++i) {
      Mark& m = marks[i];
      m.age = std::max(0.0f, m.age - dt * (0.20f + float(i / 5) * 0.28f));
      if (m.age <= 0) continue;
      float s = m.size * (0.45f + 0.55f * m.age);
      uint16_t c = wash(ink[m.tint], i < 5 ? 0.0f : 0.40f);
      if (m.shape == 0)
        triangle(m.x - s * 0.5f, m.y + s * 0.4f, m.x + s * 0.5f, m.y + s * 0.4f, m.x, m.y - s * 0.6f, c);
      else if (m.shape == 1) {
        triangle(m.x - s * 0.6f, m.y + s * 0.2f, m.x, m.y - s * 0.4f, m.x, m.y + s * 0.5f, c);
        triangle(m.x + s * 0.6f, m.y + s * 0.2f, m.x, m.y - s * 0.4f, m.x, m.y + s * 0.5f, c);
      } else
        rect(m.x - s * 0.2f, m.y - s * 0.55f, m.x + s * 0.2f, m.y + s * 0.55f, c);
    }
  }

  // A kumiko panel: a lattice of strips over paper, cut into pieces, and
  // each strike fills one piece at the column its pitch sets, growing out
  // from its centre. The lattice is there from the start; what the music
  // adds is the colour, until the panel is nearly full and a new one is
  // started. Three traditional lattices: asanoha, the hemp leaf, whose
  // triangles are cut in three from the centre; uroko, the scales, whose
  // triangles are whole; and kikko, the tortoise shell, whose hexagons are cut
  // into three diamonds and so fill in as stars and stacked cubes.
  //
  // Pitch is spread over the notes actually heard rather than the whole
  // register: a melody that keeps to a few notes would otherwise fill one
  // side of the panel and leave the other bare.
  float spread(uint8_t note) {
    heardLow = std::min(heardLow, int(note));
    heardHigh = std::max(heardHigh, int(note));
    float t = (float(note) - float(heardLow)) / float(std::max(5, heardHigh - heardLow));
    return std::max(0.0f, std::min(1.0f, t));
  }
  // The corners of one triangle of the asanoha and uroko grid, pointing up
  // or down.
  static void gridTriangle(unsigned row, unsigned t, float* xs, float* ys) {
    const float side = 32, rise = 27.7f;
    float x = float(t / 2) * side - (row % 2 ? side * 0.5f : 0) - 8, y = float(row) * rise - 3;
    if (t % 2 == 0) {
      xs[0] = x; ys[0] = y + rise; xs[1] = x + side; ys[1] = y + rise; xs[2] = x + side * 0.5f; ys[2] = y;
    } else {
      xs[0] = x + side * 0.5f; ys[0] = y; xs[1] = x + side * 1.5f; ys[1] = y; xs[2] = x + side; ys[2] = y + rise;
    }
  }
  static constexpr unsigned gridRows = 6, gridTriangles = 20, shellRows = 7, shellColumns = 10;
  // The centre and corners of one hexagon of the kikko lattice, point up.
  void shell(unsigned row, unsigned column, float& cx, float& cy, float* xs, float* ys) const {
    const float r = 17, across = r * 1.732f;
    cx = float(column) * across + (row % 2 ? across * 0.5f : 0) - 6;
    cy = float(row) * r * 1.5f - 4;
    for (unsigned k = 0; k < 6; ++k) {
      xs[k] = cx + fcos(float(k) / 6 + 1.0f / 12) * r;
      ys[k] = cy + fsin(float(k) / 6 + 1.0f / 12) * r;
    }
  }
  unsigned pieceCount() const {
    return character == Asanoha ? gridRows * gridTriangles * 3
         : character == Uroko ? gridRows * gridTriangles : shellRows * shellColumns * 3;
  }
  // One piece: the point it grows from, and the corners it is fanned to
  // from there, in order.
  unsigned piece(unsigned i, float& ox, float& oy, float* xs, float* ys) const {
    float vx[6], vy[6];
    if (character == Kikko) {
      shell(i / 3 / shellColumns, i / 3 % shellColumns, ox, oy, vx, vy);
      unsigned k = i % 3 * 2;
      for (unsigned j = 0; j < 3; ++j) { xs[j] = vx[(k + j) % 6]; ys[j] = vy[(k + j) % 6]; }
      return 3;
    }
    unsigned cell = character == Asanoha ? i / 3 : i;
    gridTriangle(cell / gridTriangles, cell % gridTriangles, vx, vy);
    ox = (vx[0] + vx[1] + vx[2]) / 3; oy = (vy[0] + vy[1] + vy[2]) / 3;
    if (character == Asanoha) {
      unsigned k = i % 3;
      xs[0] = vx[k]; ys[0] = vy[k]; xs[1] = vx[(k + 1) % 3]; ys[1] = vy[(k + 1) % 3];
      return 2;
    }
    for (unsigned j = 0; j < 4; ++j) { xs[j] = vx[j % 3]; ys[j] = vy[j % 3]; }
    return 4;
  }
  // Where a piece sits, for placing it by pitch and for leaving out the ones
  // the frame cuts off.
  bool pieceCentre(unsigned i, float& x, float& y) const {
    float ox, oy, xs[4], ys[4];
    unsigned n = piece(i, ox, oy, xs, ys);
    x = ox; y = oy;
    for (unsigned j = 0; j < n; ++j) { x += xs[j]; y += ys[j]; }
    x /= float(n + 1); y /= float(n + 1);
    return x > 4 && y > 4 && x < float(width) - 4 && y < float(height) - 4;
  }
  // A strip two pixels wide, thickened across its own direction so the
  // horizontals and the diagonals read as the same wood.
  void strip(float x0, float y0, float x1, float y1, uint16_t c) {
    line(x0, y0, x1, y1, c);
    if (std::abs(x1 - x0) > std::abs(y1 - y0)) line(x0, y0 - 1, x1, y1 - 1, c);
    else line(x0 - 1, y0, x1 - 1, y1, c);
  }
  void renderKumiko(float dt, uint8_t note, float weight) {
    const unsigned total = pieceCount();
    if (note && clearAt > 2.0f) {
      float at = 10 + spread(note) * 220;
      // Try nearest the pitch first, then a little wider, so a column that
      // is already full passes the note to its neighbour.
      for (float reach : {16.0f, 34.0f}) {
        unsigned options[360], n = 0;
        for (unsigned i = 0; i < total; ++i) {
          float x, y;
          if (pieces[i] <= 0 && pieceCentre(i, x, y) && std::abs(x - at) < reach) options[n++] = i;
        }
        if (!n) continue;
        unsigned i = options[random() % n];
        pieces[i] = 0.01f;
        pieceTints[i] = uint8_t((unsigned(at / 40) + unsigned(weight * 2.0f) + random() % 2) % 3);
        if (++piecesFilled >= piecesShown * 5 / 12) clearAt = 2.0f;
        break;
      }
    }
    if (clearAt <= 2.0f) {
      clearAt -= dt;
      if (clearAt <= 0) { pieces.fill(0); piecesFilled = 0; clearAt = 99; }
    }
    clear();
    static const float tints[3] = {0.0f, 0.22f, 0.45f};
    for (unsigned i = 0; i < total; ++i) {
      float& grown = pieces[i];
      if (grown <= 0) continue;
      grown = std::min(1.0f, grown + dt * 5.0f);
      float ox, oy, xs[4], ys[4];
      unsigned n = piece(i, ox, oy, xs, ys);
      uint16_t c = wash(ink[pieceTints[i]], tints[i % 3]);
      for (unsigned j = 0; j + 1 < n; ++j)
        triangle(ox, oy, ox + (xs[j] - ox) * grown, oy + (ys[j] - oy) * grown,
                 ox + (xs[j + 1] - ox) * grown, oy + (ys[j + 1] - oy) * grown, c);
    }
    // The strips go over the pieces, the way the wood holds the paper.
    uint16_t wood = wash(ink[2], 0.55f);
    float xs[6], ys[6];
    if (character == Kikko) {
      for (unsigned row = 0; row < shellRows; ++row)
        for (unsigned column = 0; column < shellColumns; ++column) {
          float cx, cy;
          shell(row, column, cx, cy, xs, ys);
          for (unsigned k = 0; k < 6; ++k) {
            strip(xs[k], ys[k], xs[(k + 1) % 6], ys[(k + 1) % 6], wood);
            if (k % 2 == 0) strip(cx, cy, xs[k], ys[k], wood);
          }
        }
      return;
    }
    for (unsigned row = 0; row < gridRows; ++row)
      for (unsigned t = 0; t < gridTriangles; ++t) {
        gridTriangle(row, t, xs, ys);
        float cx = (xs[0] + xs[1] + xs[2]) / 3, cy = (ys[0] + ys[1] + ys[2]) / 3;
        for (unsigned k = 0; k < 3; ++k) {
          strip(xs[k], ys[k], xs[(k + 1) % 3], ys[(k + 1) % 3], wood);
          if (character == Asanoha) line(cx, cy, xs[k], ys[k], wood);
        }
      }
  }

  // A row of moons across the sky, one to each stretch of the register. A
  // strike turns its moon a step through its phases, so a melody that dwells
  // on one note waxes one moon to full and wanes it again.
  void renderMoons(float dt, uint8_t note, float weight) {
    if (note) {
      unsigned i = std::min(moonCount - 1, unsigned(place(note) * float(moonCount)));
      moonGoal[i] += 0.08f + weight * 0.12f;
    }
    clear();
    float sky = 67 + (evolving[1] - 0.5f) * 16;
    for (unsigned i = 0; i < moonCount; ++i) {
      moonPhase[i] += (moonGoal[i] - moonPhase[i]) * std::min(1.0f, dt * 6.0f);
      float cx = 28 + float(i) * 46, cy = sky + fsin(float(i) * 0.2f + evolving[0]) * 18;
      float r = 13 + float((i * 2) % 3) * 2;
      float p = moonPhase[i] - std::floor(moonPhase[i]);
      // The shadow slides across the face: new at 0, full at a half, new
      // again at 1, and never so far out that the face goes blank for long.
      float offset = (p < 0.5f ? 1 - p * 4 : (p - 0.5f) * 4 - 1) * r * 2.05f;
      ring(cx, cy, r + 5, 1.5f, wash(ink[(i + 2) % 3], 0.4f));
      disc(cx, cy, r, wash(ink[i % 3], 0.7f));
      // The lit face is the disc less the shadow, row by row: at most two
      // spans a row, and every edge stays hard.
      uint16_t lit = wash(ink[i % 3], 0.0f);
      int top = std::max(0, int(std::floor(cy - r))), bottom = std::min(int(height) - 1, int(std::ceil(cy + r)));
      for (int y = top; y <= bottom; ++y) {
        float v = (float(y) + 0.5f - cy) / r;
        if (std::abs(v) > 1) continue;
        float e = r * std::sqrt(1 - v * v);
        int left = int(std::ceil(cx - e)), right = int(std::floor(cx + e));
        int shadowLeft = int(std::ceil(cx + offset - e)), shadowRight = int(std::floor(cx + offset + e));
        if (shadowRight < left || shadowLeft > right) { span(y, left, right, lit); continue; }
        if (shadowLeft > left) span(y, left, shadowLeft - 1, lit);
        if (shadowRight < right) span(y, shadowRight + 1, right, lit);
      }
    }
  }

  // Petals opening one to a strike, at an angle the pitch sets, until the
  // rosette is full and another starts. The wheel this replaces turned on its
  // own whatever was played, which is the definition of not listening.
  void renderBloom(float dt, uint8_t note, float weight) {
    if (note) {
      if (petalCount >= petals.size()) { petalCount = 0; bloomTurn = unit(); petalShape = range(0.1f, 0.9f); }
      Petal& p = petals[petalCount++];
      // Successive petals step round by the golden angle, offset by pitch, so
      // a rosette fills evenly however the melody moves.
      p.angle = bloomTurn + float(petalCount) * 0.381966f + (place(note) - 0.5f) * 0.12f;
      p.length = 40 + weight * 48 + place(note) * 30;
      p.width = 0.038f + petalShape * 0.055f;
      p.tint = petalCount % 3;
      p.grown = 0;
    }
    float cx = 120 + (evolving[0] - 0.5f) * 34, cy = 67 + (evolving[1] - 0.5f) * 22;
    clear();
    for (unsigned i = 0; i < petalCount; ++i) {
      Petal& p = petals[i];
      p.grown = std::min(1.0f, p.grown + dt * 4.5f);
      float ease = p.grown * p.grown * (3 - 2 * p.grown);
      float length = p.length * ease;
      float a0 = p.angle - p.width, a1 = p.angle + p.width;
      static const float tints[3] = {0.0f, 0.0f, 0.36f};
      uint16_t c = wash(ink[p.tint], tints[i % 3]);
      // Each petal is a narrow fan of triangles, so its outer edge is a curve
      // and every edge stays hard.
      unsigned steps = 4;
      for (unsigned k = 0; k < steps; ++k) {
        float t0 = a0 + (a1 - a0) * float(k) / float(steps);
        float t1 = a0 + (a1 - a0) * float(k + 1) / float(steps);
        triangle(cx, cy,
                 cx + fcos(t0) * length, cy + fsin(t0) * length * 0.66f,
                 cx + fcos(t1) * length, cy + fsin(t1) * length * 0.66f, c);
      }
    }
    disc(cx, cy, 5 + breath * 4, wash(ink[2], 0.15f));
  }

 public:
  explicit Painting(uint32_t value = 17) : rng(value ? value : 1) {
    for (unsigned i = 0; i < wave.size(); ++i) wave[i] = std::sin(float(i) * (6.283185307f / 256.0f));
    regenerate();
  }
  void seed(uint32_t value) { rng = value ? value : 1; count = 0; regenerate(); }

  // The character follows the instrument. A shake rearranges it; a new
  // generation brings a new instrument and with it a new character.
  // The register the instrument is playing in, so pitch can be placed across
  // the whole frame whatever that register is.
  void setRegister(int low, int high) { registerLow = low; registerHigh = high; }
  // Force a particular character. The firmware does not use this; the host
  // preview does, to look at one of them on its own.
  void setCharacter(unsigned which) {
    character = which % characterCount;
    arrange();
  }

  // A new character, never the one already showing, and a new arrangement of
  // it. Called on a new generation and on a shake alike.
  void regenerate() {
    // Open on concentric rings; subsequent changes still select another character.
    const unsigned choice = random();
    character = count ? (character + 1 + choice % (characterCount - 1)) % characterCount
                      : Ring;
    arrange();
  }

  // A new arrangement of the character already showing.
  void arrange() {
    palette = count ? (palette + 1 + random() % 5) % 6 : random() % 6;
    ++count;
    phase = unit() * 40.0f;
    breath = 0;
    for (auto& p : parameters) p = unit();
    // Rill's daylight palettes: a coloured ground and three inks that sit on
    // it, in the register of a faded photograph rather than emitted light.
    static const Color palettes[6][4] = {
      {{232, 220, 192}, {196,  85,  63}, { 78, 138, 134}, {211, 160,  60}},  // shōwa afternoon
      {{201, 210, 206}, { 62,  90,  99}, {179,  91,  74}, {240, 230, 210}},  // rainy ginza
      {{143, 203, 232}, {226,  88,  75}, { 95, 163,  82}, {251, 240, 216}},  // park in spring
      {{234, 227, 210}, {126, 154, 107}, {192, 107,  78}, {110, 132, 148}},  // village morning
      {{240, 201, 160}, {212,  91,  60}, {107, 122,  58}, {122,  74,  70}},  // late sun
      {{ 44,  58,  74}, {232, 163,  61}, {201, 106, 106}, {143, 185, 168}},  // night market
    };
    groundColor = palettes[palette][0];
    for (unsigned i = 0; i < 3; ++i) ink[i] = palettes[palette][i + 1];
    ground = color(groundColor);
    evolutionRng = (rng ^ 0x85ebca6bu) | 1u;
    evolutionAt = 0;
    for (unsigned i = 0; i < goals.size(); ++i) evolving[i] = goals[i] = evolveUnit();

    markCount = 0; nextMark = 0;
    for (auto& m : marks) m = Mark{};
    pieces.fill(0);
    piecesFilled = piecesShown = 0;
    for (unsigned i = 0; i < pieceCount(); ++i) { float x, y; if (pieceCentre(i, x, y)) ++piecesShown; }
    heardLow = (registerLow + registerHigh) / 2 - 3;
    heardHigh = heardLow + 6;
    swell.fill(0); knock.fill(0); memory.fill(0);
    for (unsigned i = 0; i < moonCount; ++i) moonPhase[i] = moonGoal[i] = 0.1f + unit() * 0.8f;
    ringCount = 0; nextRing = 0;
    rings.fill(0);
    clearAt = 99;
    petalCount = 0;
    bloomTurn = unit();
    petalShape = range(0.1f, 0.9f);
    clear();
  }

  unsigned generation() const { return count; }
  unsigned visualFamily() const { return character; }
  const uint16_t* pixels() const { return frame.data(); }

  void render(float seconds, float audio, uint8_t note = 0, float weight = 0.6f) {
    seconds = std::max(0.0f, std::min(0.1f, seconds));
    phase += seconds;
    if (phase > 100000.0f) phase -= 100000.0f;
    breath += (std::max(0.0f, std::min(1.0f, audio)) - breath) * std::min(1.0f, seconds * 5);
    evolve(seconds);
    switch (character) {
      case Asanoha: case Uroko: case Kikko: renderKumiko(seconds, note, weight); break;
      case Ring: renderRing(seconds, note, weight); break;
      case Ripple: renderRipple(seconds, note, weight); break;
      case Moons: renderMoons(seconds, note, weight); break;
      case Bloom: renderBloom(seconds, note, weight); break;
      default: renderSnap(seconds, note, weight); break;
    }
  }
};
}
