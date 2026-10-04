#include "../CatS3Controller/BleProtocol.h"
#include <cassert>
#include <iostream>
int main() {
  const char *token="0123456789abcdef";
  auto good=catble::parse("0123456789abcdef|A:nod",token);
  assert(good.kind==catble::Kind::Action&&!strcmp(good.value,"nod"));
  assert(catble::parse("0123456789abcdef|M:happy",token).kind==catble::Kind::Mood);
  assert(catble::parse("0123456789abcdef|A:arm",token).kind==catble::Kind::Invalid);
  assert(catble::parse("bad|A:nod",token).kind==catble::Kind::Invalid);
  assert(catble::parse("0123456789abcdef|A:nod","").kind==catble::Kind::Invalid);
  assert(catble::parse("0123456789abcdef|A:stop",token).kind==catble::Kind::Action);
  std::cout<<"PASS: BLE token, action/mood whitelist, arm rejection\n";
}
