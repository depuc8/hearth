#include "version.hpp"

namespace hearth {

std::string_view version() noexcept {
  return HEARTH_VERSION;
}

} // namespace hearth
