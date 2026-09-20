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
// ragged field or the sparse page and neither of them is its.
//
// Shared by firmware and the host preview tools.
namespace resonance {
class Painting {
 public:
  static constexpr unsigned width = 240, height = 135;
  static constexpr unsigned characterCount = 7;
  enum Character : unsigned { Split = 0, Spark, Ring, Trace, Stack, Bloom, Snap };

 private:
  struct Color { float r, g, b; };
  struct Mark { float x, y, size, age; unsigned tint, shape; };
  struct Band { float span, offset, thickness, age; unsigned tint; };
  // Split: a flat region of the page, and how far its newest edge has slid
  // into place.
  struct Region { float x0, y0, x1, y1, open; unsigned tint; };
  // Bloom: one petal, and how far it has grown out of the centre.
  struct Petal { float angle, length, width, grown; unsigned tint; };
  // Trace: one corner of the drawn line.
  struct Corner { float x, y, drawn; unsigned tint; };

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
  std::array<Band, 16> bands{};
  unsigned bandCount = 0;
  std::array<Region, 22> regions{};
  unsigned regionCount = 0;
  std::array<Petal, 20> petals{};
  unsigned petalCount = 0;
  std::array<Corner, 26> corners{};
  unsigned cornerCount = 0;
  std::array<float, 12> rings{};
  unsigned ringCount = 0, nextRing = 0;
  float clearAt = 0, gutter = 2, bloomTurn = 0, petalShape = 0;
  unsigned traceRun = 0;
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

  // Small precise marks on an empty page, one per strike, accumulating until
  // the page is full and a new one is started.
  void renderSpark(float dt, uint8_t note, float weight) {
    if (note) {
      Mark& m = marks[nextMark];
      nextMark = (nextMark + 1) % marks.size();
      if (markCount < marks.size()) ++markCount;
      m.x = 16 + place(note) * 208;
      // Down the page as they arrive, wrapped, so a page fills evenly instead
      // of piling wherever the melody happens to sit.
      m.y = 14 + float((markCount * 37) % 101) + range(-5.0f, 5.0f);
      if (m.y > 121) m.y -= 100;
      m.size = 3.4f + weight * 6.0f;
      m.tint = (markCount + unsigned(place(note) * 3.0f)) % 3;
      m.shape = random() % 3;
      m.age = 0;
    }
    clearAt -= dt;
    if (clearAt <= 0) { clearAt = range(14.0f, 22.0f); markCount = 0; nextMark = 0; }
    clear();
    // Echoes first, then the marks: a ring punches back to the ground, and
    // drawn afterwards it would take a bite out of whatever it overlapped.
    for (unsigned i = 0; i < markCount; ++i) {
      const Mark& m = marks[i];
      if (i + 3 >= markCount) ring(m.x, m.y, m.size * 2.6f, 1.6f, wash(ink[(m.tint + 1) % 3], 0.35f));
    }
    for (unsigned i = 0; i < markCount; ++i) {
      const Mark& m = marks[i];
      uint16_t c = wash(ink[m.tint], 0.0f);
      if (m.shape == 0) rect(m.x - m.size, m.y - m.size, m.x + m.size, m.y + m.size, c);
      else if (m.shape == 1) disc(m.x, m.y, m.size, c);
      else {
        // A cross, which at this size is two bars and reads as a star.
        rect(m.x - m.size * 1.6f, m.y - m.size * 0.4f, m.x + m.size * 1.6f, m.y + m.size * 0.4f, c);
        rect(m.x - m.size * 0.4f, m.y - m.size * 1.6f, m.x + m.size * 0.4f, m.y + m.size * 1.6f, c);
      }
    }
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

  // Each note lays a band across the page and pushes the older ones up. Time
  // reads bottom to top, pitch reads as width.
  void renderStack(float dt, uint8_t note, float weight) {
    if (note) {
      for (unsigned i = bands.size() - 1; i > 0; --i) bands[i] = bands[i - 1];
      Band& b = bands[0];
      b.span = 0.25f + place(note) * 0.7f;
      b.offset = (evolving[0] - 0.5f) * 0.4f;
      b.thickness = 5.0f + weight * 9.0f;
      b.tint = unsigned(place(note) * 3.0f) % 3;
      b.age = 0;
      if (bandCount < bands.size()) ++bandCount;
    }
    clear();
    float y = float(height) + 4;
    for (unsigned i = 0; i < bandCount; ++i) {
      Band& b = bands[i];
      b.age += dt;
      float w = float(width) * b.span;
      float cx = float(width) * (0.5f + b.offset);
      float rise = std::min(1.0f, b.age * 2.2f);
      float h = b.thickness * rise;
      y -= h + 2;
      if (y < -20) break;
      rect(cx - w * 0.5f, y, cx + w * 0.5f, y + h, wash(ink[b.tint], i > 5 ? 0.45f : 0.0f));
    }
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

  // The page is cut in two by every strike, and each new region takes its own
  // flat colour: a composition built note by note rather than animated. The
  // newest edge slides into place, so the only movement on screen is movement
  // a note caused.
  void renderSplit(float dt, uint8_t note, float weight) {
    if (note && regionCount < regions.size() - 1) {
      // Cut the largest region, which keeps the page subdividing evenly
      // instead of shredding one corner.
      unsigned pick = 0;
      float widest = 0;
      for (unsigned i = 0; i < regionCount; ++i) {
        float area = (regions[i].x1 - regions[i].x0) * (regions[i].y1 - regions[i].y0);
        if (area > widest) { widest = area; pick = i; }
      }
      Region& parent = regions[pick];
      float w = parent.x1 - parent.x0, h = parent.y1 - parent.y0;
      if (w > 14 && h > 12) {
        float at = 0.30f + place(note) * 0.40f;
        Region child = parent;
        if (w * 0.62f > h) {
          float cut = parent.x0 + w * at;
          child.x0 = cut; parent.x1 = cut;
        } else {
          float cut = parent.y0 + h * at;
          child.y0 = cut; parent.y1 = cut;
        }
        child.tint = (parent.tint + 1 + unsigned(weight * 2.0f)) % 3;
        child.open = 0;
        parent.open = 1;
        regions[regionCount++] = child;
      }
    }
    if (regionCount >= regions.size() - 1) {
      clearAt -= dt;
      if (clearAt <= 0) { startPage(); }
    } else {
      clearAt = 2.5f;
    }
    clear();
    for (unsigned i = 0; i < regionCount; ++i) {
      Region& r = regions[i];
      r.open = std::min(1.0f, r.open + dt * 5.0f);
      float w = (r.x1 - r.x0) * r.open, h = (r.y1 - r.y0) * r.open;
      float cx = (r.x0 + r.x1) * 0.5f, cy = (r.y0 + r.y1) * 0.5f;
      static const float tints[3] = {0.0f, 0.34f, 0.62f};
      rect(cx - w * 0.5f + gutter, cy - h * 0.5f + gutter,
           cx + w * 0.5f - gutter, cy + h * 0.5f - gutter,
           wash(ink[r.tint], tints[(i + r.tint) % 3]));
    }
  }
  void startPage() {
    regionCount = 1;
    regions[0] = Region{0, 0, float(width), float(height), 1, unsigned(random() % 3)};
    clearAt = 2.5f;
    gutter = 1.0f + unit() * 3.0f;
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

  // A line that turns a corner on every strike and is drawn to the new corner
  // over the next fraction of a second: the page fills the way a plotter
  // fills one, and stops dead when the music does.
  void renderTrace(float dt, uint8_t note, float weight) {
    if (note) {
      if (cornerCount >= corners.size() || corners[cornerCount ? cornerCount - 1 : 0].x > float(width) - 12) {
        cornerCount = 0;
        ++traceRun;
      }
      Corner& c = corners[cornerCount++];
      float step = 14.0f + weight * 18.0f + evolving[0] * 14.0f;
      c.x = cornerCount == 1 ? 8.0f : corners[cornerCount - 2].x + step;
      c.y = 14 + (1.0f - place(note)) * 106;
      c.tint = (traceRun + cornerCount) % 3;
      c.drawn = 0;
    }
    clear();
    for (unsigned i = 1; i < cornerCount; ++i) {
      Corner& c = corners[i];
      c.drawn = std::min(1.0f, c.drawn + dt * 6.0f);
      const Corner& from = corners[i - 1];
      float x = from.x + (c.x - from.x) * c.drawn, y = from.y + (c.y - from.y) * c.drawn;
      uint16_t colour = wash(ink[c.tint], 0.0f);
      // Three passes across the direction of travel: one pixel of line
      // disappears on this panel, and a solid bar is not a drawing.
      float dx = c.x - from.x, dy = c.y - from.y;
      float length = std::sqrt(dx * dx + dy * dy);
      float ox = length > 0.001f ? -dy / length : 0, oy = length > 0.001f ? dx / length : 0;
      for (int k = -2; k <= 2; ++k)
        line(from.x + ox * float(k), from.y + oy * float(k), x + ox * float(k), y + oy * float(k), colour);
      disc(from.x, from.y, 4.0f, wash(ink[(c.tint + 1) % 3], 0.0f));
      if (c.drawn >= 1.0f) disc(c.x, c.y, 4.0f, wash(ink[(c.tint + 1) % 3], 0.0f));
    }
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
    character = count ? (character + 1 + random() % (characterCount - 1)) % characterCount
                      : random() % characterCount;
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
    bandCount = 0;
    for (auto& b : bands) b = Band{};
    ringCount = 0; nextRing = 0;
    rings.fill(0);
    clearAt = range(14.0f, 22.0f);
    startPage();
    petalCount = 0;
    bloomTurn = unit();
    petalShape = range(0.1f, 0.9f);
    cornerCount = 0;
    traceRun = unsigned(random() % 3);
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
      case Split: renderSplit(seconds, note, weight); break;
      case Spark: renderSpark(seconds, note, weight); break;
      case Ring: renderRing(seconds, note, weight); break;
      case Trace: renderTrace(seconds, note, weight); break;
      case Stack: renderStack(seconds, note, weight); break;
      case Bloom: renderBloom(seconds, note, weight); break;
      default: renderSnap(seconds, note, weight); break;
    }
  }
};
}
