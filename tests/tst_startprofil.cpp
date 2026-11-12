// =================================================================
// tests/tst_startprofil.cpp  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber 2026-11-12: „das erste Profil, bei mir N muss immer das
// Startprofil sein. Ich will nichts anderes!!!!!" Am Morgen war
// Longpath mit FT8 heraufgekommen, weil seit #408 (2026-11-06) das
// zuletzt aktive Profil galt.
//
// tst_layout_profiles prueft startWithFirst() fuer sich. Dieser Lauf
// prueft, dass das ECHTE Hauptfenster es beim Start auch ruft: erstes
// Fenster, zweites Profil aktivieren, Fenster schliessen, neues
// Fenster -- dort muss Profil 1 aktiv sein. Mit startWithLastActive()
// im MainWindow faellt der Lauf (Gegenprobe).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-12 — Created by Martin Fischer, AI-assisted via
//                 Anthropic Claude.
// =================================================================

#include <QtTest>

#include "gui/LayoutProfiles.h"
#include "gui/MainWindow.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstStartprofil : public QObject
{
    Q_OBJECT

private slots:
    void derStartIstImmerProfilEins()
    {
        QString erstes;
        const QString zweites = QStringLiteral("Zweites");
        {
            auto* mw = new MainWindow();
            mw->resize(1400, 900);
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));
            for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

            auto* profile = mw->findChild<LayoutProfiles*>();
            QVERIFY(profile);
            if (profile->names().isEmpty()) {
                QVERIFY(profile->create(QStringLiteral("N")));
            }
            erstes = profile->names().first();
            if (!profile->exists(zweites)) {
                QVERIFY(profile->create(zweites));
            }
            QVERIFY(profile->activate(zweites));
            QCOMPARE(profile->current(), zweites);
            profile->save();
            delete mw;
        }

        auto* mw = new MainWindow();
        mw->resize(1400, 900);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        auto* profile = mw->findChild<LayoutProfiles*>();
        QVERIFY(profile);
        QVERIFY2(profile->names().contains(zweites),
                 "das zweite Profil kam nicht von der Platte zurueck -- "
                 "dann prueft dieser Lauf nichts");
        qInfo() << "Profile" << profile->names() << "aktiv" << profile->current();
        QVERIFY2(profile->current() == erstes,
                 qPrintable(QStringLiteral("Start im Profil '%1' statt in "
                                           "Profil 1 '%2'")
                                .arg(profile->current(), erstes)));
        delete mw;
    }
};

QTEST_MAIN(TstStartprofil)
#include "tst_startprofil.moc"
