// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/Resonance.h"
#include <cassert>
#include <iostream>
#include <memory>
uint32_t hash(const resonance::Painting& p) {
  uint32_t value = 2166136261u;
  for (unsigned i = 0; i < 240 * 135; ++i) value = (value ^ p.pixels()[i]) * 16777619u;
  return value;
}
int main() {
  for (uint32_t seed : {1u, 17u, 42u, 999u}) {
    resonance::Painting initial(seed);
    assert(initial.visualFamily() == resonance::Painting::Ring);
    initial.regenerate();
    assert(initial.visualFamily() != resonance::Painting::Ring);
    initial.seed(seed);
    assert(initial.visualFamily() == resonance::Painting::Ring);
  }
  // Character and instrument are drawn separately, so a run of generations
  // has to reach every character on its own.
  {
    auto sweep = std::unique_ptr<resonance::Painting>(new resonance::Painting(3));
    unsigned seen = 0;
    for (unsigned i = 0; i < 60; ++i) { seen |= 1u << sweep->visualFamily(); sweep->regenerate(); }
    assert(seen == (1u << resonance::Painting::characterCount) - 1);
  }
  auto a = std::unique_ptr<resonance::Painting>(new resonance::Painting(17));
  auto b = std::unique_ptr<resonance::Painting>(new resonance::Painting(17));
  a->render(0, 0); b->render(0, 0);
  assert(hash(*a) == hash(*b));

  for (unsigned character = 0; character < resonance::Painting::characterCount; ++character) {
    a->seed(17); b->seed(17);
    a->setCharacter(character); b->setCharacter(character);
    a->setRegister(60, 84); b->setRegister(60, 84);
    assert(a->visualFamily() == character);
    // Both, not just one: the first frame advances the slow-parameter clock,
    // so rendering a and not b is enough to put them out of step.
    a->render(0, 0); b->render(0, 0);
    auto start = hash(*a);
    // The same strikes must give the same picture, frame for frame.
    for (unsigned i = 0; i < 120; ++i) {
      uint8_t note = i % 7 == 0 ? uint8_t(62 + (i % 19)) : uint8_t(0);
      a->render(.083f, .4f, note, .7f);
      b->render(.083f, .4f, note, .7f);
      assert(hash(*a) == hash(*b));
    }
    // Every character does something with a phrase, and something different
    // with a different phrase.
    assert(hash(*a) != start);
    auto played = hash(*a);
    a->seed(17); a->setCharacter(character); a->setRegister(60, 84);
    for (unsigned i = 0; i < 120; ++i)
      a->render(.083f, .4f, i % 5 == 0 ? uint8_t(70 + (i % 11)) : uint8_t(0), .4f);
    assert(hash(*a) != played);
    // A shake draws a different character, never the one already showing.
    auto before = hash(*a);
    a->regenerate(); a->render(0, 0);
    assert(a->visualFamily() != character && hash(*a) != before);
  }
  std::cout << "Eight characters, deterministic seeds, per-strike response and frame bounds passed\n";
}
