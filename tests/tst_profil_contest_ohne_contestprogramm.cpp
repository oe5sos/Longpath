// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_profil_contest_ohne_contestprogramm.cpp  (Longpath)
// =================================================================
//
// Ein Bau OHNE Contestprogramm (Submodul third_party/contestprogramm nicht
// ausgecheckt, LONGPATH_CONTESTPROGRAMM=OFF) darf die Contest-Anordnung
// eines Layoutprofils nicht wegwerfen.
//
// Martin am 2026-11-11: „hier ist wieder alles irgendwie verschoben". Der
// Baum ~/Longpath/Longpath baut ohne das Submodul; jeder Lauf von dort
// schrieb die erfassten Profile ohne `contestPanels`/`contestLogbook`
// zurueck. Im Profil CONTEST war danach das Unified-Log-Fenster (717 x 445)
// weg. Gegen die alte Fassung faellt Fall 1 um.
//
// In einem Bau MIT Contestprogramm uebernimmt das
// tst_contest_panels_im_containers_menu (Fall 3).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "gui/LayoutProfiles.h"
#include "gui/MainWindow.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstProfilContestOhneContestprogramm : public QObject {
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};

private slots:
    void initTestCase()
    {
#ifdef LONGPATH_HAVE_CONTESTPROGRAMM
        QSKIP("Bau mit Contestprogramm: das deckt tst_contest_panels_im_containers_menu ab");
#endif
        m_mw = new MainWindow();
        m_mw->resize(1280, 800);
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);
    }

    void cleanupTestCase()
    {
        delete m_mw;
        m_mw = nullptr;
    }

    // Fall 1: das Profil bringt eine Contest-Anordnung mit; die Erfassung
    // schreibt sie unveraendert zurueck.
    void dieAnordnungUeberstehtDieErfassung()
    {
        auto* profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(profile);
        const QString vorher = profile->current();
        QVERIFY(!vorher.isEmpty());

        const QVariantMap log{{QStringLiteral("x"), 752}, {QStringLiteral("y"), 160},
                              {QStringLiteral("w"), 717}, {QStringLiteral("h"), 445}};
        QVariantMap zustand = profile->snapshot(vorher);
        zustand.insert(QStringLiteral("contestPanels"),
                       QVariantMap{{QStringLiteral("unifiedlog"), log}});
        zustand.insert(QStringLiteral("contestLogbook"), false);
        QVERIFY(profile->createWith(QStringLiteral("Contest"), zustand));
        QVERIFY(profile->activate(QStringLiteral("Contest")));
        QTest::qWait(300);

        profile->captureIntoCurrent();
        const QVariantMap erfasst = profile->snapshot(QStringLiteral("Contest"));
        QVERIFY2(erfasst.contains(QStringLiteral("contestPanels")),
                 "contestPanels fehlt nach der Erfassung");
        const QVariantMap panels = erfasst.value(QStringLiteral("contestPanels")).toMap();
        QCOMPARE(panels.value(QStringLiteral("unifiedlog")).toMap(), log);
        QVERIFY2(erfasst.contains(QStringLiteral("contestLogbook")),
                 "contestLogbook fehlt nach der Erfassung");
        QCOMPARE(erfasst.value(QStringLiteral("contestLogbook")).toBool(), false);

        // Fall 2: ein Profil OHNE diese Felder bekommt keine aus dem vorigen.
        QVariantMap ohne = profile->snapshot(vorher);
        ohne.remove(QStringLiteral("contestPanels"));
        ohne.remove(QStringLiteral("contestLogbook"));
        QVERIFY(profile->createWith(QStringLiteral("Ohne"), ohne));
        QVERIFY(profile->activate(QStringLiteral("Ohne")));
        QTest::qWait(300);
        profile->captureIntoCurrent();
        const QVariantMap erfasstOhne = profile->snapshot(QStringLiteral("Ohne"));
        QVERIFY2(!erfasstOhne.contains(QStringLiteral("contestPanels")),
                 "die Anordnung des vorigen Profils ist hinuebergewandert");
    }
};

QTEST_MAIN(TstProfilContestOhneContestprogramm)
#include "tst_profil_contest_ohne_contestprogramm.moc"
