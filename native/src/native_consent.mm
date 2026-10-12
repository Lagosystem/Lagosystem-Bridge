// SPDX-License-Identifier: GPL-3.0-or-later
#import <AppKit/AppKit.h>
#include "native_consent.h"
namespace lagosystem::ui {
bool ConfirmDeviceAction(const std::string& title, const std::string& message) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [NSApp activateIgnoringOtherApps:YES];
        NSString* heading = [NSString stringWithUTF8String:title.c_str()];
        NSString* detail = [NSString stringWithUTF8String:message.c_str()];
        if (!heading || !detail) return false;
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSAlertStyleWarning];
        [alert setMessageText:heading];
        [alert setInformativeText:detail];
        [alert addButtonWithTitle:@"Allow"];
        [alert addButtonWithTitle:@"Deny"];
        const NSModalResponse response = [alert runModal];
        return response == NSAlertFirstButtonReturn;
    }
}
}
