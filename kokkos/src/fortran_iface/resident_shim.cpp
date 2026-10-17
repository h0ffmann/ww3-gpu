// kokkos/src/fortran_iface/resident_shim.cpp
// The resident spectrum of WFIP-0005 (units 1 and 2): one VA context per WW3 grid, the
// sync calls of resident_c.hpp that W3WAVE makes at its host statements of VA, and
// ww::resident::device_va() for the kernel shims (resident_state.hpp).
//
// The Fortran owns VA; this file only copies it whole, host to device or back, when the
// other side holds the newer bits. A copy moves bits and computes nothing. Every call
// comes from outside an OpenMP parallel region (the Fortran wrappers refuse one), so the
// contexts and counters need no lock. Views die in Kokkos' finalize hook.
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/resident_c.hpp"
#include "fortran_iface/timers.hpp"
#include "fortran_iface/ww_kokkos_c.hpp"
#include "ww_kokkos/resident_state.hpp"

#include <Kokkos_Core.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <new>

namespace ww::resident {
namespace {

struct Ctx {
  float* host = nullptr;
  int nspec = 0, nsealm = -1;
  DevVa d;
  Side side = Side::Host;
  size_t count() const { return static_cast<size_t>(nspec) * static_cast<size_t>(nsealm + 1); }
};

// Never destroyed, as every shim's map: the finalize hook and the printer run from atexit.
std::map<int, Ctx>& grids() {
  static auto* m = new std::map<int, Ctx>();
  return *m;
}

struct Counts {
  long long h2d = 0, d2h = 0, host_reads = 0, host_writes = 0, device_calls = 0;
  double h2d_bytes = 0.0, d2h_bytes = 0.0, spectrum_bytes = 0.0;
};
Counts g_n;
bool g_hooks = false;

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}
bool resident_diag() {
  static const bool on = env_is_one("WW_KOKKOS_VA_RESIDENT");
  return on;
}
bool count_diag() {
  static const bool on = env_is_one("WW_KOKKOS_VA_COUNT");
  return on;
}

void say(const char* what) { std::fprintf(stderr, "ww_kokkos: %s\n", what); }

void release() {
  for (auto& [igrid, c] : grids()) {
    if (c.side == Side::Device)
      std::fprintf(stderr, "ww_kokkos: VA of grid %d was still newer on the device at finalize\n",
                   igrid);
    c.d = DevVa();
  }
  grids().clear();
}

void report_at_exit() { ww_va_report(); }

void hooks() {
  if (g_hooks) return;
  g_hooks = true;
  if (Kokkos::is_initialized()) Kokkos::push_finalize_hook(release);
  if (count_diag()) std::atexit(report_at_exit);
}

/// The context of `igrid`, bound to va on the first call; nullptr (and a message) when a
/// later call passes another address or shape (ww3_multi re-points VA through W3SETW).
Ctx* bind(int igrid, float* va, int nspec, int nsealm) {
  if (va == nullptr || nspec <= 0 || nsealm < 0) {
    say("ww_va: a null VA or a negative extent");
    return nullptr;
  }
  hooks();
  Ctx& c = grids()[igrid];
  if (c.host == nullptr) {
    c.host = va;
    c.nspec = nspec;
    c.nsealm = nsealm;
    g_n.spectrum_bytes += static_cast<double>(c.count() * sizeof(float));
    return &c;
  }
  if (c.host != va || c.nspec != nspec || c.nsealm != nsealm) {
    std::fprintf(stderr,
                 "ww_kokkos: ww_va: grid %d's VA moved or changed shape (%p %d x 0:%d, bound "
                 "%p %d x 0:%d); the resident spectrum supports one VA per grid\n",
                 igrid, static_cast<void*>(va), nspec, nsealm, static_cast<void*>(c.host),
                 c.nspec, c.nsealm);
    return nullptr;
  }
  return &c;
}

HostVa host_view(const Ctx& c) { return HostVa(c.host, c.nspec, c.nsealm + 1); }

void allocate(Ctx& c) {
  if (c.d.data() != nullptr) return;
  c.d = DevVa(Kokkos::view_alloc(Kokkos::WithoutInitializing, "ww_va.d_va"), c.nspec,
              c.nsealm + 1);
}

void h2d(Ctx& c) {
  allocate(c);
  ww::timers::Region t("va.h2d");
  Kokkos::deep_copy(c.d, host_view(c));
  Kokkos::fence("ww_va.h2d");
  ++g_n.h2d;
  g_n.h2d_bytes += static_cast<double>(c.count() * sizeof(float));
  c.side = Side::Both;
}

void d2h(Ctx& c) {
  ww::timers::Region t("va.d2h");
  Kokkos::deep_copy(host_view(c), c.d);
  Kokkos::fence("ww_va.d2h");
  ++g_n.d2h;
  g_n.d2h_bytes += static_cast<double>(c.count() * sizeof(float));
  c.side = Side::Both;
}

/// The host side of the protocol: the newer bits onto the Fortran array, then HOST if the
/// statement writes it.
int host_sync(int igrid, float* va, int nspec, int nsealm, bool write) {
  ww::timers::Region t("va.host.call");
  Ctx* c = bind(igrid, va, nspec, nsealm);
  if (c == nullptr) return WW_KOKKOS_ERR_BAD_SHAPE;
  if (resident_diag()) {
    if (!Kokkos::is_initialized()) return WW_KOKKOS_ERR_NOT_INITIALISED;
    if (c->side == Side::Host) h2d(*c);
    d2h(*c);
  } else if (c->side == Side::Device) {
    d2h(*c);
  }
  if (write) c->side = Side::Host;
  return WW_KOKKOS_OK;
}

template <class F>
int guarded(F&& f) {
  try {
    return f();
  } catch (const std::bad_alloc& e) {
    say(e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (const std::exception& e) {
    say(e.what());
    return WW_KOKKOS_ERR_KERNEL;
  } catch (...) {
    say("ww_va: unknown exception");
    return WW_KOKKOS_ERR_KERNEL;
  }
}

}  // namespace

int device_va(int igrid, float* va, int nspec, int nsealm, DevVa& out) {
  return guarded([&]() {
    if (!Kokkos::is_initialized()) return static_cast<int>(WW_KOKKOS_ERR_NOT_INITIALISED);
    Ctx* c = bind(igrid, va, nspec, nsealm);
    if (c == nullptr) return static_cast<int>(WW_KOKKOS_ERR_BAD_SHAPE);
    ++g_n.device_calls;
    if (c->side == Side::Host) h2d(*c);
    out = c->d;
    return static_cast<int>(WW_KOKKOS_OK);
  });
}

void device_written(int igrid) {
  const auto it = grids().find(igrid);
  if (it != grids().end() && it->second.d.data() != nullptr) it->second.side = Side::Device;
}

}  // namespace ww::resident

using namespace ww::resident;

extern "C" {

int ww_va_host_read(int igrid, float* va, int nspec, int nsealm) {
  ++g_n.host_reads;
  return guarded([&]() { return host_sync(igrid, va, nspec, nsealm, false); });
}

int ww_va_host_write(int igrid, float* va, int nspec, int nsealm) {
  ++g_n.host_writes;
  return guarded([&]() { return host_sync(igrid, va, nspec, nsealm, true); });
}

int ww_va_device(int igrid, int write) {
  return guarded([&]() {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !Kokkos::is_initialized())
      return static_cast<int>(WW_KOKKOS_ERR_NOT_INITIALISED);
    Ctx& c = it->second;
    ++g_n.device_calls;
    if (c.side == Side::Host) h2d(c);
    if (write != 0) c.side = Side::Device;
    return static_cast<int>(WW_KOKKOS_OK);
  });
}

void ww_va_report(void) {
  constexpr double mb = 1.0e6;
  std::fprintf(stderr,
               "ww_kokkos: Kokkos VA: resident on %s; H2D %lld (%.2f MB), D2H %lld (%.2f MB); "
               "host reads %lld, host writes %lld, device syncs %lld; spectrum %.2f MB%s\n",
               Kokkos::DefaultExecutionSpace::name(), g_n.h2d, g_n.h2d_bytes / mb, g_n.d2h,
               g_n.d2h_bytes / mb, g_n.host_reads, g_n.host_writes, g_n.device_calls,
               g_n.spectrum_bytes / mb,
               resident_diag() ? " (WW_KOKKOS_VA_RESIDENT=1: both copies at every host call)" : "");
  // One count per line, "<name>: <n>", the form scripts/ww3-gpu.sh prints.
  std::fprintf(stderr, "ww_kokkos: VA host-to-device copies: %lld\n", g_n.h2d);
  std::fprintf(stderr, "ww_kokkos: VA device-to-host copies: %lld\n", g_n.d2h);
  std::fprintf(stderr, "ww_kokkos: VA host sync calls: %lld\n", g_n.host_reads + g_n.host_writes);
  std::fprintf(stderr, "ww_kokkos: VA device sync calls: %lld\n", g_n.device_calls);
}

}  // extern "C"
