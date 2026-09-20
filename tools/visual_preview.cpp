// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/Mallet.h"
#include "../src/Resonance.h"
#include <cstdlib>
#include <fstream>
#include <memory>
// Preview one character, played by the engine that owns it: the visuals are
// driven by real strikes, so rendering them against a synthetic beat would
// show something the device never does.
int main(int argc, char** argv) {
  if (argc < 3) return 1;
  unsigned instrument = unsigned(std::strtoul(argv[2], nullptr, 10)) % resonance::Painting::characterCount;
  unsigned frames = argc > 3 ? unsigned(std::strtoul(argv[3], nullptr, 10)) : 120;
  uint32_t seed = argc > 4 ? uint32_t(std::strtoul(argv[4], nullptr, 10)) : 7u;
  auto engine = std::unique_ptr<mallet::Engine>(new mallet::Engine(seed, int(instrument)));
  auto painting = std::unique_ptr<resonance::Painting>(new resonance::Painting(seed));
  painting->setCharacter(instrument);
  painting->setRegister(engine->melodyBottom(), engine->melodyTop());
  std::array<int16_t, 512> block{};
  for (unsigned f = 0; f < frames; ++f) {
    uint8_t note = 0;
    float weight = 0.6f, energy = 0;
    // One display frame is a twelfth of a second of audio.
    for (unsigned b = 0; b < 32000 / 12 / 512; ++b) {
      engine->render(block.data(), block.size());
      for (auto s : block) energy += float(std::abs(int(s)));
      uint8_t struck = engine->drainOnset();
      if (struck) { note = struck; weight = engine->onsetWeight(); }
    }
    painting->render(1.0f / 12, energy / (512 * 5 * 8000.0f), note, weight);
  }
  std::ofstream out(argv[1], std::ios::binary);
  out << "P6\n240 135\n255\n";
  for (unsigned i = 0; i < 240 * 135; ++i) {
    uint16_t c = painting->pixels()[i];
    out.put(char(((c >> 11) & 31) * 255 / 31));
    out.put(char(((c >> 5) & 63) * 255 / 63));
    out.put(char((c & 31) * 255 / 31));
  }
  return out ? 0 : 2;
}
