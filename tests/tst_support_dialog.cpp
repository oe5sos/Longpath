// tests/tst_support_dialog.cpp
//
// Von einer AetherSDR-Sichtung angestossen (2026-09-05, "System Info
// dialog"): SupportBundle::collectSystemInfo() um Kernzahl/RAM erweitert
// und als neue "System"-Gruppe in den schon vorhandenen Support-Dialog
// gesetzt (der den Log-Teil laengst hatte). Deckt beides ab: die
// erweiterten Felder selbst, und dass die Gruppe im Dialog auch wirklich
// ankommt.

#include <QtTest/QtTest>
#include <QGroupBox>
#include <QLabel>
#include "gui/SupportDialog.h"
#include "core/SupportBundle.h"

using namespace Longpath;

class TstSupportDialog : public QObject
{
    Q_OBJECT

private slots:
    void systemInfoIncludesCoreCountAndVersion()
    {
        const auto sys = SupportBundle::collectSystemInfo();
        QVERIFY2(sys.cpuCoreCount > 0, "Kernzahl sollte positiv sein");
        QVERIFY2(!sys.qtVersion.isEmpty(), "Qt-Version sollte gesetzt sein");
        QVERIFY2(!sys.osName.isEmpty(), "Betriebssystemname sollte gesetzt sein");
        QVERIFY2(!sys.cpuArch.isEmpty(), "CPU-Architektur sollte gesetzt sein");
    }

    void dialogShowsSystemGroup()
    {
        SupportDialog dlg(nullptr);
        bool found = false;
        for (auto* g : dlg.findChildren<QGroupBox*>()) {
            if (g->title() == QStringLiteral("System")) { found = true; break; }
        }
        QVERIFY2(found, "Erwartete Gruppe 'System' im Support-Dialog nicht gefunden");
    }
};

QTEST_MAIN(TstSupportDialog)
#include "tst_support_dialog.moc"
