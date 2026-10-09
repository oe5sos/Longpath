// =================================================================
// src/core/strip/AuPluginEditor.mm  (Longpath)
// =================================================================
//
// Longpath-original. Warum ein eigenes Cocoa-Fenster und nicht ein
// eingebettetes Qt-Widget: siehe AuPluginEditor.h.
//
// Kein ARC. Der Baum uebersetzt Objective-C++ ohne -fobjc-arc (wie
// MacMicPermission.mm und MacFloatingWindowBehavior.mm), also wird
// hier von Hand behalten und freigegeben. Was mit alloc/init kommt,
// gehoert uns; was aus einer Fabrikmethode kommt, nicht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Objective-C++, AI-assisted via
//                 Anthropic Claude (Claude Code), operator Martin
//                 Fischer.
// =================================================================

#include "core/strip/AuPluginEditor.h"

#include "core/LogCategories.h"

#include <QGuiApplication>

#ifdef Q_OS_MACOS

#import <AppKit/AppKit.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudioKit/CoreAudioKit.h>

#include <map>

namespace {

// Ein Fenster je Audio Unit. Zweimal auf „Open window" soll das
// vorhandene nach vorne holen, nicht ein zweites aufmachen -- zwei
// Fenster auf dieselbe Instanz sind zwei Oberflaechen, die sich
// gegenseitig verstellen.
std::map<void*, NSWindow*>& windows()
{
    static std::map<void*, NSWindow*> m;
    return m;
}

// Die eigene Ansicht des Plugins, falls es eine hat.
//
// Zurueckgegeben wird eine Ansicht, die UNS NICHT gehoert (das
// Plugin hat sie gebaut und autoreleased); das Fenster behaelt sie,
// sobald sie contentView wird.
NSView* cocoaViewFor(AudioUnit unit)
{
    UInt32 size = 0;
    Boolean writable = NO;
    OSStatus err = AudioUnitGetPropertyInfo(unit, kAudioUnitProperty_CocoaUI,
                                            kAudioUnitScope_Global, 0,
                                            &size, &writable);
    if (err != noErr || size < sizeof(AudioUnitCocoaViewInfo)) { return nil; }

    auto* info = static_cast<AudioUnitCocoaViewInfo*>(std::malloc(size));
    if (info == nullptr) { return nil; }

    NSView* view = nil;
    if (AudioUnitGetProperty(unit, kAudioUnitProperty_CocoaUI,
                             kAudioUnitScope_Global, 0, info, &size) == noErr
        && info->mCocoaAUViewBundleLocation != nullptr) {

        NSURL* url = (NSURL*)info->mCocoaAUViewBundleLocation;
        NSString* className = (NSString*)info->mCocoaAUViewClass[0];
        NSBundle* bundle = [NSBundle bundleWithURL:url];
        if (bundle != nil && [bundle load]) {
            Class factoryClass = [bundle classNamed:className];
            if (factoryClass != nil) {
                id factory = [[factoryClass alloc] init];
                if ([factory respondsToSelector:
                         @selector(uiViewForAudioUnit:withSize:)]) {
                    view = [factory uiViewForAudioUnit:unit
                                              withSize:NSMakeSize(0.0, 0.0)];
                }
                [factory release];
            }
        }
        // Die Zeichenketten und die Adresse gehoeren uns, seit
        // AudioUnitGetProperty sie herausgegeben hat.
        CFRelease(info->mCocoaAUViewBundleLocation);
        const int classes =
            int((size - sizeof(CFURLRef)) / sizeof(CFStringRef));
        for (int i = 0; i < classes; ++i) {
            if (info->mCocoaAUViewClass[i] != nullptr) {
                CFRelease(info->mCocoaAUViewClass[i]);
            }
        }
    }
    std::free(info);
    return view;
}

} // namespace

namespace Longpath::AuPluginEditor {

bool available()
{
    // Unter einer Offscreen-Plattform gibt es keinen Fensterserver,
    // auf dem ein NSWindow etwas zu suchen haette. Derselbe Grund wie
    // in MacFloatingWindowBehavior.mm, nur eine Etage frueher
    // abgefangen: dort kostete es im ersten CI-Lauf jeden
    // Fensterkonstruktor einen SIGSEGV.
    return QGuiApplication::platformName() != QLatin1String("offscreen");
}

bool show(void* unit, const QString& title)
{
    if (!available() || unit == nullptr) { return false; }
    auto* au = static_cast<AudioUnit>(unit);

    const auto at = windows().find(unit);
    if (at != windows().end()) {
        [at->second makeKeyAndOrderFront:nil];
        return true;
    }

    NSView* view = cocoaViewFor(au);
    bool own = false;
    if (view == nil) {
        // Apples Reglertafel. Jede Audio Unit hat Parameter, also hat
        // jede hiermit eine Oberflaeche -- sie sieht nur nicht nach
        // dem Hersteller aus.
        AUGenericView* generic = [[AUGenericView alloc] initWithAudioUnit:au];
        [generic setShowsExpertParameters:YES];
        view = generic;
        own = true;
    }
    if (view == nil) { return false; }

    NSRect frame = [view frame];
    if (frame.size.width < 240.0)  { frame.size.width = 420.0; }
    if (frame.size.height < 120.0) { frame.size.height = 280.0; }

    NSWindow* window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:(NSWindowStyleMaskTitled
                             | NSWindowStyleMaskClosable
                             | NSWindowStyleMaskMiniaturizable
                             | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [window setTitle:title.toNSString()];
    // NICHT beim Schliessen freigeben: wir halten den Zeiger in
    // windows() und raeumen in close() ab. Mit dem Vorgabeverhalten
    // waere der Eintrag nach einem Klick auf das rote Kreuz ein
    // Zeiger auf nichts -- und der naechste „Open window" ein
    // Absturz.
    [window setReleasedWhenClosed:NO];
    [window setContentView:view];
    if (own) { [view release]; }   // das Fenster haelt sie jetzt
    [window center];
    [window makeKeyAndOrderFront:nil];

    windows()[unit] = window;
    qCInfo(lcAudio) << "AU plugin window opened:" << title;
    return true;
}

void close(void* unit)
{
    const auto at = windows().find(unit);
    if (at == windows().end()) { return; }
    NSWindow* window = at->second;
    windows().erase(at);
    // Erst die Ansicht aus dem Fenster nehmen, dann das Fenster
    // zumachen. Die Ansicht gehoert dem Plugin und darf dessen Tod
    // nicht ueberleben -- ein Fenster, dessen Inhalt auf eine
    // freigegebene Audio Unit zeigt, stuerzt beim naechsten
    // Mausklick ab, und zwar irgendwo tief in fremdem Code.
    [window setContentView:[[[NSView alloc] initWithFrame:NSZeroRect]
                               autorelease]];
    [window close];
    [window release];
}

} // namespace Longpath::AuPluginEditor

#else

namespace Longpath::AuPluginEditor {
bool available() { return false; }
bool show(void*, const QString&) { return false; }
void close(void*) {}
} // namespace Longpath::AuPluginEditor

#endif
