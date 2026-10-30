// SPDX-License-Identifier: GPL-2.0-or-later
// Test-only Ivy peer: one complete message per stdin/stdout line.
#include <Ivy/ivy.hpp>
#include <Ivy/ivy_thread.hpp>
#include <iostream>
#include <syncstream>

int main(int argc, char **argv)
{
  if (argc != 2) return 2;
  auto bus = ivy::Bus::create("link-test-peer", "TEST_PEER_READY");
  if (!bus) return 1;
  auto subscription = bus->bind_raw(
    [](IvyClientPtr, std::span<const std::string_view> fields) {
      if (fields.size() == 1) std::osyncstream(std::cout) << fields[0] << std::endl;
    }, ivy::runtime_regexp("^(.*)$"));
  if (!subscription || !bus->start(argv[1])) return 1;
  auto loop = ivy::LoopThread::create(*bus);
  if (!loop) return 1;
  std::cout << "TEST_BUS_READY" << std::endl;
  for (std::string line; std::getline(std::cin, line); ) {
    if (line == "!quit") break;
    if (!bus->send(line)) return 1;
  }
  return 0;
}
