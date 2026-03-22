#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "cocoawindowcomponent.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Key-code table: macOS hardware keycode → Windows-compatible VK code.
// This keeps runwindowtask.cpp (and upstream handler code) platform-agnostic.
// ---------------------------------------------------------------------------
namespace {

unsigned int mapMacKeyCode(unsigned short macKey) {
  // clang-format off
    static const std::unordered_map<unsigned short, unsigned int> kTable = {
        // Letters (kVK_ANSI_*)
        {0x00, 'A'}, {0x01, 'S'}, {0x02, 'D'}, {0x03, 'F'},
        {0x04, 'H'}, {0x05, 'G'}, {0x06, 'Z'}, {0x07, 'X'},
        {0x08, 'C'}, {0x09, 'V'}, {0x0B, 'B'}, {0x0C, 'Q'},
        {0x0D, 'W'}, {0x0E, 'E'}, {0x0F, 'R'}, {0x10, 'Y'},
        {0x11, 'T'}, {0x1F, 'O'}, {0x20, 'U'}, {0x22, 'I'},
        {0x23, 'P'}, {0x25, 'L'}, {0x26, 'J'}, {0x28, 'K'},
        {0x2D, 'N'}, {0x2E, 'M'},
        // Digits
        {0x12, '1'}, {0x13, '2'}, {0x14, '3'}, {0x15, '4'},
        {0x17, '5'}, {0x16, '6'}, {0x1A, '7'}, {0x1C, '8'},
        {0x19, '9'}, {0x1D, '0'},
        // Control keys
        {0x24, 0x0D},  // Return      → VK_RETURN
        {0x30, 0x09},  // Tab         → VK_TAB
        {0x31, 0x20},  // Space       → VK_SPACE
        {0x33, 0x08},  // Backspace   → VK_BACK
        {0x35, 0x1B},  // Escape      → VK_ESCAPE
        // Modifier keys
        {0x38, 0xA0},  // Left Shift    → VK_LSHIFT  (0x10 also accepted)
        {0x3C, 0xA1},  // Right Shift   → VK_RSHIFT
        {0x3B, 0xA2},  // Left Control  → VK_LCONTROL
        {0x3E, 0xA3},  // Right Control → VK_RCONTROL
        {0x3A, 0xA4},  // Left Option   → VK_LMENU
        {0x3D, 0xA5},  // Right Option  → VK_RMENU
        {0x37, 0x5B},  // Left Command  → VK_LWIN
        {0x36, 0x5C},  // Right Command → VK_RWIN
        {0x39, 0x14},  // Caps Lock     → VK_CAPITAL
        // Arrow keys
        {0x7B, 0x25},  // Left  → VK_LEFT
        {0x7C, 0x27},  // Right → VK_RIGHT
        {0x7D, 0x28},  // Down  → VK_DOWN
        {0x7E, 0x26},  // Up    → VK_UP
        // Function keys
        {0x7A, 0x70}, {0x78, 0x71}, {0x63, 0x72}, {0x76, 0x73},
        {0x60, 0x74}, {0x61, 0x75}, {0x62, 0x76}, {0x64, 0x77},
        {0x65, 0x78}, {0x6D, 0x79}, {0x67, 0x7A}, {0x6F, 0x7B},
    };
  // clang-format on

  auto it = kTable.find(macKey);
  return (it != kTable.end()) ? it->second : 0u;
}

unsigned int mapModifiers(NSEventModifierFlags flags) {
  using namespace vigine::platform;
  unsigned int mods = KeyModifierNone;
  if (flags & NSEventModifierFlagShift)
    mods |= KeyModifierShift;
  if (flags & NSEventModifierFlagControl)
    mods |= KeyModifierControl;
  if (flags & NSEventModifierFlagOption)
    mods |= KeyModifierAlt;
  if (flags & NSEventModifierFlagCommand)
    mods |= KeyModifierSuper;
  if (flags & NSEventModifierFlagCapsLock)
    mods |= KeyModifierCaps;
  return mods;
}

} // namespace

// ---------------------------------------------------------------------------
// Objective-C classes
// ---------------------------------------------------------------------------

@interface VigineWindowDelegate : NSObject <NSWindowDelegate> {
  vigine::platform::CocoaWindowComponent *_component;
}
- (instancetype)initWithComponent:
    (vigine::platform::CocoaWindowComponent *)component;
@end

@implementation VigineWindowDelegate

- (instancetype)initWithComponent:
    (vigine::platform::CocoaWindowComponent *)component {
  self = [super init];
  if (self)
    _component = component;
  return self;
}

- (BOOL)windowShouldClose:(NSWindow *)__unused sender {
  return YES;
}

- (void)windowWillClose:(NSNotification *)__unused notification {
  if (_component)
    _component->handleWindowClose();
}

- (void)windowDidResize:(NSNotification *)notification {
  NSWindow *window = notification.object;
  NSRect frame = [window.contentView bounds];
  const int w = static_cast<int>(frame.size.width);
  const int h = static_cast<int>(frame.size.height);
  if (_component)
    _component->handleWindowResize(w, h);
}

@end

// ---------------------------------------------------------------------------
// Pimpl definition
// ---------------------------------------------------------------------------

struct vigine::platform::CocoaWindowComponent::Impl {
  NSWindow *window = nil;
  CAMetalLayer *metalLayer = nil;
  VigineWindowDelegate *delegate = nil;
  bool shouldClose = false;
};

// ---------------------------------------------------------------------------
// CocoaWindowComponent implementation
// ---------------------------------------------------------------------------

vigine::platform::CocoaWindowComponent::CocoaWindowComponent()
    : _impl(std::make_unique<Impl>()) {}

vigine::platform::CocoaWindowComponent::~CocoaWindowComponent() {
  @autoreleasepool {
    if (_impl->delegate) {
      [_impl->window setDelegate:nil];
      _impl->delegate = nil;
    }
    if (_impl->window) {
      [_impl->window close];
      _impl->window = nil;
    }
  }
}

void *vigine::platform::CocoaWindowComponent::nativeHandle() const {
  auto *self = const_cast<CocoaWindowComponent *>(this);
  if (!self->ensureWindowCreated())
    return nullptr;

  return (__bridge void *)_impl->metalLayer;
}

bool vigine::platform::CocoaWindowComponent::ensureWindowCreated() {
  if (_impl->window && _impl->metalLayer)
    return true;

  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

  const NSRect contentRect = NSMakeRect(0, 0, 940, 660);
  NSWindowStyleMask style =
      NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

  _impl->window = [[NSWindow alloc] initWithContentRect:contentRect
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
  if (!_impl->window)
    return false;

  [_impl->window setTitle:@"Vigine Window"];
  [_impl->window center];

  _impl->delegate = [[VigineWindowDelegate alloc] initWithComponent:this];
  [_impl->window setDelegate:_impl->delegate];

  NSView *contentView = [_impl->window contentView];
  [contentView setWantsLayer:YES];
  _impl->metalLayer = [CAMetalLayer layer];
  _impl->metalLayer.device = MTLCreateSystemDefaultDevice();
  _impl->metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  _impl->metalLayer.drawableSize =
      CGSizeMake(contentRect.size.width, contentRect.size.height);
  [contentView setLayer:_impl->metalLayer];

  return _impl->metalLayer != nil;
}

void vigine::platform::CocoaWindowComponent::handleWindowClose() {
  _impl->shouldClose = true;
  if (_eventHandler)
    _eventHandler->onWindowClosed();
}

void vigine::platform::CocoaWindowComponent::handleWindowResize(int width,
                                                                int height) {
  if (_eventHandler)
    _eventHandler->onWindowResized(width, height);

  // Resize the Metal layer to match the new content size.
  if (_impl->metalLayer)
    _impl->metalLayer.drawableSize = CGSizeMake(width, height);
}

void vigine::platform::CocoaWindowComponent::runFrame() {
  runFrameCallback();

  // Update FPS in title bar (~every 500 ms)
  ++_fpsFrameCount;
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = now - _fpsSampleStart;
  if (elapsed >= std::chrono::milliseconds(500)) {
    const double elapsedMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    const double fps = static_cast<double>(_fpsFrameCount) * 1000.0 / elapsedMs;
    const double frameMs =
        elapsedMs / static_cast<double>(std::max(_fpsFrameCount, 1u));

    std::array<char, 128> title{};
    std::snprintf(title.data(), title.size(),
                  "Vigine Window | FPS: %.1f (%.2f ms)", fps, frameMs);

    if (_impl->window) {
      NSString *nsTitle = [NSString stringWithUTF8String:title.data()];
      dispatch_async(dispatch_get_main_queue(), ^{
        [_impl->window setTitle:nsTitle];
      });
    }

    _fpsSampleStart = now;
    _fpsFrameCount = 0;
  }
}

void vigine::platform::CocoaWindowComponent::show() {
  @autoreleasepool {
    if (!ensureWindowCreated())
      return;

    // ---- Show window ----
    [_impl->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp finishLaunching];

    _fpsSampleStart = std::chrono::steady_clock::now();
    _fpsFrameCount = 0;
    _impl->shouldClose = false;

    constexpr auto kTargetFrameTime = std::chrono::nanoseconds(6944444);
    auto nextFrameTime = std::chrono::steady_clock::now();

    while (!_impl->shouldClose) {
      @autoreleasepool {
        const auto deadline = nextFrameTime + kTargetFrameTime;
        const auto now = std::chrono::steady_clock::now();
        const auto waitDuration = deadline - now;
        NSDate *untilDate =
            waitDuration > std::chrono::nanoseconds(0)
                ? [NSDate dateWithTimeIntervalSinceNow:std::chrono::duration<
                                                           double>(waitDuration)
                                                           .count()]
                : [NSDate distantPast];

        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:untilDate
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];

        if (event) {
          const NSEventType type = event.type;
          const NSRect contentBounds = [_impl->window.contentView bounds];

          if (_eventHandler) {
            if (type == NSEventTypeKeyDown || type == NSEventTypeKeyUp) {
              vigine::platform::KeyEvent keyEvent{};
              keyEvent.keyCode = mapMacKeyCode(event.keyCode);
              keyEvent.scanCode = static_cast<unsigned int>(event.keyCode);
              keyEvent.modifiers = mapModifiers(event.modifierFlags);
              keyEvent.isRepeat =
                  (type == NSEventTypeKeyDown) && event.isARepeat;
              keyEvent.repeatCount = keyEvent.isRepeat ? 1u : 0u;

              if (type == NSEventTypeKeyDown)
                _eventHandler->onKeyDown(keyEvent);
              else
                _eventHandler->onKeyUp(keyEvent);

              if (type == NSEventTypeKeyDown && event.characters.length > 0) {
                const uint32_t cp = [event.characters characterAtIndex:0];
                if (cp >= 0x20 && cp != 0x7F) {
                  vigine::platform::TextEvent textEvent{};
                  textEvent.codePoint = cp;
                  textEvent.modifiers = keyEvent.modifiers;
                  textEvent.repeatCount = keyEvent.repeatCount;
                  _eventHandler->onChar(textEvent);
                }
              }
            } else if (type == NSEventTypeMouseMoved ||
                       type == NSEventTypeLeftMouseDragged ||
                       type == NSEventTypeRightMouseDragged ||
                       type == NSEventTypeOtherMouseDragged) {
              const NSPoint loc =
                  [_impl->window mouseLocationOutsideOfEventStream];
              _eventHandler->onMouseMove(
                  static_cast<int>(loc.x),
                  static_cast<int>(contentBounds.size.height - loc.y));
            } else if (type == NSEventTypeLeftMouseDown) {
              const NSPoint loc = event.locationInWindow;
              _eventHandler->onMouseButtonDown(
                  vigine::platform::MouseButton::Left, static_cast<int>(loc.x),
                  static_cast<int>(contentBounds.size.height - loc.y));
            } else if (type == NSEventTypeLeftMouseUp) {
              const NSPoint loc = event.locationInWindow;
              _eventHandler->onMouseButtonUp(
                  vigine::platform::MouseButton::Left, static_cast<int>(loc.x),
                  static_cast<int>(contentBounds.size.height - loc.y));
            } else if (type == NSEventTypeRightMouseDown) {
              const NSPoint loc = event.locationInWindow;
              _eventHandler->onMouseButtonDown(
                  vigine::platform::MouseButton::Right, static_cast<int>(loc.x),
                  static_cast<int>(contentBounds.size.height - loc.y));
            } else if (type == NSEventTypeRightMouseUp) {
              const NSPoint loc = event.locationInWindow;
              _eventHandler->onMouseButtonUp(
                  vigine::platform::MouseButton::Right, static_cast<int>(loc.x),
                  static_cast<int>(contentBounds.size.height - loc.y));
            } else if (type == NSEventTypeScrollWheel) {
              const NSPoint loc = event.locationInWindow;
              _eventHandler->onMouseWheel(
                  static_cast<int>(event.scrollingDeltaY * 120.0),
                  static_cast<int>(loc.x),
                  static_cast<int>(contentBounds.size.height - loc.y));
            }
          }

          [NSApp sendEvent:event];
        }

        if (_impl->shouldClose)
          break;

        runFrame();
        nextFrameTime = deadline;
      }
    }
  }
}

#endif // __APPLE__
