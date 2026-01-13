/*******************************************************************************************
*
*   raylib iOS - Application Delegate
*
*   This file sets up the iOS application and integrates with raylib's iOS platform layer.
*   Add this to your Xcode project along with raylib source files.
*
*   LICENSE: zlib/libpng (same as raylib)
*
********************************************************************************************/

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>

// raylib iOS platform integration functions
extern void SetPlatformWindow(void *window);
extern void SetPlatformViewController(void *viewController);
extern void SetPlatformView(void *view);
extern void SetPlatformDisplayLink(void *displayLink);
extern void SetUpdateCallback(void (*callback)(void));

extern void _iosFrameCallback(void);
extern void _iosTouchEvent(int action, int index, float x, float y);
extern void _iosAppPaused(void);
extern void _iosAppResumed(void);

// Forward declaration - implement this in your game code
// Note: When using PLATFORM_IOS, raylib.h renames main() to GameInit() automatically
// so games compile without modification. The int return type is from main().
extern int GameInit(void);

// Starts the game loop on a background thread (allows main thread to handle touch events)
extern void StartGameThread(void);

//----------------------------------------------------------------------------------
// RaylibView - Custom UIView with CAMetalLayer for ANGLE rendering
//----------------------------------------------------------------------------------
@interface RaylibView : UIView
@end

@implementation RaylibView

+ (Class)layerClass
{
    // ANGLE uses Metal backend, so we need CAMetalLayer
    return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        self.multipleTouchEnabled = YES;
        self.userInteractionEnabled = YES;

        // Configure the Metal layer for ANGLE
        CAMetalLayer *metalLayer = (CAMetalLayer *)self.layer;
        metalLayer.opaque = YES;
        metalLayer.contentsScale = [[UIScreen mainScreen] scale];
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = NO;

        // Get the default Metal device
        metalLayer.device = MTLCreateSystemDefaultDevice();

        self.backgroundColor = [UIColor blackColor];
    }
    return self;
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    // Update Metal layer drawable size when layout changes
    CAMetalLayer *metalLayer = (CAMetalLayer *)self.layer;
    CGFloat scale = [[UIScreen mainScreen] scale];
    metalLayer.drawableSize = CGSizeMake(self.bounds.size.width * scale,
                                          self.bounds.size.height * scale);
}

// Touch handling
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    NSArray *allTouches = [[event allTouches] allObjects];
    for (NSUInteger i = 0; i < allTouches.count && i < 10; i++)
    {
        UITouch *touch = allTouches[i];
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(0, (int)i, location.x, location.y);
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    NSArray *allTouches = [[event allTouches] allObjects];
    for (NSUInteger i = 0; i < allTouches.count && i < 10; i++)
    {
        UITouch *touch = allTouches[i];
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(1, (int)i, location.x, location.y);
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    NSArray *allTouches = [[event allTouches] allObjects];
    for (NSUInteger i = 0; i < allTouches.count && i < 10; i++)
    {
        UITouch *touch = allTouches[i];
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(2, (int)i, location.x, location.y);
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    NSArray *allTouches = [[event allTouches] allObjects];
    for (NSUInteger i = 0; i < allTouches.count && i < 10; i++)
    {
        UITouch *touch = allTouches[i];
        CGPoint location = [touch locationInView:self];
        _iosTouchEvent(3, (int)i, location.x, location.y);
    }
}

@end

//----------------------------------------------------------------------------------
// RaylibViewController - Main view controller
//----------------------------------------------------------------------------------
@interface RaylibViewController : UIViewController
@property (nonatomic, strong) CADisplayLink *displayLink;
@property (nonatomic, strong) RaylibView *raylibView;
@end

@implementation RaylibViewController

- (void)viewDidLoad
{
    [super viewDidLoad];

    // Create raylib view
    self.raylibView = [[RaylibView alloc] initWithFrame:self.view.bounds];
    self.raylibView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:self.raylibView];

    // Pass view to raylib
    SetPlatformView((__bridge void *)self.raylibView);
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];

    // Create display link for frame callbacks FIRST (before GameInit blocks in game loop)
    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(frameCallback:)];
    self.displayLink.preferredFramesPerSecond = 60;
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    SetPlatformDisplayLink((__bridge void *)self.displayLink);

    // Start the game loop on a background thread
    // This allows the main thread to continue handling touch events
    StartGameThread();
}

- (void)frameCallback:(CADisplayLink *)sender
{
    _iosFrameCallback();
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
    [self.displayLink invalidate];
    self.displayLink = nil;
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

- (BOOL)prefersHomeIndicatorAutoHidden
{
    return YES;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
    return UIRectEdgeAll;
}

@end

//----------------------------------------------------------------------------------
// RaylibAppDelegate - Application delegate
//----------------------------------------------------------------------------------
@interface RaylibAppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow *window;
@end

@implementation RaylibAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    // Create window
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

    // Create view controller
    RaylibViewController *viewController = [[RaylibViewController alloc] init];
    self.window.rootViewController = viewController;

    // Pass to raylib
    SetPlatformWindow((__bridge void *)self.window);
    SetPlatformViewController((__bridge void *)viewController);

    // Show window
    [self.window makeKeyAndVisible];

    return YES;
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    _iosAppPaused();
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    _iosAppResumed();
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Cleanup handled by raylib
}

@end

//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([RaylibAppDelegate class]));
    }
}
