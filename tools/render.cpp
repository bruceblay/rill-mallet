// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/Chime.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
// Audition: write a WAV of the sampled instrument playing its own score.
int main(int argc, char** argv) {
  if (argc < 3) return 1;
  unsigned seconds = std::strtoul(argv[2], nullptr, 10);
  uint32_t seed = argc > 3 ? uint32_t(std::strtoul(argv[3], nullptr, 10)) : 1u;
  int family = argc > 4 ? int(std::strtol(argv[4], nullptr, 10)) : -1;
  auto engine = std::unique_ptr<chime::Engine>(new chime::Engine(seed, family));
  std::vector<int16_t> pcm(size_t(seconds) * chime::rate);
  engine->render(pcm.data(), unsigned(pcm.size()));
  std::ofstream out(argv[1], std::ios::binary);
  uint32_t dataBytes = uint32_t(pcm.size() * 2), fileBytes = 36 + dataBytes;
  auto put32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  auto put16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
  out.write("RIFF", 4); put32(fileBytes); out.write("WAVEfmt ", 8);
  put32(16); put16(1); put16(1); put32(chime::rate); put32(chime::rate * 2); put16(2); put16(16);
  out.write("data", 4); put32(dataBytes);
  out.write(reinterpret_cast<const char*>(pcm.data()), dataBytes);
  return out ? 0 : 2;
}
