/*
 * OMKernel.hpp — C++ kernel-object layer for the OpenMac darwin-shim.
 *
 * Darwin-style RAII wrappers (IOMobileReference / OSObject-flavoured) over
 * the libopenmach C shim. Original code; contains nothing derived from
 * Apple's darwin-xnu sources — only public API shapes are reproduced.
 *
 * Licensing: proprietary (see ../../LICENSE).
 */
#ifndef _OPENMAC_CPP_OMKERNEL_HPP
#define _OPENMAC_CPP_OMKERNEL_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>
#include <stdexcept>
#include <memory>

extern "C" {
#include "../mach.h"
}

namespace omac {

/* ---- kern_return translation ------------------------------------------- */
class KernelError : public std::runtime_error {
public:
    explicit KernelError(kern_return_t kr, const char *what)
        : std::runtime_error(std::string(what) + " (kern_return=" +
                             std::to_string(static_cast<int>(kr)) + ")"),
          kr_(kr) {}
    kern_return_t kern_return() const noexcept { return kr_; }
private:
    kern_return_t kr_;
};

inline void throwIfFailed(kern_return_t kr, const char *what) {
    if (kr != KERN_SUCCESS) throw KernelError(kr, what);
}

/* ---- SystemVersion / ProcessInfo (see OMKernel.cpp) -------------------- */
struct SystemVersion {
    std::string productName    = "OpenMac";
    std::string productVersion = "1.0";
    std::string buildVersion   = "1O101";
    static SystemVersion current();
};

struct ProcessInfo {
    pid_t pid = 0;
    pid_t ppid = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    std::array<char, 512> executablePath{};
    static ProcessInfo self();
};

/* ---- MachPort: moveable RAII handle ------------------------------------ */
class MachPort {
public:
    /* Prefer MachPort::allocate(); default ctor also allocates a port. */
    MachPort() {
        mach_port_t n = 0;
        kern_return_t kr = omach_port_allocate(&n);
        throwIfFailed(kr, "omach_port_allocate");
        name_ = n;
    }
    static MachPort allocate() {
        mach_port_t n = 0;
        kern_return_t kr = omach_port_allocate(&n);
        throwIfFailed(kr, "omach_port_allocate");
        return MachPort(n);
    }
    ~MachPort() { reset(); }

    MachPort(const MachPort&) = delete;
    MachPort& operator=(const MachPort&) = delete;

    MachPort(MachPort&& o) noexcept : name_(o.name_) { o.name_ = 0; }
    MachPort& operator=(MachPort&& o) noexcept {
        if (this != &o) { reset(); name_ = o.name_; o.name_ = 0; }
        return *this;
    }

    void connect(const std::string &targetPath) {
        kern_return_t kr = omach_port_connect(name_, targetPath.c_str());
        throwIfFailed(kr, "omach_port_connect");
    }
    const char* path() const { return omach_port_path(name_); }

    void send(const void *buf, size_t len) {
        throwIfFailed(omach_msg_send(name_, buf, len), "omach_msg_send");
    }
    void send(const std::string &s) { send(s.data(), s.size()); }

    size_t recv(void *buf, size_t maxlen) {
        size_t out = 0;
        throwIfFailed(omach_msg_recv(name_, buf, maxlen, &out),
                      "omach_msg_recv");
        return out;
    }
    std::string recvString(size_t maxlen = 65536) {
        std::string s; s.resize(maxlen);
        size_t n = recv(&s[0], maxlen);
        s.resize(n);
        return s;
    }

    mach_port_t name() const noexcept { return name_; }
    void reset() noexcept {
        if (name_) { omach_port_deallocate(name_); name_ = 0; }
    }

private:
    explicit MachPort(mach_port_t n) noexcept : name_(n) {}
    mach_port_t name_ = 0;
};

/* ---- HostInfo / Host (host_self + host_info) ---------------------------- */
struct HostInfo {
    uint32_t maxCpus = 0;
    uint32_t availCpus = 0;
    uint64_t memorySize = 0;
};

class Host {
public:
    Host() {
        throwIfFailed(omach_host_self(&port_), "omach_host_self");
    }
    HostInfo info() const {
        ::host_basic_info hi{};
        throwIfFailed(omach_host_info(port_, HOST_BASIC_INFO, &hi),
                      "omach_host_info");
        HostInfo out;
        out.maxCpus = hi.max_cpus;
        out.availCpus = hi.avail_cpus;
        out.memorySize = hi.memory_size;
        return out;
    }
    mach_port_t port() const noexcept { return port_; }
private:
    mach_port_t port_ = 0;
};

/* ---- VMRegion: mmap-backed region with vm_allocate semantics ------------ */
class VMRegion {
public:
    VMRegion() = default;
    static VMRegion allocate(uint64_t size,
                             vm_prot_t prot = VM_PROT_DEFAULT) {
        VMRegion r;
        r.size_ = size;
        uint64_t addr = 0;
        throwIfFailed(omach_vm_allocate(0, &addr, size, prot),
                      "omach_vm_allocate");
        r.addr_ = reinterpret_cast<void*>(static_cast<uintptr_t>(addr));
        return r;
    }
    ~VMRegion() { reset(); }
    VMRegion(const VMRegion&) = delete;
    VMRegion& operator=(const VMRegion&) = delete;
    VMRegion(VMRegion&& o) noexcept : addr_(o.addr_), size_(o.size_) {
        o.addr_ = nullptr; o.size_ = 0;
    }
    VMRegion& operator=(VMRegion&& o) noexcept {
        if (this != &o) { reset(); addr_ = o.addr_; size_ = o.size_;
                          o.addr_ = nullptr; o.size_ = 0; }
        return *this;
    }
    void* data() const noexcept { return addr_; }
    uint64_t size() const noexcept { return size_; }
    void reset() noexcept {
        if (addr_) {
            omach_vm_deallocate(0,
                reinterpret_cast<uintptr_t>(addr_), size_);
            addr_ = nullptr; size_ = 0;
        }
    }
private:
    void* addr_ = nullptr;
    uint64_t size_ = 0;
};

} /* namespace omac */

#endif /* _OPENMAC_CPP_OMKERNEL_HPP */
