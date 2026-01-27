#if defined(__APPLE__)

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

namespace {
    struct WindowSnapshot {
        bool valid = false;
        NSRect frame = NSZeroRect;
        NSWindowStyleMask styleMask = 0;
        NSInteger level = 0;
        BOOL ignoresMouseEvents = NO;
        BOOL opaque = YES;
        BOOL hasShadow = YES;
        NSWindowCollectionBehavior collectionBehavior = 0;
    };

    WindowSnapshot g_snapshot;

    NSInteger desktopBehindIconsLevel() {
        const int32_t desktopLevel = CGWindowLevelForKey(kCGDesktopWindowLevelKey);
        return static_cast<NSInteger>(desktopLevel - 1);
    }

    NSScreen* primaryScreen() {
        NSScreen* screen = [NSScreen mainScreen];
        if (screen) return screen;
        NSArray<NSScreen*>* screens = [NSScreen screens];
        return screens.count > 0 ? screens[0] : nil;
    }
}

void toggleDesktopPinned(GLFWwindow* window, bool& isDesktopPinned) {
    if (!window) return;

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (!nsWindow) return;

    if (!isDesktopPinned) {
        if (!g_snapshot.valid) {
            g_snapshot.valid = true;
            g_snapshot.frame = [nsWindow frame];
            g_snapshot.styleMask = [nsWindow styleMask];
            g_snapshot.level = [nsWindow level];
            g_snapshot.ignoresMouseEvents = [nsWindow ignoresMouseEvents];
            g_snapshot.opaque = [nsWindow isOpaque];
            g_snapshot.hasShadow = [nsWindow hasShadow];
            g_snapshot.collectionBehavior = [nsWindow collectionBehavior];
        }

        NSScreen* screen = primaryScreen();
        if (!screen) return;

        [nsWindow setStyleMask:NSWindowStyleMaskBorderless];
        [nsWindow setHasShadow:NO];
        [nsWindow setOpaque:YES];
        [nsWindow setBackgroundColor:[NSColor blackColor]];

        // Pin behind normal apps (and commonly behind desktop icons depending on macOS version)
        [nsWindow setLevel:desktopBehindIconsLevel()];

        // Non-interactive
        [nsWindow setIgnoresMouseEvents:YES];

        // Behave like a desktop background across Spaces
        [nsWindow setCollectionBehavior:(NSWindowCollectionBehaviorCanJoinAllSpaces |
                                         NSWindowCollectionBehaviorStationary |
                                         NSWindowCollectionBehaviorIgnoresCycle)];

        [nsWindow setFrame:[screen frame] display:YES];
        [nsWindow orderFront:nil];

        isDesktopPinned = true;
        return;
    }

    // Restore previous window state
    if (g_snapshot.valid) {
        [nsWindow setLevel:g_snapshot.level];
        [nsWindow setStyleMask:g_snapshot.styleMask];
        [nsWindow setIgnoresMouseEvents:g_snapshot.ignoresMouseEvents];
        [nsWindow setOpaque:g_snapshot.opaque];
        [nsWindow setHasShadow:g_snapshot.hasShadow];
        [nsWindow setCollectionBehavior:g_snapshot.collectionBehavior];
        [nsWindow setFrame:g_snapshot.frame display:YES];
        [nsWindow orderFront:nil];
    }

    isDesktopPinned = false;
}

#endif
