// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_DAWN_NATIVE_EVENTMANAGER_H_
#define SRC_DAWN_NATIVE_EVENTMANAGER_H_

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <variant>

#include "dawn/common/FutureUtils.h"
#include "dawn/common/MutexProtected.h"
#include "dawn/common/NonCopyable.h"
#include "dawn/common/Ref.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/SystemEvent.h"

namespace dawn::native {

struct InstanceDescriptor;

// Subcomponent of the Instance which tracks callback events for the Future-based callback
// entrypoints. All events from this instance (regardless of whether from an adapter, device, queue,
// etc.) are tracked here, and used by the instance-wide ProcessEvents and WaitAny entrypoints.
//
// TODO(crbug.com/dawn/2050): Can this eventually replace CallbackTaskManager?
//
// There are various ways to optimize ProcessEvents/WaitAny:
// - TODO(crbug.com/dawn/2059) Spontaneously set events as "early-ready" in other places when we see
//   serials advance, e.g. Submit, or when checking a later wait before an earlier wait.
// - TODO(crbug.com/dawn/2049) For thread-driven events (async pipeline compilation and Metal queue
//   events), defer tracking for ProcessEvents until the event is already completed.
class EventManager final : NonMovable {
  public:
    EventManager();
    ~EventManager();

    MaybeError Initialize(const InstanceDescriptor*);
    // Called by WillDropLastExternalRef. Once shut down, the EventManager stops tracking anything.
    // It drops any refs to TrackedEvents, to break reference cycles. If doing so frees the last ref
    // of any uncompleted TrackedEvents, they'll get completed with EventCompletionType::Shutdown.
    void ShutDown();

    class TrackedEvent;
    // Track a TrackedEvent and give it a FutureID.
    [[nodiscard]] FutureID TrackEvent(wgpu::CallbackMode mode, Ref<TrackedEvent>&&);
    void ProcessPollEvents();
    [[nodiscard]] wgpu::WaitStatus WaitAny(size_t count,
                                           FutureWaitInfo* infos,
                                           Nanoseconds timeout);

  private:
    bool mTimedWaitAnyEnable = false;
    size_t mTimedWaitAnyMaxCount = kTimedWaitAnyMaxCountDefault;
    std::atomic<FutureID> mNextFutureID = 1;

    // Only 1 thread is allowed to call ProcessEvents at a time. This lock ensures that.
    std::mutex mProcessEventLock;

    // Freed once the user has dropped their last ref to the Instance, so can't call WaitAny or
    // ProcessEvents anymore. This breaks reference cycles.
    using EventMap = std::unordered_map<FutureID, Ref<TrackedEvent>>;
    std::optional<MutexProtected<EventMap>> mEvents;
};

struct QueueAndSerial {
    Ref<QueueBase> queue;
    ExecutionSerial completionSerial;
};

// Base class for the objects that back WGPUFutures. TrackedEvent is responsible for the lifetime
// the callback it contains. If TrackedEvent gets destroyed before it completes, it's responsible
// for cleaning up (by calling the callback with an "Unknown" status).
//
// For Future-based and ProcessEvents-based TrackedEvents, the EventManager will track them for
// completion in WaitAny or ProcessEvents. However, once the Instance has lost all its external
// refs, the user can't call either of those methods anymore, so EventManager will stop holding refs
// to any TrackedEvents. Any which are not ref'd elsewhere (in order to be `Spontaneous`ly
// completed) will be cleaned up at that time.
class EventManager::TrackedEvent : public RefCounted {
  protected:
    // Note: TrackedEvents are (currently) only for Device events. Events like RequestAdapter and
    // RequestDevice complete immediately in dawn native, so should never need to be tracked.
    TrackedEvent(wgpu::CallbackMode callbackMode, Ref<SystemEvent> completionEvent);

    // Create a TrackedEvent from a queue completion serial.
    TrackedEvent(wgpu::CallbackMode callbackMode,
                 QueueBase* queue,
                 ExecutionSerial completionSerial);

  public:
    // Subclasses must implement this to complete the event (if not completed) with
    // EventCompletionType::Shutdown.
    ~TrackedEvent() override;

    class WaitRef;
    // Events may be one of two types:
    // - A queue and the ExecutionSerial after which the event will be completed.
    //   Used for queue completion.
    // - A SystemEvent which will be signaled from our code, usually on a separate thread.
    //   It stores a boolean that we can check instead of polling with the OS, or it can be
    //   transformed lazily into a SystemEventReceiver. Used for async pipeline creation, and Metal
    //   queue completion.
    // The queue ref creates a temporary ref cycle
    // (Queue->Device->Instance->EventManager->TrackedEvent). This is OK because the instance will
    // clear out the EventManager on shutdown.
    // TODO(crbug.com/dawn/2067): This is a bit fragile. Is it possible to remove the ref cycle?
    using CompletionData = std::variant<QueueAndSerial, Ref<SystemEvent>>;

    const CompletionData& GetCompletionData() const;

  protected:
    void EnsureComplete(EventCompletionType);
    void CompleteIfSpontaneous();

    virtual void Complete(EventCompletionType) = 0;

    wgpu::CallbackMode mCallbackMode;

#if DAWN_ENABLE_ASSERTS
    std::atomic<bool> mCurrentlyBeingWaited;
#endif

  private:
    friend class EventManager;

    CompletionData mCompletionData;
    // Callback has been called.
    std::atomic<bool> mCompleted = false;
};

// A Ref<TrackedEvent>, but ASSERTing that a future isn't used concurrently in multiple
// WaitAny/ProcessEvents call (by checking that there's never more than one WaitRef for a
// TrackedEvent). While concurrent calls on the same futures are not explicitly disallowed, they are
// generally unintentional, and hence this can help to identify potential bugs. Note that for
// WaitAny, this checks the embedder's behavior, but for ProcessEvents this is only an internal
// DAWN_ASSERT (it's supposed to be synchronized so that this never happens).
class EventManager::TrackedEvent::WaitRef : dawn::NonCopyable {
  public:
    WaitRef(WaitRef&& rhs) = default;
    WaitRef& operator=(WaitRef&& rhs) = default;

    explicit WaitRef(TrackedEvent* future);
    ~WaitRef();

    TrackedEvent* operator->();
    const TrackedEvent* operator->() const;

  private:
    Ref<TrackedEvent> mRef;
};

// TrackedEvent::WaitRef plus a few extra fields needed for some implementations.
// Sometimes they'll be unused, but that's OK; it simplifies code reuse.
struct TrackedFutureWaitInfo {
    FutureID futureID;
    EventManager::TrackedEvent::WaitRef event;
    // Used by EventManager::ProcessPollEvents
    size_t indexInInfos;
    // Used by EventManager::ProcessPollEvents and ::WaitAny
    bool ready;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_EVENTMANAGER_H_
