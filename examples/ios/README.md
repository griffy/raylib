# raylib iOS Platform

This is an iOS platform implementation for raylib using ANGLE for OpenGL ES to Metal translation.

## Status

**Proof of Concept** - This implementation demonstrates the architecture for iOS support but needs testing and refinement before being production-ready.

## Requirements

- Xcode 14+
- iOS 12.0+ deployment target
- ANGLE frameworks (libEGL.xcframework, libGLESv2.xcframework)

## Getting ANGLE

### Option 1: MetalANGLE (Recommended)

```bash
git clone https://github.com/nickvido/nickvido.git
cd nickvido/nickvido/nickvido/nickvido
```

Or download pre-built frameworks from the releases.

### Option 2: Build from Source

```bash
# Install depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:$(pwd)/depot_tools

# Get ANGLE
mkdir angle && cd angle
fetch angle
gclient sync

# Build for iOS
gn gen out/ios --args='target_os="ios" ios_deployment_target="12.0" is_component_build=false'
autoninja -C out/ios libEGL libGLESv2
```

## Xcode Project Setup

### 1. Create New Project

1. File → New → Project → iOS App
2. Language: Objective-C
3. Uncheck "Use Core Data" and "Include Tests"

### 2. Add ANGLE Frameworks

1. Drag `libEGL.xcframework` and `libGLESv2.xcframework` into your project
2. In Target → General → Frameworks, Libraries, and Embedded Content:
   - Set both to "Embed & Sign"

### 3. Add raylib Source Files

Add these from the raylib `src/` directory:
- `rcore.c`
- `rshapes.c`
- `rtextures.c`
- `rtext.c`
- `rmodels.c`
- `raudio.c`
- `rglfw.c` (won't be used but may be needed for compilation)
- `platforms/rcore_ios.c`

And headers:
- `raylib.h`
- `rlgl.h`
- `raymath.h`
- `config.h`

### 4. Add Example Files

From this directory:
- `AppDelegate.m`
- `game.c`

### 5. Remove Default Files

Delete these auto-generated files:
- `main.m` (we have our own in AppDelegate.m)
- `ViewController.m/h`
- `SceneDelegate.m/h`
- `Main.storyboard`
- `LaunchScreen.storyboard` (optional - keep if you want launch screen)

### 6. Configure Build Settings

In Target → Build Settings:

**Header Search Paths:**
```
$(PROJECT_DIR)/path/to/angle/include
$(PROJECT_DIR)/path/to/raylib/src
```

**Other C Flags:**
```
-DPLATFORM_IOS
-DGRAPHICS_API_OPENGL_ES3
-DSUPPORT_GESTURES_SYSTEM
```

**Other Linker Flags:**
```
-ObjC
```

### 7. Configure Info.plist

Remove `Main storyboard file base name` key (or set to empty).

Add for fullscreen:
```xml
<key>UIStatusBarHidden</key>
<true/>
<key>UIRequiresFullScreen</key>
<true/>
```

For landscape games:
```xml
<key>UISupportedInterfaceOrientations</key>
<array>
    <string>UIInterfaceOrientationLandscapeLeft</string>
    <string>UIInterfaceOrientationLandscapeRight</string>
</array>
```

### 8. Build and Run

Select an iOS Simulator or device and click Run.

## Code Structure

### AppDelegate.m

Sets up iOS application:
- Creates UIWindow and RaylibViewController
- Handles app lifecycle (background/foreground)
- Contains RaylibView with touch handling
- Uses CADisplayLink for frame callbacks

### game.c

Your game code:
- Implements `GameInit()` - called once at startup
- Implements `GameUpdate()` - called every frame via SetUpdateCallback

### rcore_ios.c

raylib platform layer:
- EGL/ANGLE initialization
- Touch input processing
- iOS-specific implementations of raylib functions

## iOS-Specific Considerations

### Callback-Based Rendering

iOS doesn't allow traditional game loops. Instead:

```c
// Desktop raylib
while (!WindowShouldClose()) {
    // Update and draw
}

// iOS raylib
void GameUpdate(void) {
    // Update and draw - called automatically
}
SetUpdateCallback(GameUpdate);
```

### Touch Input

Touch is automatically mapped to mouse:

```c
if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Vector2 pos = GetMousePosition();
    // Handle tap
}
```

For multi-touch:
```c
int count = GetTouchPointCount();
for (int i = 0; i < count; i++) {
    Vector2 pos = GetTouchPosition(i);
}
```

### Screen Size

Always fullscreen - size parameters to InitWindow() are ignored:

```c
InitWindow(0, 0, "My Game");  // Size doesn't matter
int w = GetScreenWidth();      // Get actual size
int h = GetScreenHeight();
```

### Assets

Place assets in Xcode project with "Copy Bundle Resources" build phase.
Use relative paths:

```c
Texture2D tex = LoadTexture("player.png");
Sound snd = LoadSound("jump.wav");
```

## Troubleshooting

### Black Screen

- Check EGL initialization in Xcode console
- Ensure ANGLE frameworks are embedded
- Verify view is created before InitPlatform()

### Touch Not Working

- Check RaylibView has `userInteractionEnabled = YES`
- Check `multipleTouchEnabled = YES`

### Crash on Launch

- Check ANGLE frameworks are properly linked
- Check all raylib source files are added to target
- Verify preprocessor defines are set

### Linker Errors

- Add `-ObjC` to Other Linker Flags
- Ensure all raylib .c files are in Compile Sources

## Performance

ANGLE's Metal backend is highly optimized and can actually outperform native OpenGL ES on iOS, since Apple has deprioritized OpenGL.

Typical performance on modern iOS devices:
- 60 FPS easily achievable for 2D games
- 3D games depend on complexity
- Metal translation overhead is minimal

## Known Limitations

- No physical keyboard support (would need UIKeyInput)
- No gamepad support (would need GCController)
- No window management (always fullscreen)
- Orientation changes need manual handling

## Contributing

This is a work in progress. Areas that need work:

1. Testing on real devices
2. External keyboard support
3. Game controller support
4. Orientation change handling
5. Integration with raylib's CMake build system

PRs welcome!
