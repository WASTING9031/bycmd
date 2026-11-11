#pragma once
// One guarded 32-bit instruction word in the game's r-x code. Text pages are written through
// /proc/<pid>/mem opened O_RDWR (root), the same path the Fog setter lock uses; the kernel keeps
// instruction-cache behavior must be verified on the target device. A word is only replaced
// when it holds the expected value; matching instructions alone do not prove build compatibility.
#include "../other/memory.hpp"
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace code_patch {
namespace detail { inline int fd = -1, fd_pid = -1; }
inline bool write_word(uint64_t address, uint32_t word) {
    using namespace detail;
    if (fd >= 0 && fd_pid != proc::pid) { ::close(fd); fd = -1; }
    if (fd < 0) {
        char path[64]; std::snprintf(path, sizeof path, "/proc/%d/mem", proc::pid);
        fd = ::open(path, O_RDWR | O_CLOEXEC); fd_pid = proc::pid;
        if (fd < 0) return false;
    }
    return ::pwrite(fd, &word, sizeof word, off_t(address)) == ssize_t(sizeof word);
}
// `from` -> `to` only if the live word is exactly `from`. True when the word is `to` afterwards.
inline bool swap(uint64_t address, uint32_t from, uint32_t to) {
    const uint32_t now = rpm<uint32_t>(address);
    if (now == to) return true;
    if (now != from) return false;
    return write_word(address, to) && rpm<uint32_t>(address) == to;
}
} // namespace code_patch
