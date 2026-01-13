# raylib iOS Example

This example demonstrates raylib running on iOS using [ANGLE](https://chromium.googlesource.com/angle/angle) (the official Google project) to translate OpenGL ES to Metal.

## Quick Start

### Prerequisites

1. **Xcode** (14.0 or later)
2. **XcodeGen** - Install with: `brew install xcodegen`
3. **depot_tools** - Required for building ANGLE (see below)

### Setup

1. **Build ANGLE for iOS:**

   First, install Google's depot_tools:
   ```bash
   git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
   export PATH="$PATH:$(pwd)/depot_tools"
   ```

   Clone and set up ANGLE:
   ```bash
   git clone https://chromium.googlesource.com/angle/angle
   cd angle
   python3 scripts/bootstrap.py
   gclient sync
   ```

   Build for iOS device (arm64):
   ```bash
   gn gen out/ios-device --args='
     target_os="ios"
     target_cpu="arm64"
     target_environment="device"
     is_debug=false
     angle_enable_metal=true
     ios_enable_code_signing=false
   '
   autoninja -C out/ios-device libEGL libGLESv2
   ```

   Build for iOS simulator (arm64):
   ```bash
   gn gen out/ios-simulator --args='
     target_os="ios"
     target_cpu="arm64"
     target_environment="simulator"
     is_debug=false
     angle_enable_metal=true
     ios_enable_code_signing=false
   '
   autoninja -C out/ios-simulator libEGL libGLESv2
   ```

2. **Copy ANGLE frameworks:**

   ANGLE builds as two separate frameworks (libEGL and libGLESv2). Copy them to the examples directory:
   ```bash
   # Create framework directories
   mkdir -p examples/ios/frameworks/device examples/ios/frameworks/simulator

   # Copy device frameworks
   cp -R out/ios-device/libEGL.framework examples/ios/frameworks/device/
   cp -R out/ios-device/libGLESv2.framework examples/ios/frameworks/device/

   # Copy simulator frameworks
   cp -R out/ios-simulator/libEGL.framework examples/ios/frameworks/simulator/
   cp -R out/ios-simulator/libGLESv2.framework examples/ios/frameworks/simulator/

   # Copy headers into frameworks
   for fw in device simulator; do
     mkdir -p examples/ios/frameworks/$fw/libEGL.framework/Headers
     mkdir -p examples/ios/frameworks/$fw/libGLESv2.framework/Headers
     cp -R include/EGL include/KHR examples/ios/frameworks/$fw/libEGL.framework/Headers/
     cp -R include/GLES include/GLES2 include/GLES3 include/KHR examples/ios/frameworks/$fw/libGLESv2.framework/Headers/
   done
   ```

3. **Generate Xcode project:**
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
└── frameworks/        # ANGLE frameworks (not in git)
    ├── simulator/     # For iOS Simulator
    └── device/        # For real devices
```

## How It Works

raylib's iOS platform layer (`src/platforms/rcore_ios.c`) uses:
- **ANGLE** for OpenGL ES 3.0 rendering (translated to Metal)
- **UIKit** for window management and touch input
- **CADisplayLink** for frame timing

The platform layer is compiled as Objective-C via `src/rcore_objc.m` which includes `rcore.c`.

## Writing Games for iOS

### Standard Game Loop

raylib games work on iOS with no code changes! The standard game loop works exactly like desktop:

```c
int main(void)  // Or GameInit() when called from AppDelegate
{
    InitWindow(0, 0, "My Game");  // Size ignored on iOS (always fullscreen)

    while (!WindowShouldClose())
    {
        // Update game logic

        BeginDrawing();
        // Draw
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

### Touch Input

Touch is automatically mapped to mouse events - no changes needed:

```c
if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Vector2 pos = GetMousePosition();
    // Handle tap
}

// Multi-touch is also available
int count = GetTouchPointCount();
for (int i = 0; i < count; i++) {
    Vector2 pos = GetTouchPosition(i);
}
```

### Screen Size

iOS apps are always fullscreen - InitWindow size parameters are ignored:

```c
InitWindow(0, 0, "My Game");  // Size parameters don't matter
int w = GetScreenWidth();      // Get actual screen size
int h = GetScreenHeight();
```

## Building for Device

The project automatically selects the correct framework based on the target platform.

1. Ensure you have built ANGLE for both simulator and device (see Setup above)
2. In Xcode, select your device and set a valid signing team
3. Build and run

## Current Limitations

- **Audio disabled** - needs Objective-C wrapper for miniaudio
- **No gamepad support** - would need GCController integration
- **No keyboard support** - would need UIKeyInput implementation

## Troubleshooting

### Black Screen
- Check Xcode console for EGL errors
- Verify ANGLE.framework is embedded (not just linked)

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
