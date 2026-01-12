# raylib iOS Example

This example demonstrates raylib running on iOS using [MetalANGLE](https://github.com/nicbarker/nicbarker/MetalANGLE) to translate OpenGL ES to Metal.

## Quick Start

### Prerequisites

1. **Xcode** (14.0 or later)
2. **XcodeGen** - Install with: `brew install xcodegen`

### Setup

1. **Download MetalANGLE frameworks:**
   ```bash
   mkdir -p frameworks/simulator frameworks/device

   # Download frameworks
   curl -L -o frameworks/simulator.zip \
     https://github.com/nicbarker/nicbarker/MetalANGLE/releases/download/v0.0.8/nicbarker/MetalANGLE.framework.ios.simulator.zip
   curl -L -o frameworks/device.zip \
     https://github.com/nicbarker/nicbarker/MetalANGLE/releases/download/v0.0.8/nicbarker/MetalANGLE.framework.ios.zip

   # Extract
   unzip -o frameworks/simulator.zip -d frameworks/simulator/
   unzip -o frameworks/device.zip -d frameworks/device/
   rm frameworks/*.zip
   ```

2. **Generate Xcode project:**
   ```bash
   xcodegen generate
   ```

3. **Build and run:**
   ```bash
   # Simulator
   xcodebuild -project RaylibIOSExample.xcodeproj \
     -scheme RaylibIOSExample \
     -destination 'generic/platform=iOS Simulator' build

   # Or open in Xcode
   open RaylibIOSExample.xcodeproj
   ```

## Project Structure

```
examples/ios/
├── AppDelegate.m      # iOS app delegate with UIKit integration
├── game.c             # Example game using raylib API
├── project.yml        # XcodeGen project specification
├── Info.plist         # iOS app configuration
└── frameworks/        # MetalANGLE frameworks (not in git)
    ├── simulator/     # For iOS Simulator
    └── device/        # For real devices
```

## How It Works

raylib's iOS platform layer (`src/platforms/rcore_ios.c`) uses:
- **MetalANGLE** for OpenGL ES 3.0 rendering (translated to Metal)
- **UIKit** for window management and touch input
- **CADisplayLink** for frame timing

The platform layer is compiled as Objective-C via `src/rcore_objc.m` which includes `rcore.c`.

## iOS-Specific Code Patterns

### Callback-Based Rendering

iOS doesn't allow traditional game loops. Use `SetUpdateCallback`:

```c
// Instead of this (desktop):
while (!WindowShouldClose()) {
    BeginDrawing();
    // draw
    EndDrawing();
}

// Use this (iOS):
void GameUpdate(void) {
    BeginDrawing();
    // draw
    EndDrawing();
}

void GameInit(void) {
    InitWindow(0, 0, "My Game");
    SetUpdateCallback(GameUpdate);
}
```

### Touch Input

Touch is mapped to mouse events:

```c
if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Vector2 pos = GetMousePosition();
    // Handle tap
}

// Multi-touch
int count = GetTouchPointCount();
for (int i = 0; i < count; i++) {
    Vector2 pos = GetTouchPosition(i);
}
```

### Screen Size

Always fullscreen - InitWindow size is ignored:

```c
InitWindow(0, 0, "My Game");  // Size parameters don't matter
int w = GetScreenWidth();      // Get actual screen size
int h = GetScreenHeight();
```

## Building for Device

The project uses the simulator framework by default. To build for a real device:

1. Edit `project.yml`:
   - Change `frameworks/simulator` to `frameworks/device` in `FRAMEWORK_SEARCH_PATHS`
   - Change the dependency path to `frameworks/device/MetalANGLE.framework`

2. Regenerate: `xcodegen generate`

3. In Xcode, select your device and set a valid signing team.

## Current Limitations

- **Audio disabled** - needs Objective-C wrapper for miniaudio
- **No gamepad support** - would need GCController integration
- **No keyboard support** - would need UIKeyInput implementation

## Troubleshooting

### Black Screen
- Check Xcode console for EGL errors
- Verify MetalANGLE.framework is embedded (not just linked)

### Touch Not Working
- Verify RaylibView has `userInteractionEnabled = YES`

### Linker Errors
- Ensure `-ObjC` is in Other Linker Flags
- Verify framework search paths are correct

## Performance

ANGLE's Metal backend is highly optimized. On modern iOS devices:
- 60 FPS easily achievable for 2D games
- 3D performance depends on scene complexity
- Metal translation overhead is minimal
