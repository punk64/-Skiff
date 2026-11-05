#pragma once

#include <map>
#include <memory>
#include "ui/Terminal.h"

namespace sk {

class SessionHub {
public:
  static SessionHub& Instance();

  Terminal& Ensure(int id);
  Terminal* Find(int id);
  void Close(int id);
  void Clear();

private:
  SessionHub() = default;
  std::map<int, std::unique_ptr<Terminal>> terms_;
};

}
