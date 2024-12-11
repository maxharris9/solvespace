//-----------------------------------------------------------------------------
// Platform-dependent GUI functionality that has only minor differences.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {
  namespace Platform {

    //-----------------------------------------------------------------------------
    // Keyboard events
    //-----------------------------------------------------------------------------

    std::string AcceleratorDescription (const KeyboardEvent &accel) {
      std::string label;
      if (accel.controlDown) {
#ifdef __APPLE__
        label += "⌘+";
#else
        label += "Ctrl+";
#endif
      }

      if (accel.shiftDown) {
        label += "Shift+";
      }

      switch (accel.key) {
      case KeyboardEvent::Key::FUNCTION: label += ssprintf ("F%d", accel.num); break;

      case KeyboardEvent::Key::CHARACTER:
        if (accel.chr == '\t') {
          label += "Tab";
        } else if (accel.chr == ' ') {
          label += "Space";
        } else if (accel.chr == '\x1b') {
          label += "Esc";
        } else if (accel.chr == '\x7f') {
          label += "Del";
        } else if (accel.chr != 0) {
          label += toupper ((char)(accel.chr & 0xff));
        }
        break;
      }

      return label;
    }

    //-----------------------------------------------------------------------------
    // Settings
    //-----------------------------------------------------------------------------

    void Settings::FreezeBool (const std::string &key, bool value) {
      FreezeInt (key, (int)value);
    }

    bool Settings::ThawBool (const std::string &key, bool defaultValue) {
      return ThawInt (key, (int)defaultValue) != 0;
    }

    void Settings::FreezeColor (const std::string &key, RgbaColor value) {
      FreezeInt (key, value.ToPackedInt ());
    }

    RgbaColor Settings::ThawColor (const std::string &key, RgbaColor defaultValue) {
      return RgbaColor::FromPackedInt (ThawInt (key, defaultValue.ToPackedInt ()));
    }
  } // namespace Platform
} // namespace SolveSpace
