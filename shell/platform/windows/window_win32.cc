// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/window_win32.h"

#include <imm.h>

#include <cstring>

#include "dpi_utils_win32.h"

namespace flutter {

namespace {

static constexpr int32_t kDefaultPointerDeviceId = 0;

// This method is only valid during a window message related to mouse/touch
// input.
// See
// https://docs.microsoft.com/en-us/windows/win32/tablet/system-events-and-mouse-messages?redirectedfrom=MSDN#distinguishing-pen-input-from-mouse-and-touch.
static FlutterPointerDeviceKind GetFlutterPointerDeviceKind() {
  constexpr LPARAM kTouchOrPenSignature = 0xFF515700;
  constexpr LPARAM kTouchSignature = kTouchOrPenSignature | 0x80;
  constexpr LPARAM kSignatureMask = 0xFFFFFF00;
  LPARAM info = GetMessageExtraInfo();
  if ((info & kSignatureMask) == kTouchOrPenSignature) {
    if ((info & kTouchSignature) == kTouchSignature) {
      return kFlutterPointerDeviceKindTouch;
    }
    return kFlutterPointerDeviceKindStylus;
  }
  return kFlutterPointerDeviceKindMouse;
}

char32_t CodePointFromSurrogatePair(wchar_t high, wchar_t low) {
  return 0x10000 + ((static_cast<char32_t>(high) & 0x000003FF) << 10) +
         (low & 0x3FF);
}

static const int kMinTouchDeviceId = 0;
static const int kMaxTouchDeviceId = 128;

}  // namespace

WindowWin32::WindowWin32() : WindowWin32(nullptr) {}

WindowWin32::WindowWin32(
    std::unique_ptr<TextInputManagerWin32> text_input_manager)
    : touch_id_generator_(kMinTouchDeviceId, kMaxTouchDeviceId),
      text_input_manager_(std::move(text_input_manager)) {
  // Get the DPI of the primary monitor as the initial DPI. If Per-Monitor V2 is
  // supported, |current_dpi_| should be updated in the
  // kWmDpiChangedBeforeParent message.
  current_dpi_ = GetDpiForHWND(nullptr);
  if (text_input_manager_ == nullptr) {
    text_input_manager_ = std::make_unique<TextInputManagerWin32>();
  }
}

WindowWin32::~WindowWin32() {
  Destroy();
}

void WindowWin32::InitializeChild(const char* title,
                                  unsigned int width,
                                  unsigned int height) {
  Destroy();
  std::wstring converted_title = NarrowToWide(title);

  WNDCLASS window_class = RegisterWindowClass(converted_title);

  auto* result = CreateWindowEx(
      0, window_class.lpszClassName, converted_title.c_str(),
      WS_CHILD | WS_VISIBLE, CW_DEFAULT, CW_DEFAULT, width, height,
      HWND_MESSAGE, nullptr, window_class.hInstance, this);

  if (result == nullptr) {
    auto error = GetLastError();
    LPWSTR message = nullptr;
    size_t size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&message), 0, NULL);
    OutputDebugString(message);
    LocalFree(message);
  }
}

std::wstring WindowWin32::NarrowToWide(const char* source) {
  size_t length = strlen(source);
  size_t outlen = 0;
  std::wstring wideTitle(length, L'#');
  mbstowcs_s(&outlen, &wideTitle[0], length + 1, source, length);
  return wideTitle;
}

WNDCLASS WindowWin32::RegisterWindowClass(std::wstring& title) {
  window_class_name_ = title;

  WNDCLASS window_class{};
  window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
  window_class.lpszClassName = title.c_str();
  window_class.style = CS_HREDRAW | CS_VREDRAW;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = GetModuleHandle(nullptr);
  window_class.hIcon = nullptr;
  window_class.hbrBackground = 0;
  window_class.lpszMenuName = nullptr;
  window_class.lpfnWndProc = WndProc;
  RegisterClass(&window_class);
  return window_class;
}

LRESULT CALLBACK WindowWin32::WndProc(HWND const window,
                                      UINT const message,
                                      WPARAM const wparam,
                                      LPARAM const lparam) noexcept {
  if (message == WM_NCCREATE) {
    auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
    SetWindowLongPtr(window, GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(cs->lpCreateParams));

    auto that = static_cast<WindowWin32*>(cs->lpCreateParams);
    that->window_handle_ = window;
    that->text_input_manager_->SetWindowHandle(window);
    RegisterTouchWindow(window, 0);
  } else if (WindowWin32* that = GetThisFromHandle(window)) {
    return that->HandleMessage(message, wparam, lparam);
  }

  return DefWindowProc(window, message, wparam, lparam);
}

void WindowWin32::TrackMouseLeaveEvent(HWND hwnd) {
  if (!tracking_mouse_leave_) {
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.hwndTrack = hwnd;
    tme.dwFlags = TME_LEAVE;
    TrackMouseEvent(&tme);
    tracking_mouse_leave_ = true;
  }
}

void WindowWin32::OnGetObject(UINT const message,
                              WPARAM const wparam,
                              LPARAM const lparam) {
  LRESULT reference_result = static_cast<LRESULT>(0L);

  // Only the lower 32 bits of lparam are valid when checking the object id
  // because it sometimes gets sign-extended incorrectly (but not always).
  DWORD obj_id = static_cast<DWORD>(static_cast<DWORD_PTR>(lparam));

  bool is_msaa_request = static_cast<DWORD>(OBJID_CLIENT) == obj_id;
  if (is_msaa_request) {
    // On Windows, we don't get a notification that the screen reader has been
    // enabled or disabled. There is an API to query for screen reader state,
    // but that state isn't set by all screen readers, including by Narrator,
    // the screen reader that ships with Windows:
    // https://docs.microsoft.com/en-us/windows/win32/winauto/screen-reader-parameter
    //
    // Instead, we enable semantics in Flutter if Windows issues queries for
    // Microsoft Active Accessibility (MSAA) COM objects.
    OnUpdateSemanticsEnabled(true);

    // TODO(cbracken): https://github.com/flutter/flutter/issues/77838
    // Once AccessibilityBridge is wired up, look up the IAccessible
    // representing the root view and call LresultFromObject.
  }
}

void WindowWin32::OnImeSetContext(UINT const message,
                                  WPARAM const wparam,
                                  LPARAM const lparam) {
  if (wparam != 0) {
    text_input_manager_->CreateImeWindow();
  }
}

void WindowWin32::OnImeStartComposition(UINT const message,
                                        WPARAM const wparam,
                                        LPARAM const lparam) {
  text_input_manager_->CreateImeWindow();
  OnComposeBegin();
}

void WindowWin32::OnImeComposition(UINT const message,
                                   WPARAM const wparam,
                                   LPARAM const lparam) {
  // Update the IME window position.
  text_input_manager_->UpdateImeWindow();

  if (lparam & GCS_COMPSTR) {
    // Read the in-progress composing string.
    long pos = text_input_manager_->GetComposingCursorPosition();
    std::optional<std::u16string> text =
        text_input_manager_->GetComposingString();
    if (text) {
      OnComposeChange(text.value(), pos);
    }
  }
  if (lparam & GCS_RESULTSTR) {
    // Commit but don't end composing.
    // Read the committed composing string.
    long pos = text_input_manager_->GetComposingCursorPosition();
    std::optional<std::u16string> text = text_input_manager_->GetResultString();
    if (text) {
      OnComposeChange(text.value(), pos);
      OnComposeCommit();
    }
  }
}

void WindowWin32::OnImeEndComposition(UINT const message,
                                      WPARAM const wparam,
                                      LPARAM const lparam) {
  text_input_manager_->DestroyImeWindow();
  OnComposeEnd();
}

void WindowWin32::OnImeRequest(UINT const message,
                               WPARAM const wparam,
                               LPARAM const lparam) {
  // TODO(cbracken): Handle IMR_RECONVERTSTRING, IMR_DOCUMENTFEED,
  // and IMR_QUERYCHARPOSITION messages.
  // https://github.com/flutter/flutter/issues/74547
}

void WindowWin32::AbortImeComposing() {
  text_input_manager_->AbortComposing();
}

void WindowWin32::UpdateCursorRect(const Rect& rect) {
  text_input_manager_->UpdateCaretRect(rect);
}

static uint16_t ResolveKeyCode(uint16_t original,
                               bool extended,
                               uint8_t scancode) {
  switch (original) {
    case VK_SHIFT:
    case VK_LSHIFT:
      return MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
    case VK_MENU:
    case VK_LMENU:
      return extended ? VK_RMENU : VK_LMENU;
    case VK_CONTROL:
    case VK_LCONTROL:
      return extended ? VK_RCONTROL : VK_LCONTROL;
    default:
      return original;
  }
}

static bool IsPrintable(uint32_t c) {
  constexpr char32_t kMinPrintable = ' ';
  constexpr char32_t kDelete = 0x7F;
  return c >= kMinPrintable && c != kDelete;
}

LRESULT
WindowWin32::HandleMessage(UINT const message,
                           WPARAM const wparam,
                           LPARAM const lparam) noexcept {
  LPARAM result_lparam = lparam;
  int xPos = 0, yPos = 0;
  UINT width = 0, height = 0;
  UINT button_pressed = 0;
  FlutterPointerDeviceKind device_kind;

  switch (message) {
    case kWmDpiChangedBeforeParent:
      current_dpi_ = GetDpiForHWND(window_handle_);
      OnDpiScale(current_dpi_);
      return 0;
    case WM_SIZE:
      width = LOWORD(lparam);
      height = HIWORD(lparam);

      current_width_ = width;
      current_height_ = height;
      HandleResize(width, height);
      break;
    case WM_TOUCH: {
      UINT num_points = LOWORD(wparam);
      touch_points_.resize(num_points);
      auto touch_input_handle = reinterpret_cast<HTOUCHINPUT>(lparam);
      if (GetTouchInputInfo(touch_input_handle, num_points,
                            touch_points_.data(), sizeof(TOUCHINPUT))) {
        for (const auto& touch : touch_points_) {
          // Generate a mapped ID for the Windows-provided touch ID
          auto touch_id = touch_id_generator_.GetGeneratedId(touch.dwID);

          POINT pt = {TOUCH_COORD_TO_PIXEL(touch.x),
                      TOUCH_COORD_TO_PIXEL(touch.y)};
          ScreenToClient(window_handle_, &pt);
          auto x = static_cast<double>(pt.x);
          auto y = static_cast<double>(pt.y);

          if (touch.dwFlags & TOUCHEVENTF_DOWN) {
            OnPointerDown(x, y, kFlutterPointerDeviceKindTouch, touch_id,
                          WM_LBUTTONDOWN);
          } else if (touch.dwFlags & TOUCHEVENTF_MOVE) {
            OnPointerMove(x, y, kFlutterPointerDeviceKindTouch, touch_id);
          } else if (touch.dwFlags & TOUCHEVENTF_UP) {
            OnPointerUp(x, y, kFlutterPointerDeviceKindTouch, touch_id,
                        WM_LBUTTONDOWN);
            OnPointerLeave(kFlutterPointerDeviceKindTouch, touch_id);
            touch_id_generator_.ReleaseNumber(touch.dwID);
          }
        }
        CloseTouchInputHandle(touch_input_handle);
      }
      return 0;
    }
    case WM_MOUSEMOVE:
      device_kind = GetFlutterPointerDeviceKind();
      if (device_kind == kFlutterPointerDeviceKindMouse) {
        TrackMouseLeaveEvent(window_handle_);

        xPos = GET_X_LPARAM(lparam);
        yPos = GET_Y_LPARAM(lparam);

        OnPointerMove(static_cast<double>(xPos), static_cast<double>(yPos),
                      device_kind, kDefaultPointerDeviceId);
      }
      break;
    case WM_MOUSELEAVE:
      device_kind = GetFlutterPointerDeviceKind();
      if (device_kind == kFlutterPointerDeviceKindMouse) {
        OnPointerLeave(device_kind, kDefaultPointerDeviceId);
      }

      // Once the tracked event is received, the TrackMouseEvent function
      // resets. Set to false to make sure it's called once mouse movement is
      // detected again.
      tracking_mouse_leave_ = false;
      break;
    case WM_SETCURSOR: {
      UINT hit_test_result = LOWORD(lparam);
      if (hit_test_result == HTCLIENT) {
        OnSetCursor();
        return TRUE;
      }
      break;
    }
    case WM_SETFOCUS:
      ::CreateCaret(window_handle_, nullptr, 1, 1);
      break;
    case WM_KILLFOCUS:
      ::DestroyCaret();
      break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
      device_kind = GetFlutterPointerDeviceKind();
      if (device_kind != kFlutterPointerDeviceKindMouse) {
        break;
      }

      if (message == WM_LBUTTONDOWN) {
        // Capture the pointer in case the user drags outside the client area.
        // In this case, the "mouse leave" event is delayed until the user
        // releases the button. It's only activated on left click given that
        // it's more common for apps to handle dragging with only the left
        // button.
        SetCapture(window_handle_);
      }
      button_pressed = message;
      if (message == WM_XBUTTONDOWN) {
        button_pressed = GET_XBUTTON_WPARAM(wparam);
      }
      xPos = GET_X_LPARAM(lparam);
      yPos = GET_Y_LPARAM(lparam);
      OnPointerDown(static_cast<double>(xPos), static_cast<double>(yPos),
                    device_kind, kDefaultPointerDeviceId, button_pressed);
      break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
      device_kind = GetFlutterPointerDeviceKind();
      if (device_kind != kFlutterPointerDeviceKindMouse) {
        break;
      }

      if (message == WM_LBUTTONUP) {
        ReleaseCapture();
      }
      button_pressed = message;
      if (message == WM_XBUTTONUP) {
        button_pressed = GET_XBUTTON_WPARAM(wparam);
      }
      xPos = GET_X_LPARAM(lparam);
      yPos = GET_Y_LPARAM(lparam);
      OnPointerUp(static_cast<double>(xPos), static_cast<double>(yPos),
                  device_kind, kDefaultPointerDeviceId, button_pressed);
      break;
    case WM_MOUSEWHEEL:
      OnScroll(0.0,
               -(static_cast<short>(HIWORD(wparam)) /
                 static_cast<double>(WHEEL_DELTA)),
               kFlutterPointerDeviceKindMouse, kDefaultPointerDeviceId);
      break;
    case WM_MOUSEHWHEEL:
      OnScroll((static_cast<short>(HIWORD(wparam)) /
                static_cast<double>(WHEEL_DELTA)),
               0.0, kFlutterPointerDeviceKindMouse, kDefaultPointerDeviceId);
      break;
    case WM_GETOBJECT:
      OnGetObject(message, wparam, lparam);
      break;
    case WM_INPUTLANGCHANGE:
      // TODO(cbracken): pass this to TextInputManager to aid with
      // language-specific issues.
      break;
    case WM_IME_SETCONTEXT:
      OnImeSetContext(message, wparam, lparam);
      // Strip the ISC_SHOWUICOMPOSITIONWINDOW bit from lparam before passing it
      // to DefWindowProc() so that the composition window is hidden since
      // Flutter renders the composing string itself.
      result_lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
      break;
    case WM_IME_STARTCOMPOSITION:
      OnImeStartComposition(message, wparam, lparam);
      // Suppress further processing by DefWindowProc() so that the default
      // system IME style isn't used, but rather the one set in the
      // WM_IME_SETCONTEXT handler.
      return TRUE;
    case WM_IME_COMPOSITION:
      OnImeComposition(message, wparam, lparam);
      if (lparam & GCS_RESULTSTR || lparam & GCS_COMPSTR) {
        // Suppress further processing by DefWindowProc() since otherwise it
        // will emit the result string as WM_CHAR messages on commit. Instead,
        // committing the composing text to the EditableText string is handled
        // in TextInputModel::CommitComposing, triggered by
        // OnImeEndComposition().
        return TRUE;
      }
      break;
    case WM_IME_ENDCOMPOSITION:
      OnImeEndComposition(message, wparam, lparam);
      return TRUE;
    case WM_IME_REQUEST:
      OnImeRequest(message, wparam, lparam);
      break;
    case WM_UNICHAR: {
      // Tell third-pary app, we can support Unicode.
      if (wparam == UNICODE_NOCHAR)
        return TRUE;
      // DefWindowProc will send WM_CHAR for this WM_UNICHAR.
      break;
    }
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
    case WM_CHAR:
    case WM_SYSCHAR: {
      static wchar_t s_pending_high_surrogate = 0;

      wchar_t character = static_cast<wchar_t>(wparam);
      std::u16string text({character});
      char32_t code_point = character;
      if (IS_HIGH_SURROGATE(character)) {
        // Save to send later with the trailing surrogate.
        s_pending_high_surrogate = character;
      } else if (IS_LOW_SURROGATE(character) && s_pending_high_surrogate != 0) {
        text.insert(text.begin(), s_pending_high_surrogate);
        // Merge the surrogate pairs for the key event.
        code_point =
            CodePointFromSurrogatePair(s_pending_high_surrogate, character);
        s_pending_high_surrogate = 0;
      }

      const unsigned int scancode = (lparam >> 16) & 0xff;

      // All key presses that generate a character should be sent from
      // WM_CHAR. In order to send the full key press information, the keycode
      // is persisted in keycode_for_char_message_ obtained from WM_KEYDOWN.
      //
      // A high surrogate is always followed by a low surrogate, while a
      // non-surrogate character always appears alone. Filter out high
      // surrogates so that it's the low surrogate message that triggers
      // the onKey, asks if the framework handles it (which can only be done
      // once), and calls OnText during the redispatched messages.
      if (keycode_for_char_message_ != 0 && !IS_HIGH_SURROGATE(character)) {
        const bool extended = ((lparam >> 24) & 0x01) == 0x01;
        const bool was_down = lparam & 0x40000000;
        // Certain key combinations yield control characters as WM_CHAR's
        // lParam. For example, 0x01 for Ctrl-A. Filter these characters.
        // See
        // https://docs.microsoft.com/en-us/windows/win32/learnwin32/accelerator-tables
        const char32_t event_character =
            (message == WM_DEADCHAR || message == WM_SYSDEADCHAR)
                ? Win32MapVkToChar(keycode_for_char_message_)
            : IsPrintable(code_point) ? code_point
                                      : 0;
        bool handled = OnKey(keycode_for_char_message_, scancode,
                             message == WM_SYSCHAR ? WM_SYSKEYDOWN : WM_KEYDOWN,
                             event_character, extended, was_down);
        keycode_for_char_message_ = 0;
        if (handled) {
          // If the OnKey handler handles the message, then return so we don't
          // pass it to OnText, because handling the message indicates that
          // OnKey either just sent it to the framework to be processed.
          //
          // This message will be redispatched if not handled by the framework,
          // during which the OnText (below) might be reached. However, if the
          // original message was preceded by dead chars (such as ^ and e
          // yielding ê), then since the redispatched message is no longer
          // preceded by the dead char, the text will be wrong. Therefore we
          // record the text here for the redispached event to use.
          if (message == WM_CHAR) {
            text_for_scancode_on_redispatch_[scancode] = text;
          }

          // For system characters, always pass them to the default WndProc so
          // that system keys like the ALT-TAB are processed correctly.
          if (message == WM_SYSCHAR) {
            break;
          }
          return 0;
        }
      }

      // Of the messages handled here, only WM_CHAR should be treated as
      // characters. WM_SYS*CHAR are not part of text input, and WM_DEADCHAR
      // will be incorporated into a later WM_CHAR with the full character.
      // Also filter out:
      // - Lead surrogates, which like dead keys will be send once combined.
      // - ASCII control characters, which are sent as WM_CHAR events for all
      //   control key shortcuts.
      if (message == WM_CHAR && s_pending_high_surrogate == 0 &&
          IsPrintable(character)) {
        auto found_text_iter = text_for_scancode_on_redispatch_.find(scancode);
        if (found_text_iter != text_for_scancode_on_redispatch_.end()) {
          text = found_text_iter->second;
          text_for_scancode_on_redispatch_.erase(found_text_iter);
        }
        OnText(text);
      }
      return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
      const bool is_keydown_message =
          (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
      // Check if this key produces a character. If so, the key press should
      // be sent with the character produced at WM_CHAR. Store the produced
      // keycode (it's not accessible from WM_CHAR) to be used in WM_CHAR.
      //
      // Messages with Control or Win modifiers down are never considered as
      // character messages. This allows key combinations such as "CTRL + Digit"
      // to properly produce key down events even though `MapVirtualKey` returns
      // a valid character. See https://github.com/flutter/flutter/issues/85587.
      unsigned int character = Win32MapVkToChar(wparam);
      UINT next_key_message = PeekNextMessageType(WM_KEYFIRST, WM_KEYLAST);
      bool has_wm_char =
          (next_key_message == WM_DEADCHAR ||
           next_key_message == WM_SYSDEADCHAR || next_key_message == WM_CHAR ||
           next_key_message == WM_SYSCHAR);
      if (character > 0 && is_keydown_message && has_wm_char) {
        keycode_for_char_message_ = wparam;
        return 0;
      }
      unsigned int keyCode(wparam);
      const uint8_t scancode = (lparam >> 16) & 0xff;
      const bool extended = ((lparam >> 24) & 0x01) == 0x01;
      // If the key is a modifier, get its side.
      keyCode = ResolveKeyCode(keyCode, extended, scancode);
      const bool was_down = lparam & 0x40000000;
      bool is_syskey = message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
      const int action = is_keydown_message
                             ? (is_syskey ? WM_SYSKEYDOWN : WM_KEYDOWN)
                             : (is_syskey ? WM_SYSKEYUP : WM_KEYUP);
      if (OnKey(keyCode, scancode, action, 0, extended, was_down)) {
        // For system keys, always pass them to the default WndProc so that keys
        // like the ALT-TAB or Kanji switches are processed correctly.
        if (is_syskey) {
          break;
        }
        return 0;
      }
      break;
  }

  return Win32DefWindowProc(window_handle_, message, wparam, result_lparam);
}

UINT WindowWin32::GetCurrentDPI() {
  return current_dpi_;
}

UINT WindowWin32::GetCurrentWidth() {
  return current_width_;
}

UINT WindowWin32::GetCurrentHeight() {
  return current_height_;
}

HWND WindowWin32::GetWindowHandle() {
  return window_handle_;
}

void WindowWin32::Destroy() {
  if (window_handle_) {
    text_input_manager_->SetWindowHandle(nullptr);
    DestroyWindow(window_handle_);
    window_handle_ = nullptr;
  }

  UnregisterClass(window_class_name_.c_str(), nullptr);
}

void WindowWin32::HandleResize(UINT width, UINT height) {
  current_width_ = width;
  current_height_ = height;
  OnResize(width, height);
}

UINT WindowWin32::PeekNextMessageType(UINT wMsgFilterMin, UINT wMsgFilterMax) {
  MSG next_message;
  BOOL has_msg = Win32PeekMessage(&next_message, window_handle_, wMsgFilterMin,
                                  wMsgFilterMax, PM_NOREMOVE);
  if (!has_msg) {
    return 0;
  }
  return next_message.message;
}

WindowWin32* WindowWin32::GetThisFromHandle(HWND const window) noexcept {
  return reinterpret_cast<WindowWin32*>(
      GetWindowLongPtr(window, GWLP_USERDATA));
}

LRESULT WindowWin32::Win32DefWindowProc(HWND hWnd,
                                        UINT Msg,
                                        WPARAM wParam,
                                        LPARAM lParam) {
  return DefWindowProc(hWnd, Msg, wParam, lParam);
}

BOOL WindowWin32::Win32PeekMessage(LPMSG lpMsg,
                                   HWND hWnd,
                                   UINT wMsgFilterMin,
                                   UINT wMsgFilterMax,
                                   UINT wRemoveMsg) {
  return PeekMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

uint32_t WindowWin32::Win32MapVkToChar(uint32_t virtual_key) {
  return MapVirtualKey(virtual_key, MAPVK_VK_TO_CHAR);
}

}  // namespace flutter
