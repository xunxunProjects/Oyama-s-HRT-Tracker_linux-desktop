#pragma once

namespace oyama {

// Best-effort hardening for processes that handle vault secrets.
// Disables core dumps and, on Linux, marks the process non-dumpable.
void harden_process_for_secrets();

} // namespace oyama
