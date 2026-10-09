// no-port-check: Longpath-original file; siehe den Kopf von UiScale.h.

#include "core/UiScale.h"

#include "core/AppSettings.h"

namespace Longpath::UiScale {

const QList<int>& steps()
{
    static const QList<int> kSteps = { 75, 100, 115, 130, 150, 175, 200 };
    return kSteps;
}

bool isValid(int percent)
{
    return percent >= 50 && percent <= 300;
}

int current()
{
    const int pct = AppSettings::instance()
                        .value(QString::fromLatin1(kSettingsKey), 100)
                        .toInt();
    return isValid(pct) ? pct : 100;
}

bool store(int percent)
{
    if (!isValid(percent)) {
        return false;
    }
    AppSettings::instance().setValue(QString::fromLatin1(kSettingsKey), percent);
    // Sofort schreiben: `main.cpp` liest beim naechsten Start die Datei.
    AppSettings::instance().save();
    return true;
}

QString restartHint(int percent)
{
    return QStringLiteral(
        "Die Darstellungsgröße steht jetzt auf %1 %. Sie greift beim nächsten "
        "Start — Qt liest den Maßstab nur einmal, beim Hochfahren des "
        "Programms.").arg(percent);
}

} // namespace Longpath::UiScale
