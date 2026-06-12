/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/emulator.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <thread>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "xenia/cpu/function.h"
#include "xenia/cpu/ppc/ppc_opcode_info.h"
#include "xenia/base/string_buffer.h"

#include "config.h"
#include "third_party/fmt/include/fmt/format.h"
#include "xenia/apu/audio_system.h"
#include "xenia/base/assert.h"
#include "xenia/base/byte_stream.h"
#include "xenia/base/clock.h"
#include "xenia/base/cvar.h"
#include "xenia/base/debugging.h"
#include "xenia/base/exception_handler.h"
#include "xenia/base/literals.h"
#include "xenia/base/logging.h"
#include "xenia/base/mapped_memory.h"
#include "xenia/base/platform.h"
#include "xenia/base/string.h"
#include "xenia/cpu/backend/code_cache.h"
#include "xenia/cpu/backend/null_backend.h"
#include "xenia/cpu/cpu_flags.h"
#include "xenia/cpu/thread_state.h"
#include "xenia/gpu/graphics_system.h"
#include "xenia/hid/input_driver.h"
#include "xenia/hid/input_system.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/user_module.h"
#include "xenia/kernel/util/gameinfo_utils.h"
#include "xenia/kernel/util/xdbf_utils.h"
#include "xenia/kernel/xam/xam_module.h"
#include "xenia/kernel/xbdm/xbdm_module.h"
#include "xenia/kernel/xboxkrnl/xboxkrnl_module.h"
#include "xenia/memory.h"
#include "xenia/ui/imgui_dialog.h"
#include "xenia/ui/imgui_drawer.h"
#include "xenia/ui/window.h"
#include "xenia/ui/windowed_app_context.h"
#include "xenia/vfs/devices/disc_image_device.h"
#include "xenia/vfs/devices/host_path_device.h"
#include "xenia/vfs/devices/null_device.h"
#include "xenia/vfs/devices/stfs_container_device.h"
#include "xenia/vfs/virtual_file_system.h"

#if XE_ARCH_AMD64
#include "xenia/cpu/backend/x64/x64_backend.h"
#endif  // XE_ARCH_AMD64
#if defined(__aarch64__)
#include "xenia/cpu/backend/a64/a64_backend.h"
#include "xenia/cpu/backend/arm64/arm64_backend.h"
#endif

DECLARE_int32(user_language);

DEFINE_double(time_scalar, 1.0,
              "Scalar used to speed or slow time (1x, 2x, 1/2x, etc).",
              "General");
DEFINE_string(
    launch_module, "",
    "Executable to launch from the .iso or the package instead of default.xex "
    "or the module specified by the game. Leave blank to launch the default "
    "module.",
    "General");

#if XE_ARCH_ARM64
extern "C" uint32_t g_arm64_last_fn;
extern "C" uint32_t g_arm64_last_fn_prev;
extern "C" uint32_t g_arm64_ci_target;
extern "C" uint32_t g_arm64_ci_saved;
extern "C" uint32_t g_arm64_ci_matches;
extern "C" uint32_t g_arm64_ci_misses;
extern "C" uint32_t g_arm64_fn_ring[16];
extern "C" uint32_t g_arm64_fn_ring_pos;
extern "C" uint32_t g_store_watch_lo;
extern "C" uint32_t g_store_watch_hi;
struct XeStoreWatchEntry {
  uint32_t addr;
  uint32_t value;
  uint32_t fn;
  uint32_t seq;
};
extern "C" XeStoreWatchEntry g_store_watch_ring[128];
extern "C" uint32_t g_store_watch_pos;
extern "C" uint32_t g_fa88_caller_ring[8];
extern "C" uint32_t g_fa88_r3_ring[8];
extern "C" uint32_t g_fa88_r4_ring[8];
extern "C" uint32_t g_fa88_pos;
extern "C" uint32_t g_fbc8_r4, g_fbc8_r25, g_fbc8_r28, g_fbc8_r31;
extern "C" uint32_t g_fbc8_storeval, g_fa88_local_ops;
extern "C" uint32_t g_fbc8_xregs[7];
#endif

namespace xe {

using namespace xe::literals;

#if XE_PLATFORM_ANDROID
// DIAGNOSTIC: signal-based PC sampler. The target thread (spinning in JIT code)
// receives the signal; the handler records its host PC, which the watchdog maps
// back to a guest function/address.
static constexpr int kSampleSignal = SIGPROF;
static std::atomic<uint64_t> g_sampled_pc{0};
static std::atomic<uint64_t> g_sampled_lr{0};
static std::atomic<uint64_t> g_sampled_frames[6];
static std::atomic<int> g_sampled_nframes{0};
static std::atomic<int> g_sample_ready{0};
static void XeSampleSignalHandler(int, siginfo_t*, void* ucontext) {
  auto* uc = reinterpret_cast<ucontext_t*>(ucontext);
  uint64_t pc = 0, lr = 0;
  int n = 0;
#if XE_ARCH_ARM64
  pc = static_cast<uint64_t>(uc->uc_mcontext.pc);
  lr = static_cast<uint64_t>(uc->uc_mcontext.regs[30]);
  // NOTE: do NOT walk the frame-pointer chain here — dereferencing a stale x29
  // faults inside the handler, and since the access-violation handler doesn't
  // resolve it the faulting load retries forever (a self-inflicted fault
  // storm). pc + lr are read from the captured context and are always safe.
#elif XE_ARCH_AMD64
  pc = static_cast<uint64_t>(uc->uc_mcontext.gregs[REG_RIP]);
#endif
  g_sampled_pc.store(pc, std::memory_order_relaxed);
  g_sampled_lr.store(lr, std::memory_order_relaxed);
  g_sampled_nframes.store(n, std::memory_order_relaxed);
  g_sample_ready.store(1, std::memory_order_release);
}
#endif

Emulator::GameConfigLoadCallback::GameConfigLoadCallback(Emulator& emulator)
    : emulator_(emulator) {
  emulator_.AddGameConfigLoadCallback(this);
}

Emulator::GameConfigLoadCallback::~GameConfigLoadCallback() {
  emulator_.RemoveGameConfigLoadCallback(this);
}

Emulator::Emulator(const std::filesystem::path& command_line,
                   const std::filesystem::path& storage_root,
                   const std::filesystem::path& content_root,
                   const std::filesystem::path& cache_root)
    : on_launch(),
      on_terminate(),
      on_exit(),
      command_line_(command_line),
      storage_root_(storage_root),
      content_root_(content_root),
      cache_root_(cache_root),
      title_name_(),
      title_version_(),
      display_window_(nullptr),
      memory_(),
      audio_system_(),
      graphics_system_(),
      input_system_(),
      export_resolver_(),
      file_system_(),
      kernel_state_(),
      main_thread_(),
      title_id_(std::nullopt),
      paused_(false),
      restoring_(false),
      restore_fence_() {}

Emulator::~Emulator() {
  // Note that we delete things in the reverse order they were initialized.

  // Give the systems time to shutdown before we delete them.
  if (graphics_system_) {
    graphics_system_->Shutdown();
  }
  if (audio_system_) {
    audio_system_->Shutdown();
  }

  input_system_.reset();
  graphics_system_.reset();
  audio_system_.reset();

  kernel_state_.reset();
  file_system_.reset();

  processor_.reset();

  export_resolver_.reset();

  ExceptionHandler::Uninstall(Emulator::ExceptionCallbackThunk, this);
}

X_STATUS Emulator::Setup(
    ui::Window* display_window, ui::ImGuiDrawer* imgui_drawer,
    bool require_cpu_backend,
    std::function<std::unique_ptr<apu::AudioSystem>(cpu::Processor*)>
        audio_system_factory,
    std::function<std::unique_ptr<gpu::GraphicsSystem>()>
        graphics_system_factory,
    std::function<std::vector<std::unique_ptr<hid::InputDriver>>(ui::Window*)>
        input_driver_factory) {
  X_STATUS result = X_STATUS_UNSUCCESSFUL;

  display_window_ = display_window;
  imgui_drawer_ = imgui_drawer;

  // Initialize clock.
  // 360 uses a 50MHz clock.
  Clock::set_guest_tick_frequency(50000000);
  // We could reset this with save state data/constant value to help replays.
  Clock::set_guest_system_time_base(Clock::QueryHostSystemTime());
  // This can be adjusted dynamically, as well.
  Clock::set_guest_time_scalar(cvars::time_scalar);

  // Before we can set thread affinity we must enable the process to use all
  // logical processors.
  xe::threading::EnableAffinityConfiguration();

  // Create memory system first, as it is required for other systems.
  memory_ = std::make_unique<Memory>();
  if (!memory_->Initialize()) {
    return false;
  }

  // Shared export resolver used to attach and query for HLE exports.
  export_resolver_ = std::make_unique<xe::cpu::ExportResolver>();

  std::unique_ptr<xe::cpu::backend::Backend> backend;
#if XE_ARCH_AMD64
  if (cvars::cpu == "x64") {
    backend.reset(new xe::cpu::backend::x64::X64Backend());
  }
#endif  // XE_ARCH
  if (cvars::cpu == "any") {
    if (!backend) {
#if XE_ARCH_AMD64
      backend.reset(new xe::cpu::backend::x64::X64Backend());
#endif  // XE_ARCH
    }
  }
  // AArch64 JIT backend — full HIR implementation using Dolphin's Arm64Emitter.
  // Preferred over a64 (stub) on Android arm64-v8a.
#if defined(__aarch64__)
  if (!backend) {
    backend.reset(new xe::cpu::backend::arm64::ARM64Backend());
  }
  // Fallback to stub a64 backend if arm64 fails to initialise.
  if (!backend) {
    backend.reset(new xe::cpu::backend::a64::A64Backend());
  }
#endif
  if (!backend && !require_cpu_backend) {
    backend.reset(new xe::cpu::backend::NullBackend());
  }

  // Initialize the CPU.
  processor_ = std::make_unique<xe::cpu::Processor>(memory_.get(),
                                                    export_resolver_.get());
  if (!processor_->Setup(std::move(backend))) {
    return X_STATUS_UNSUCCESSFUL;
  }

  // Initialize the APU.
  if (audio_system_factory) {
    audio_system_ = audio_system_factory(processor_.get());
    if (!audio_system_) {
      return X_STATUS_NOT_IMPLEMENTED;
    }
  }

  // Initialize the GPU.
  graphics_system_ = graphics_system_factory();
  if (!graphics_system_) {
    return X_STATUS_NOT_IMPLEMENTED;
  }

  // Initialize the HID.
  input_system_ = std::make_unique<xe::hid::InputSystem>(display_window_);
  if (!input_system_) {
    return X_STATUS_NOT_IMPLEMENTED;
  }
  if (input_driver_factory) {
    auto input_drivers = input_driver_factory(display_window_);
    for (size_t i = 0; i < input_drivers.size(); ++i) {
      auto& input_driver = input_drivers[i];
      input_driver->set_is_active_callback(
          []() -> bool { return !xe::kernel::xam::xeXamIsUIActive(); });
      input_system_->AddDriver(std::move(input_driver));
    }
  }

  result = input_system_->Setup();
  if (result) {
    return result;
  }

  // Bring up the virtual filesystem used by the kernel.
  file_system_ = std::make_unique<xe::vfs::VirtualFileSystem>();

  // Shared kernel state.
  kernel_state_ = std::make_unique<xe::kernel::KernelState>(this);

  // Setup the core components.
  result = graphics_system_->Setup(
      processor_.get(), kernel_state_.get(),
      display_window_ ? &display_window_->app_context() : nullptr,
      display_window_ != nullptr);
  if (result) {
    return result;
  }

  if (audio_system_) {
    result = audio_system_->Setup(kernel_state_.get());
    if (result) {
      return result;
    }
  }

#define LOAD_KERNEL_MODULE(t) \
  static_cast<void>(kernel_state_->LoadKernelModule<kernel::t>())
  // HLE kernel modules.
  LOAD_KERNEL_MODULE(xboxkrnl::XboxkrnlModule);
  LOAD_KERNEL_MODULE(xam::XamModule);
  LOAD_KERNEL_MODULE(xbdm::XbdmModule);
#undef LOAD_KERNEL_MODULE

  // Initialize emulator fallback exception handling last.
  ExceptionHandler::Install(Emulator::ExceptionCallbackThunk, this);

  return result;
}

X_STATUS Emulator::TerminateTitle() {
  if (!is_title_open()) {
    return X_STATUS_UNSUCCESSFUL;
  }

  kernel_state_->TerminateTitle();
  title_id_ = std::nullopt;
  title_name_ = "";
  title_version_ = "";
  on_terminate();
  return X_STATUS_SUCCESS;
}

X_STATUS Emulator::LaunchPath(const std::filesystem::path& path) {
  // Determine file type from extension.
  // On Android the game is opened via /proc/self/fd/N (no extension), so
  // resolve the symlink first to recover the real filename's extension.
  std::filesystem::path dispatch_path = path;
  if (!path.has_extension()) {
    std::error_code ec;
    auto real = std::filesystem::canonical(path, ec);
    if (!ec && real.has_extension()) {
      dispatch_path = real;
    }
  }

  if (!dispatch_path.has_extension()) {
    // Still no extension — assume STFS container.
    return LaunchStfsContainer(path);
  }
  auto extension =
      xe::utf8::lower_ascii(xe::path_to_utf8(dispatch_path.extension()));
  if (extension == ".xex" || extension == ".elf" || extension == ".exe") {
    return LaunchXexFile(path);
  } else {
    // Covers .iso, .xbla, .zar, and everything else — treat as disc image.
    return LaunchDiscImage(path);
  }
}

X_STATUS Emulator::LaunchXexFile(const std::filesystem::path& path) {
  // We create a virtual filesystem pointing to its directory and symlink
  // that to the game filesystem.
  // e.g., /my/files/foo.xex will get a local fs at:
  // \\Device\\Harddisk0\\Partition1
  // and then get that symlinked to game:\, so
  // -> game:\foo.xex

  auto mount_path = "\\Device\\Harddisk0\\Partition1";

  // Register the local directory in the virtual filesystem.
  auto parent_path = path.parent_path();
  auto device =
      std::make_unique<vfs::HostPathDevice>(mount_path, parent_path, true);
  if (!device->Initialize()) {
    XELOGE("Unable to scan host path");
    return X_STATUS_NO_SUCH_FILE;
  }
  if (!file_system_->RegisterDevice(std::move(device))) {
    XELOGE("Unable to register host path");
    return X_STATUS_NO_SUCH_FILE;
  }

  // Create symlinks to the device.
  file_system_->RegisterSymbolicLink("game:", mount_path);
  file_system_->RegisterSymbolicLink("d:", mount_path);

  // Get just the filename (foo.xex).
  auto file_name = path.filename();

  // Launch the game.
  auto fs_path = "game:\\" + xe::path_to_utf8(file_name);
  return CompleteLaunch(path, fs_path);
}

X_STATUS Emulator::LaunchDiscImage(const std::filesystem::path& path) {
  auto mount_path = "\\Device\\Cdrom0";

  // Register the disc image in the virtual filesystem.
  auto device = std::make_unique<vfs::DiscImageDevice>(mount_path, path);
  if (!device->Initialize()) {
    xe::FatalError("Unable to mount disc image; file not found or corrupt.");
    return X_STATUS_NO_SUCH_FILE;
  }
  if (!file_system_->RegisterDevice(std::move(device))) {
    xe::FatalError("Unable to register disc image.");
    return X_STATUS_NO_SUCH_FILE;
  }

  // Create symlinks to the device.
  file_system_->RegisterSymbolicLink("game:", mount_path);
  file_system_->RegisterSymbolicLink("d:", mount_path);

  // Launch the game.
  auto module_path(FindLaunchModule());
  return CompleteLaunch(path, module_path);
}

X_STATUS Emulator::LaunchStfsContainer(const std::filesystem::path& path) {
  auto mount_path = "\\Device\\Cdrom0";

  // Register the container in the virtual filesystem.
  auto device = std::make_unique<vfs::StfsContainerDevice>(mount_path, path);
  if (!device->Initialize()) {
    xe::FatalError(
        "Unable to mount STFS container; file not found or corrupt.");
    return X_STATUS_NO_SUCH_FILE;
  }
  if (!file_system_->RegisterDevice(std::move(device))) {
    xe::FatalError("Unable to register STFS container.");
    return X_STATUS_NO_SUCH_FILE;
  }

  file_system_->RegisterSymbolicLink("game:", mount_path);
  file_system_->RegisterSymbolicLink("d:", mount_path);

  // Launch the game.
  auto module_path(FindLaunchModule());
  return CompleteLaunch(path, module_path);
}

void Emulator::Pause() {
  if (paused_) {
    return;
  }
  paused_ = true;

  // Don't hold the lock on this (so any waits follow through)
  graphics_system_->Pause();
  audio_system_->Pause();

  auto lock = global_critical_region::AcquireDirect();
  auto threads =
      kernel_state()->object_table()->GetObjectsByType<kernel::XThread>(
          kernel::XObject::Type::Thread);
  auto current_thread = kernel::XThread::IsInThread()
                            ? kernel::XThread::GetCurrentThread()
                            : nullptr;
  for (auto thread : threads) {
    // Don't pause ourself or host threads.
    if (thread == current_thread || !thread->can_debugger_suspend()) {
      continue;
    }

    if (thread->is_running()) {
      thread->thread()->Suspend(nullptr);
    }
  }

  XELOGD("! EMULATOR PAUSED !");
}

void Emulator::Resume() {
  if (!paused_) {
    return;
  }
  paused_ = false;
  XELOGD("! EMULATOR RESUMED !");

  graphics_system_->Resume();
  audio_system_->Resume();

  auto threads =
      kernel_state()->object_table()->GetObjectsByType<kernel::XThread>(
          kernel::XObject::Type::Thread);
  for (auto thread : threads) {
    if (!thread->can_debugger_suspend()) {
      // Don't pause host threads.
      continue;
    }

    if (thread->is_running()) {
      thread->thread()->Resume(nullptr);
    }
  }
}

bool Emulator::SaveToFile(const std::filesystem::path& path) {
  Pause();

  filesystem::CreateEmptyFile(path);
  auto map = MappedMemory::Open(path, MappedMemory::Mode::kReadWrite, 0, 2_GiB);
  if (!map) {
    return false;
  }

  // Save the emulator state to a file
  ByteStream stream(map->data(), map->size());
  stream.Write(kEmulatorSaveSignature);
  stream.Write(title_id_.has_value());
  if (title_id_.has_value()) {
    stream.Write(title_id_.value());
  }

  // It's important we don't hold the global lock here! XThreads need to step
  // forward (possibly through guarded regions) without worry!
  processor_->Save(&stream);
  graphics_system_->Save(&stream);
  audio_system_->Save(&stream);
  kernel_state_->Save(&stream);
  memory_->Save(&stream);
  map->Close(stream.offset());

  Resume();
  return true;
}

bool Emulator::RestoreFromFile(const std::filesystem::path& path) {
  // Restore the emulator state from a file
  auto map = MappedMemory::Open(path, MappedMemory::Mode::kReadWrite);
  if (!map) {
    return false;
  }

  restoring_ = true;

  // Terminate any loaded titles.
  Pause();
  kernel_state_->TerminateTitle();

  auto lock = global_critical_region::AcquireDirect();
  ByteStream stream(map->data(), map->size());
  if (stream.Read<uint32_t>() != kEmulatorSaveSignature) {
    return false;
  }

  auto has_title_id = stream.Read<bool>();
  std::optional<uint32_t> title_id;
  if (!has_title_id) {
    title_id = {};
  } else {
    title_id = stream.Read<uint32_t>();
  }
  if (title_id_.has_value() != title_id.has_value() ||
      title_id_.value() != title_id.value()) {
    // Swapping between titles is unsupported at the moment.
    assert_always();
    return false;
  }

  if (!processor_->Restore(&stream)) {
    XELOGE("Could not restore processor!");
    return false;
  }
  if (!graphics_system_->Restore(&stream)) {
    XELOGE("Could not restore graphics system!");
    return false;
  }
  if (!audio_system_->Restore(&stream)) {
    XELOGE("Could not restore audio system!");
    return false;
  }
  if (!kernel_state_->Restore(&stream)) {
    XELOGE("Could not restore kernel state!");
    return false;
  }
  if (!memory_->Restore(&stream)) {
    XELOGE("Could not restore memory!");
    return false;
  }

  // Update the main thread.
  auto threads =
      kernel_state_->object_table()->GetObjectsByType<kernel::XThread>();
  for (auto thread : threads) {
    if (thread->main_thread()) {
      main_thread_ = thread;
      break;
    }
  }

  Resume();

  restore_fence_.Signal();
  restoring_ = false;

  return true;
}

bool Emulator::TitleRequested() {
  auto xam = kernel_state()->GetKernelModule<kernel::xam::XamModule>("xam.xex");
  return xam->loader_data().launch_data_present;
}

void Emulator::LaunchNextTitle() {
  auto xam = kernel_state()->GetKernelModule<kernel::xam::XamModule>("xam.xex");
  auto next_title = xam->loader_data().launch_path;

  CompleteLaunch("", next_title);
}

bool Emulator::ExceptionCallbackThunk(Exception* ex, void* data) {
  return reinterpret_cast<Emulator*>(data)->ExceptionCallback(ex);
}

bool Emulator::ExceptionCallback(Exception* ex) {
  // Check to see if the exception occurred in guest code.
  auto code_cache = processor()->backend()->code_cache();
  auto code_base = code_cache->execute_base_address();
  auto code_end = code_base + code_cache->total_size();

#if XE_PLATFORM_ANDROID
  // DIAGNOSTIC: rate-limited fault tracing to see what is faulting in a loop.
  {
    static std::atomic<uint64_t> s_exc_count{0};
    uint64_t n = s_exc_count.fetch_add(1, std::memory_order_relaxed);
    if ((n & 0x3FFF) == 0) {
      bool in_guest = ex->pc() >= code_base && ex->pc() < code_end;
      uint64_t lr = 0;
#if XE_ARCH_ARM64
      if (ex->thread_context()) lr = ex->thread_context()->x[30];
      XELOGE("EXC last_fn={:08X} prev_fn={:08X}", g_arm64_last_fn,
             g_arm64_last_fn_prev);
      XELOGE(
          "EXC ci_target={:08X} ci_saved={:08X} ci_matches={} ci_misses={}",
          g_arm64_ci_target, g_arm64_ci_saved, g_arm64_ci_matches,
          g_arm64_ci_misses);
      // Dump the prolog ring buffer in chronological order (oldest first) to
      // show the recursion cycle.
      {
        uint32_t pos = g_arm64_fn_ring_pos;
        std::string ring;
        for (uint32_t k = 0; k < 16; ++k) {
          uint32_t idx = (pos + k) & 0xF;
          char buf[12];
          snprintf(buf, sizeof(buf), "%08X ", g_arm64_fn_ring[idx]);
          ring += buf;
        }
        XELOGE("EXC fn_ring (oldest->newest): {}", ring);
      }
#endif
      XELOGE(
          "EXC#{} code={} pc={:016X} lr={:016X} fault_addr={:016X} in_guest={}",
          n, static_cast<int>(ex->code()), ex->pc(), lr, ex->fault_address(),
          in_guest ? 1 : 0);
#if XE_ARCH_ARM64
      // Store-watch dump FIRST (the fatal-signal handler is killed after ~24
      // log lines, so the watched-write trace must print before the larger
      // disasm dumps). Shows whether/with-what the watched guest range was
      // written before the fault.
      {
        uint32_t total = g_store_watch_pos;
        uint32_t count = total < 16 ? total : 16;
        uint32_t startk = total < 16 ? 0 : total - 16;
        XELOGE("EXC SW: {} writes to [{:08X},{:08X})", total, g_store_watch_lo,
               g_store_watch_hi);
        for (uint32_t k = 0; k < count; ++k) {
          auto& sw = g_store_watch_ring[(startk + k) & 127];
          XELOGE("EXC SW seq={} addr={:08X} val={:08X} pc={:08X}", sw.seq,
                 sw.addr, sw.value, sw.fn);
        }
        // Read the ACTUAL guest memory at the watched fields right now, to tell
        // a memory CORRUPTION (memory holds the bad value) from a LOAD bug
        // (memory is fine but the lwz produced garbage). 0x82FE7800=size,
        // 0x82FE7804=buffer ptr.
        {
          auto* xtm = kernel::XThread::GetCurrentThread();
          auto* cm = xtm ? xtm->thread_state()->context() : nullptr;
          if (cm) {
            uint8_t* mb = reinterpret_cast<uint8_t*>(cm->virtual_membase);
            uint32_t sz = xe::byte_swap(
                *reinterpret_cast<const uint32_t*>(mb + 0x82FE7800));
            uint32_t bp = xe::byte_swap(
                *reinterpret_cast<const uint32_t*>(mb + 0x82FE7804));
            uint32_t lc = xe::byte_swap(
                *reinterpret_cast<const uint32_t*>(mb + 0x82FE73EC));
            uint32_t gp = xe::byte_swap(
                *reinterpret_cast<const uint32_t*>(mb + 0x82B9CE90));
            XELOGE(
                "EXC MEM [82FE7800](size)={:08X} [82FE7804](buf)={:08X} "
                "[82FE73EC](loopN)={:08X} [82B9CE90](gobj)={:08X}",
                sz, bp, lc, gp);
          }
        }
        // Gated fn-entry ring (currently the [0x82B9CE90]-singleton DTOR body
        // 0x82A4E578): who calls the destructor, on what object?
        XELOGE("EXC DTOR entries={}", g_fa88_pos);
        for (uint32_t k = 0; k < (g_fa88_pos < 8 ? g_fa88_pos : 8); ++k) {
          XELOGE("EXC DTOR[{}] caller={:08X} r3={:08X} r4={:08X}", k,
                 g_fa88_caller_ring[k], g_fa88_r3_ring[k], g_fa88_r4_ring[k]);
        }
        // Disassemble the guest init routine around the store-watch writer
        // (the code that ZEROES the struct's buffer field) so we can see the
        // allocation+store that should fill it but is being skipped. Anchored
        // to the newest watched write's pc.
        if (count > 0) {
          auto* xt0 = kernel::XThread::GetCurrentThread();
          auto* c0 = xt0 ? xt0->thread_state()->context() : nullptr;
          uint32_t wpc = g_store_watch_ring[(startk + count - 1) & 127].fn;
          if (c0 && wpc) {
            uint8_t* mb = reinterpret_cast<uint8_t*>(c0->virtual_membase);
            for (uint32_t a = wpc - 0x18; a <= wpc + 0x28; a += 4) {
              uint32_t code =
                  xe::byte_swap(*reinterpret_cast<const uint32_t*>(mb + a));
              StringBuffer sb;
              xe::cpu::ppc::DisasmPPC(a, code, &sb);
              XELOGE("EXC I{} {:08X} {:08X} {}", a == wpc ? '>' : ' ', a, code,
                     sb.to_string_view());
            }
          }
        }
      }
      // Guest-side dump emitted HERE (before the host-stack walk and Pause(),
      // which can hang on a faulting guest thread) so it always reaches the log:
      // the guest PC, all 32 PPC GPRs (to see which pointer is 0), and the
      // faulting guest instruction disassembled.
      if (in_guest) {
        auto* gf = code_cache->LookupFunction(ex->pc());
        uint32_t gpc = gf ? gf->MapMachineCodeToGuestAddress(ex->pc()) : 0;
        auto* xt = kernel::XThread::GetCurrentThread();
        auto* ctx = xt ? xt->thread_state()->context() : nullptr;
        XELOGE("EXC guest_pc={:08X} fn_start={:08X} (ctx={})", gpc,
               gf ? static_cast<uint32_t>(gf->address()) : 0, ctx ? 1 : 0);
        if (ctx) {
          for (int r = 0; r < 32; r += 8)
            XELOGE(
                "  r{:<2}: {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} "
                "{:08X}",
                r, (uint32_t)ctx->r[r], (uint32_t)ctx->r[r + 1],
                (uint32_t)ctx->r[r + 2], (uint32_t)ctx->r[r + 3],
                (uint32_t)ctx->r[r + 4], (uint32_t)ctx->r[r + 5],
                (uint32_t)ctx->r[r + 6], (uint32_t)ctx->r[r + 7]);
          if (gpc) {
            uint8_t* mb = reinterpret_cast<uint8_t*>(ctx->virtual_membase);
            for (uint32_t a = gpc - 0x0C; a <= gpc + 0x0C; a += 4) {
              uint32_t code =
                  xe::byte_swap(*reinterpret_cast<const uint32_t*>(mb + a));
              StringBuffer sb;
              xe::cpu::ppc::DisasmPPC(a, code, &sb);
              XELOGE("EXC G{} {:08X} {:08X} {}", a == gpc ? '>' : ' ', a, code,
                     sb.to_string_view());
            }
          }
          // Map the link register (the caller's return address) to a guest
          // address and disassemble the caller's call site, to see how the
          // (null) argument in r3 was set up before this call.
          uint64_t hlr = ex->thread_context() ? ex->thread_context()->x[30] : 0;
          auto* cf = (hlr >= code_base && hlr < code_end)
                         ? code_cache->LookupFunction(hlr)
                         : nullptr;
          uint32_t clr = cf ? cf->MapMachineCodeToGuestAddress(hlr) : 0;
          XELOGE("EXC caller lr_guest={:08X}", clr);
          if (clr) {
            uint8_t* mb = reinterpret_cast<uint8_t*>(ctx->virtual_membase);
            for (uint32_t a = clr - 0x24; a <= clr + 0x04; a += 4) {
              uint32_t code =
                  xe::byte_swap(*reinterpret_cast<const uint32_t*>(mb + a));
              StringBuffer sb;
              xe::cpu::ppc::DisasmPPC(a, code, &sb);
              XELOGE("EXC C{} {:08X} {:08X} {}", a == clr ? '>' : ' ', a, code,
                     sb.to_string_view());
            }
          }
        }
      }
#endif
    }
#if XE_ARCH_ARM64
    // One-shot host-stack walk to find the Xenia caller of the faulting libc
    // function. Guarded to run exactly once so a bad frame pointer can't create
    // a fault storm.
    static std::atomic<bool> s_walked{false};
    if (ex->thread_context() && !s_walked.exchange(true)) {
      uint64_t fp = ex->thread_context()->x[29];
      std::string fr;
      for (int i = 0; i < 8 && fp && (fp & 7) == 0; ++i) {
        uint64_t ret = *reinterpret_cast<uint64_t*>(fp + 8);
        uint64_t next = *reinterpret_cast<uint64_t*>(fp);
        fr += fmt::format("{:012X} ", ret);
        if (next <= fp) break;
        fp = next;
      }
      XELOGE("EXC host stack: {}", fr);
    }
#endif
  }
  // IsDebuggerAttached() parses /proc/self/status on every call, which is
  // catastrophically slow on the exception path. Cache it (a debugger
  // attaching mid-run is not a case we need to handle live).
  static const bool s_other_debugger_attached = debugging::IsDebuggerAttached();
  if (!processor()->is_debugger_attached() && s_other_debugger_attached) {
#else
  if (!processor()->is_debugger_attached() && debugging::IsDebuggerAttached()) {
#endif
    // If Xenia's debugger isn't attached but another one is, pass it to that
    // debugger.
    return false;
  } else if (processor()->is_debugger_attached()) {
    // Let the debugger handle this exception. It may decide to continue past it
    // (if it was a stepping breakpoint, etc).
    return processor()->OnUnhandledException(ex);
  }

  if (!(ex->pc() >= code_base && ex->pc() < code_end)) {
    // Didn't occur in guest code. Let it pass.
    return false;
  }

  // Within range. Pause the emulator and eat the exception.
  Pause();

  // Dump information into the log.
  auto current_thread = kernel::XThread::GetCurrentThread();
  assert_not_null(current_thread);

  auto guest_function = code_cache->LookupFunction(ex->pc());
  auto context = current_thread->thread_state()->context();

  // DIAGNOSTIC: dump the faulting instruction words + register state for any
  // in-guest fault so we can pinpoint a bad guest memory access. Log the guest
  // PC if known. Kept lightweight and safe (no guest-memory reads, which could
  // re-fault and kill the process).
  {
    const uint32_t* ip = reinterpret_cast<const uint32_t*>(ex->pc());
    uint32_t guest_pc =
        guest_function
            ? guest_function->MapMachineCodeToGuestAddress(ex->pc())
            : 0;
    XELOGE(
        "EXC GUESTFAULT host_pc={:016X} (offset {:08X}) guest_pc={:08X} "
        "fault_addr={:016X} insn[-1..+1]={:08X} [{:08X}] {:08X}",
        ex->pc(), static_cast<uint32_t>(ex->pc() - code_base), guest_pc,
        ex->fault_address(), ip[-1], ip[0], ip[1]);
    // Host (ARM64) instructions leading up to the fault — shows the actual EA
    // computation (e.g. whether membase was added before an atomic LDAXR).
    {
      char hl[160]; int p = 0;
      for (int k = -10; k <= 1; ++k)
        p += snprintf(hl + p, sizeof(hl) - p, "%08X%s", ip[k],
                      k == 0 ? "* " : " ");
      XELOGE("EXC HOSTASM [{:08X}-]: {}",
             static_cast<uint32_t>(ex->pc() - code_base) - 40, hl);
    }
    uint8_t* membase = reinterpret_cast<uint8_t*>(context->virtual_membase);
#if XE_ARCH_ARM64
    // Gated-function entry ring FIRST (the fatal-signal handler is killed after
    // ~24 log lines): caller + r3/r4 args of the traced fn (0x829216D0), plus
    // an ASCII peek at the most recent r4 (the search-path string) and r3.
    {
      XELOGE("EXC FA88 ring pos={}", g_fa88_pos);
      for (int k = 0; k < 8; ++k) {
        XELOGE("EXC FA88[{}] caller={:08X} r3={:08X} r4={:08X}", k,
               g_fa88_caller_ring[k], g_fa88_r3_ring[k], g_fa88_r4_ring[k]);
      }
      auto peek_str = [&](const char* tag, uint32_t ga) {
        if (!ga || ga >= 0xC0000000u) return;
        char s[49];
        const char* p = reinterpret_cast<const char*>(membase + ga);
        int n = 0;
        for (; n < 48; ++n) {
          char c = p[n];
          if (!c) break;
          s[n] = (c >= 0x20 && c < 0x7F) ? c : '.';
        }
        s[n] = 0;
        XELOGE("EXC FA88 last {} str @{:08X}: \"{}\"", tag, ga, s);
      };
      if (g_fa88_pos) {
        uint32_t idx = (g_fa88_pos - 1) & 7;
        peek_str("r4", g_fa88_r4_ring[idx]);
        peek_str("r3", g_fa88_r3_ring[idx]);
      }
    }
    // Store-watch dump (the watched-write trace must print early too).
    {
      uint32_t total = g_store_watch_pos;
      uint32_t count = total < 16 ? total : 16;
      uint32_t startk = total < 16 ? 0 : total - 16;
      XELOGE("EXC SW: {} writes to [{:08X},{:08X})", total, g_store_watch_lo,
             g_store_watch_hi);
      for (uint32_t k = 0; k < count; ++k) {
        auto& sw = g_store_watch_ring[(startk + k) & 127];
        XELOGE("EXC SW seq={} addr={:08X} val={:08X} (bswap {:08X}) pc={:08X}",
               sw.seq, sw.addr, sw.value, xe::byte_swap(sw.value), sw.fn);
      }
    }
#endif
    // Disassemble a window of the original guest PPC code around the faulting
    // instruction. The XEX is mapped into guest memory, so we read the raw
    // big-endian opcodes via the membase and decode with Xenia's PPC
    // disassembler. A '>' marks the faulting instruction. Logged first so the
    // register/heap dump below survives logd "chatty" throttling.
    // Registers FIRST so all 32 survive logd "chatty" throttling.
    for (int i = 0; i < 32; i += 4) {
      XELOGE(" r{:<2}-{:<2} = {:016X} {:016X} {:016X} {:016X}", i, i + 3,
             context->r[i], context->r[i + 1], context->r[i + 2],
             context->r[i + 3]);
    }
    auto disasm_range = [&](const char* tag, uint32_t from, uint32_t to,
                            uint32_t mark) {
      for (uint32_t a = from; a <= to; a += 4) {
        uint32_t code = xe::byte_swap(
            *reinterpret_cast<const uint32_t*>(membase + a));
        StringBuffer sb;
        xe::cpu::ppc::DisasmPPC(a, code, &sb);
        XELOGE("EXC {}{} {:08X}  {:08X}  {}", tag, a == mark ? ">" : " ", a,
               code, sb.to_string_view());
      }
    };
    if (guest_pc) {
      disasm_range("", guest_pc - 0x34, guest_pc + 0x28, guest_pc);
    }
    // Dump a few guest pointers' target memory (bounds-checked to the 512 MiB
    // guest space so a wild pointer can't re-fault) to inspect heap structures.
    auto dump_guest = [&](const char* tag, uint32_t ga) {
      if (ga == 0 || ga >= 0xC0000000u) {
        XELOGE("EXC {} @{:08X}: <unmapped/null>", tag, ga);
        return;
      }
      const uint32_t* w = reinterpret_cast<const uint32_t*>(membase + ga);
      XELOGE("EXC {} @{:08X}: {:08X} {:08X} {:08X} {:08X} {:08X} {:08X}", tag, ga,
             xe::byte_swap(w[0]), xe::byte_swap(w[1]), xe::byte_swap(w[2]),
             xe::byte_swap(w[3]), xe::byte_swap(w[4]), xe::byte_swap(w[5]));
    };
    uint32_t r3 = static_cast<uint32_t>(context->r[3]);
    dump_guest("r3+00 ", r3);
    dump_guest("r3+40 ", r3 + 0x40);
    dump_guest("r3+58 ", r3 + 0x58);  // free-list head array near the bad index
    dump_guest("r3+180", r3 + 0x180);
    uint32_t r4 = static_cast<uint32_t>(context->r[4]);
    // The chunk being freed = r4 - r8 (r8 is the block size added to reach the
    // "next chunk"). Dump its header to inspect the size-class byte at +4.
    dump_guest("chunk ", r4 - static_cast<uint32_t>(context->r[8]));
    // Scan backward from the bad "next chunk" to find the real chunk header /
    // boundary, to tell a size overshoot from a lost header-init store.
    dump_guest("r4-80 ", r4 - 0x80);
    dump_guest("r4-40 ", r4 - 0x40);
    dump_guest("r4-20 ", r4 - 0x20);
    dump_guest("r4-10 ", r4 - 0x10);
    dump_guest("r4    ", r4);
    dump_guest("r10   ", static_cast<uint32_t>(context->r[10]));
    dump_guest("r9    ", static_cast<uint32_t>(context->r[9]));
    // Generic: dump targets of the registers commonly involved in pointer-walk
    // faults so any near-null deref can be traced without re-instrumenting.
    dump_guest("r11   ", static_cast<uint32_t>(context->r[11]));
    dump_guest("r28   ", static_cast<uint32_t>(context->r[28]));
    dump_guest("r29   ", static_cast<uint32_t>(context->r[29]));
    dump_guest("r30   ", static_cast<uint32_t>(context->r[30]));
    dump_guest("r30-10", static_cast<uint32_t>(context->r[30]) - 0x10);
    dump_guest("r31   ", static_cast<uint32_t>(context->r[31]));
    // GROUND-TRUTH DIFF vs Windows Xenia: block 0x40000630 lands in the wrong
    // free-list bucket. Windows chunk630 hdr = 00050063 00010000 (byte[+4]=0,
    // bucket 0); ours puts it in bucket 1 (byte[+4]=1). Dump our copy to see the
    // exact wrong field.
    dump_guest("c630  ", 0x40000630);
    dump_guest("c680  ", 0x40000680);
    // Dump the store-watch ring (writes to the watched heap LIST_ENTRY range),
    // in chronological order. value is shown both raw and byte-swapped (a guest
    // pointer store is byte-swapped in memory).
#if XE_ARCH_ARM64
    {
      uint32_t total = g_store_watch_pos;
      uint32_t count = total < 128 ? total : 128;
      uint32_t startk = total < 128 ? 0 : total - 128;
      XELOGE("EXC FBC8 ctx-regs: r4={:08X} r25={:08X} r28={:08X} r31={:08X}",
             g_fbc8_r4, g_fbc8_r25, g_fbc8_r28, g_fbc8_r31);
      XELOGE("EXC FBC8 stored value={:08X}  FA88 local-spill ops={}",
             g_fbc8_storeval, g_fa88_local_ops);
      XELOGE("EXC FBC8 x21-x27 = {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X}",
             g_fbc8_xregs[0], g_fbc8_xregs[1], g_fbc8_xregs[2], g_fbc8_xregs[3],
             g_fbc8_xregs[4], g_fbc8_xregs[5], g_fbc8_xregs[6]);
      XELOGE("EXC store-watch: {} writes to [{:08X},{:08X})", total,
             g_store_watch_lo, g_store_watch_hi);
      for (uint32_t k = 0; k < count; ++k) {
        auto& e = g_store_watch_ring[(startk + k) & 127];
        XELOGE("EXC  SW seq={} addr={:08X} val={:08X} (bswap {:08X}) pc={:08X}",
               e.seq, e.addr, e.value, xe::byte_swap(e.value), e.fn);
      }
      // Disassemble a window around the PC of the last write (the corrupting
      // store) so we can see how the stored register was built.
      if (count) {
        uint32_t pc = g_store_watch_ring[(g_store_watch_pos - 1) & 127].fn;
        if (pc) {
          for (uint32_t a = pc - 0x20; a <= pc + 0x8; a += 4) {
            uint32_t code = xe::byte_swap(
                *reinterpret_cast<const uint32_t*>(membase + a));
            StringBuffer sb;
            xe::cpu::ppc::DisasmPPC(a, code, &sb);
            XELOGE("EXC  SWdis {} {:08X}  {:08X}  {}", a == pc ? ">" : " ", a,
                   code, sb.to_string_view());
          }
        }
      }
    }
#endif
    return false;
  }

  XELOGE("==== CRASH DUMP ====");
  XELOGE("Thread ID (Host: 0x{:08X} / Guest: 0x{:08X})",
         current_thread->thread()->system_id(), current_thread->thread_id());
  XELOGE("Thread Handle: 0x{:08X}", current_thread->handle());
  XELOGE("PC: 0x{:08X}",
         guest_function->MapMachineCodeToGuestAddress(ex->pc()));
  XELOGE("Registers:");
  for (int i = 0; i < 32; i++) {
    XELOGE(" r{:<3} = {:016X}", i, context->r[i]);
  }
  for (int i = 0; i < 32; i++) {
    XELOGE(" f{:<3} = {:016X} = (double){} = (float){}", i,
           *reinterpret_cast<uint64_t*>(&context->f[i]), context->f[i],
           *(float*)&context->f[i]);
  }
  for (int i = 0; i < 128; i++) {
    XELOGE(" v{:<3} = [0x{:08X}, 0x{:08X}, 0x{:08X}, 0x{:08X}]", i,
           context->v[i].u32[0], context->v[i].u32[1], context->v[i].u32[2],
           context->v[i].u32[3]);
  }

  // Display a dialog telling the user the guest has crashed.
  if (display_window_ && imgui_drawer_) {
    display_window_->app_context().CallInUIThreadSynchronous([this]() {
      xe::ui::ImGuiDialog::ShowMessageBox(
          imgui_drawer_, "Uh-oh!",
          "The guest has crashed.\n\n"
          ""
          "Xenia has now paused itself.\n"
          "A crash dump has been written into the log.");
    });
  }

  // Now suspend ourself (we should be a guest thread).
  current_thread->Suspend(nullptr);

  // We should not arrive here!
  assert_always();
  return false;
}

void Emulator::WaitUntilExit() {
  while (true) {
    if (main_thread_) {
      xe::threading::Wait(main_thread_->thread(), false);
    }

    if (restoring_) {
      restore_fence_.Wait();
    } else {
      // Not restoring and the thread exited. We're finished.
      break;
    }
  }

  on_exit();
}

void Emulator::AddGameConfigLoadCallback(GameConfigLoadCallback* callback) {
  assert_not_null(callback);
  // Game config load callbacks handling is entirely in the UI thread.
  assert_true(!display_window_ ||
              display_window_->app_context().IsInUIThread());
  // Check if already added.
  if (std::find(game_config_load_callbacks_.cbegin(),
                game_config_load_callbacks_.cend(),
                callback) != game_config_load_callbacks_.cend()) {
    return;
  }
  game_config_load_callbacks_.push_back(callback);
}

void Emulator::RemoveGameConfigLoadCallback(GameConfigLoadCallback* callback) {
  assert_not_null(callback);
  // Game config load callbacks handling is entirely in the UI thread.
  assert_true(!display_window_ ||
              display_window_->app_context().IsInUIThread());
  auto it = std::find(game_config_load_callbacks_.cbegin(),
                      game_config_load_callbacks_.cend(), callback);
  if (it == game_config_load_callbacks_.cend()) {
    return;
  }
  if (game_config_load_callback_loop_next_index_ != SIZE_MAX) {
    // Actualize the next callback index after the erasure from the vector.
    size_t existing_index =
        size_t(std::distance(game_config_load_callbacks_.cbegin(), it));
    if (game_config_load_callback_loop_next_index_ > existing_index) {
      --game_config_load_callback_loop_next_index_;
    }
  }
  game_config_load_callbacks_.erase(it);
}

std::string Emulator::FindLaunchModule() {
  std::string path("game:\\");

  if (!cvars::launch_module.empty()) {
    return path + cvars::launch_module;
  }

  std::string default_module("default.xex");

  auto gameinfo_entry(file_system_->ResolvePath(path + "GameInfo.bin"));
  if (gameinfo_entry) {
    vfs::File* file = nullptr;
    X_STATUS result =
        gameinfo_entry->Open(vfs::FileAccess::kGenericRead, &file);
    if (XSUCCEEDED(result)) {
      std::vector<uint8_t> buffer(gameinfo_entry->size());
      size_t bytes_read = 0;
      result = file->ReadSync(buffer.data(), buffer.size(), 0, &bytes_read);
      if (XSUCCEEDED(result)) {
        kernel::util::GameInfo info(buffer);
        if (info.is_valid()) {
          XELOGI("Found virtual title {}", info.virtual_title_id());

          const std::string xna_id("584E07D1");
          auto xna_id_entry(file_system_->ResolvePath(path + xna_id));
          if (xna_id_entry) {
            default_module = xna_id + "\\" + info.module_name();
          } else {
            XELOGE("Could not find fixed XNA path {}", xna_id);
          }
        }
      }
    }
  }

  return path + default_module;
}

static std::string format_version(xex2_version version) {
  // fmt::format doesn't like bit fields
  uint32_t major, minor, build, qfe;
  major = version.major;
  minor = version.minor;
  build = version.build;
  qfe = version.qfe;
  if (qfe) {
    return fmt::format("{}.{}.{}.{}", major, minor, build, qfe);
  }
  if (build) {
    return fmt::format("{}.{}.{}", major, minor, build);
  }
  return fmt::format("{}.{}", major, minor);
}

X_STATUS Emulator::CompleteLaunch(const std::filesystem::path& path,
                                  const std::string_view module_path) {
  // Making changes to the UI (setting the icon) and executing game config load
  // callbacks which expect to be called from the UI thread.
  // On Android, LaunchPath is called from the emulator thread to avoid blocking
  // the UI thread; SetIcon calls are dispatched asynchronously below.
#if !XE_PLATFORM_ANDROID
  assert_true(display_window_->app_context().IsInUIThread());
#endif

#if XE_PLATFORM_ANDROID
  // DIAGNOSTIC watchdog: periodically dump the main guest thread's spin-loop
  // registers (r8 = the looping byte, r10/r11 = the polled pointer state) so we
  // can see what the boot code is stuck waiting on.
  static std::atomic<bool> s_watchdog_started{false};
  if (!s_watchdog_started.exchange(true)) {
    // Install the sampling signal handler once.
    struct sigaction sa = {};
    sa.sa_sigaction = XeSampleSignalHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(kSampleSignal, &sa, nullptr);

    std::thread([this]() {
      // DIAGNOSTIC watchdog: tight-poll the XNotify registry head [0x921D00BC]
      // (Flink/Blink) and log every transition with a monotonic iter counter.
      // Reads via /proc/self/mem (pread) so an uncommitted guest page returns an
      // error instead of faulting this thread. Discriminates "the head is
      // momentarily 0 (a true race / late init)" from "the head is always valid
      // and the guest load misreads it (a JIT load bug)".
      uint8_t* mb = memory_ ? memory_->virtual_membase() : nullptr;
      if (!mb) return;
      int memfd = open("/proc/self/mem", O_RDONLY | O_CLOEXEC);
      if (memfd < 0) return;
      uint64_t addr = reinterpret_cast<uint64_t>(mb) + 0x921D00BC;
      uint32_t last_f = 0xDEADBEEFu, last_b = 0xDEADBEEFu;
      uint64_t iters = 0;
      for (;;) {
        uint32_t raw[2] = {0, 0};
        if (pread64(memfd, raw, 8, static_cast<off_t>(addr)) == 8) {
          uint32_t f = xe::byte_swap(raw[0]);
          uint32_t b = xe::byte_swap(raw[1]);
          if (f != last_f || b != last_b) {
            XELOGE("WD 921D00BC Flink={:08X} Blink={:08X} (iter {})", f, b,
                   iters);
            last_f = f;
            last_b = b;
          }
        }
        ++iters;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    }).detach();
  }
#endif

  // Setup NullDevices for raw HDD partition accesses
  // Cache/STFC code baked into games tries reading/writing to these
  // By using a NullDevice that just returns success to all IO requests it
  // should allow games to believe cache/raw disk was accessed successfully

  // NOTE: this should probably be moved to xenia_main.cc, but right now we need
  // to register the \Device\Harddisk0\ NullDevice _after_ the
  // \Device\Harddisk0\Partition1 HostPathDevice, otherwise requests to
  // Partition1 will go to this. Registering during CompleteLaunch allows us to
  // make sure any HostPathDevices are ready beforehand.
  // (see comment above cache:\ device registration for more info about why)
  auto null_paths = {std::string("\\Partition0"), std::string("\\Cache0"),
                     std::string("\\Cache1")};
  auto null_device =
      std::make_unique<vfs::NullDevice>("\\Device\\Harddisk0", null_paths);
  if (null_device->Initialize()) {
    file_system_->RegisterDevice(std::move(null_device));
  }

  // Reset state.
  title_id_ = std::nullopt;
  title_name_ = "";
  title_version_ = "";
#if XE_PLATFORM_ANDROID
  {
    auto* w = display_window_;
    display_window_->app_context().CallInUIThread([w]() { w->SetIcon(nullptr, 0); });
  }
#else
  display_window_->SetIcon(nullptr, 0);
#endif

  // Allow xam to request module loads.
  auto xam = kernel_state()->GetKernelModule<kernel::xam::XamModule>("xam.xex");

  XELOGI("Launching module {}", module_path);
  auto module = kernel_state_->LoadUserModule(module_path);
  if (!module) {
    XELOGE("Failed to load user module {}", xe::path_to_utf8(path));
    return X_STATUS_NOT_FOUND;
  }

  // Grab the current title ID.
  xex2_opt_execution_info* info = nullptr;
  module->GetOptHeader(XEX_HEADER_EXECUTION_INFO, &info);

  if (!info) {
    title_id_ = 0;
  } else {
    title_id_ = info->title_id;
    auto title_version = info->version();
    if (title_version.value != 0) {
      title_version_ = format_version(title_version);
    }
  }

  // Try and load the resource database (xex only).
  if (module->title_id()) {
    auto title_id = fmt::format("{:08X}", module->title_id());

    // Load the per-game configuration file and make sure updates are handled by
    // the callbacks.
    config::LoadGameConfig(title_id);
    assert_true(game_config_load_callback_loop_next_index_ == SIZE_MAX);
    game_config_load_callback_loop_next_index_ = 0;
    while (game_config_load_callback_loop_next_index_ <
           game_config_load_callbacks_.size()) {
      game_config_load_callbacks_[game_config_load_callback_loop_next_index_++]
          ->PostGameConfigLoad();
    }
    game_config_load_callback_loop_next_index_ = SIZE_MAX;

    const kernel::util::XdbfGameData db = kernel_state_->module_xdbf(module);
    if (db.is_valid()) {
      XLanguage language =
          db.GetExistingLanguage(static_cast<XLanguage>(cvars::user_language));
      title_name_ = db.title(language);

      XELOGI("-------------------- ACHIEVEMENTS --------------------");
      const std::vector<kernel::util::XdbfAchievementTableEntry>
          achievement_list = db.GetAchievements();
      for (const kernel::util::XdbfAchievementTableEntry& entry :
           achievement_list) {
        std::string label = db.GetStringTableEntry(language, entry.label_id);
        std::string desc =
            db.GetStringTableEntry(language, entry.description_id);

        XELOGI("{} - {} - {} - {}", entry.id, label, desc, entry.gamerscore);
      }
      XELOGI("----------------- END OF ACHIEVEMENTS ----------------");

      auto icon_block = db.icon();
      if (icon_block) {
#if XE_PLATFORM_ANDROID
        // Copy buffer since icon_block lifetime ends when db goes out of scope.
        std::vector<uint8_t> icon_copy(
            static_cast<const uint8_t*>(icon_block.buffer),
            static_cast<const uint8_t*>(icon_block.buffer) + icon_block.size);
        auto* w = display_window_;
        display_window_->app_context().CallInUIThread(
            [w, ic = std::move(icon_copy)]() {
              w->SetIcon(ic.data(), ic.size());
            });
#else
        display_window_->SetIcon(icon_block.buffer, icon_block.size);
#endif
      }
    }
  }

  // Initializing the shader storage in a blocking way so the user doesn't miss
  // the initial seconds - for instance, sound from an intro video may start
  // playing before the video can be seen if doing this in parallel with the
  // main thread.
  on_shader_storage_initialization(true);
  graphics_system_->InitializeShaderStorage(cache_root_, title_id_.value(),
                                            true);
  on_shader_storage_initialization(false);

  auto main_thread = kernel_state_->LaunchModule(module);
  if (!main_thread) {
    return X_STATUS_UNSUCCESSFUL;
  }
  main_thread_ = main_thread;
  on_launch(title_id_.value(), title_name_);

  return X_STATUS_SUCCESS;
}

}  // namespace xe
