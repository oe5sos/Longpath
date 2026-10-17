#import <AVFoundation/AVFoundation.h>

#include "core/LogCategories.h"
#include <QLoggingCategory>

namespace Longpath {

void requestMicrophonePermission()
{
    AVAuthorizationStatus status =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

    const char* statusName = "unknown";
    switch (status) {
    case AVAuthorizationStatusNotDetermined: statusName = "NotDetermined"; break;
    case AVAuthorizationStatusRestricted:    statusName = "Restricted";    break;
    case AVAuthorizationStatusDenied:        statusName = "Denied";        break;
    case AVAuthorizationStatusAuthorized:    statusName = "Authorized";    break;
    }
    qCInfo(lcAudio) << "Microphone TCC status on launch:" << statusName;

    switch (status) {
    case AVAuthorizationStatusNotDetermined:
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                 completionHandler:^(BOOL granted) {
            if (granted) {
                qCInfo(lcAudio) << "Microphone access granted";
            } else {
                qCWarning(lcAudio) << "Microphone access denied by user";
            }
        }];
        break;

    case AVAuthorizationStatusAuthorized:
        break;

    case AVAuthorizationStatusDenied:
    case AVAuthorizationStatusRestricted:
        qCWarning(lcAudio)
            << "Microphone access denied/restricted. Open System Settings → "
               "Privacy & Security → Microphone and enable access for Longpath.";
        break;
    }
}

// Darf dieser Prozess das Mikrofon JETZT oeffnen?
//
// Fragt nur ab, fordert nichts an — das tut requestMicrophonePermission()
// beim Programmstart. "NotDetermined" heisst: die Frage laeuft noch (oder
// wurde nie gestellt), und ein Oeffnungsversuch wuerde blockieren, bis der
// Bediener den Dialog beantwortet. Das ist der Zustand, der am 2026-10-03
// den Verbindungsaufbau einfror.
bool microphoneAccessGranted()
{
    return [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]
           == AVAuthorizationStatusAuthorized;
}

} // namespace Longpath
