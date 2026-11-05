#include "ui/Sessions.h"

namespace sk {

SessionHub& SessionHub::Instance() {
  static SessionHub hub;
  return hub;
}

Terminal& SessionHub::Ensure(int id) {
  auto pos = terms_.find(id);
  if (pos != terms_.end()) return *pos->second;
  std::unique_ptr<Terminal> term(new Terminal());
  Terminal& ref = *term;
  terms_[id] = std::move(term);
  return ref;
}

Terminal* SessionHub::Find(int id) {
  auto pos = terms_.find(id);
  if (pos == terms_.end()) return nullptr;
  return pos->second.get();
}

void SessionHub::Close(int id) {
  terms_.erase(id);
}

void SessionHub::Clear() {
  terms_.clear();
}

}
