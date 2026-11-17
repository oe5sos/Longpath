// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_cw_key_setup_page.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Die Gruppe „Foot switch and CW key (HaliKey MIDI)"
// (src/gui/setup/HaliKeyGroup): ab Werk ist nichts angehakt, „Key the
// transmitter" ist ohne den ersten Schalter nicht zu erreichen, und was
// die Gruppe zeigt, ist, was die Tastung tut.
//
// Und sie steht dort, wo der Betreiber sie gesucht hat: auf Setup >
// CAT & Network > MIDI Control (Bildschirmfoto vom 2026-11-17 -- die
// Seite zeigte den alten Platzhalter, die Gruppe lag einen Commit lang
// unter Keyboard > Shortcuts).
//
// Modification history (Longpath):
//   2026-11-17 - Created for Longpath by Martin Fischer (OE5SOS),
//                AI-assisted via Anthropic Claude.
// =================================================================
// no-port-check: Longpath-original test.

#include <QtTest>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

#include "core/AppSettings.h"
#include "core/cw/CwTastung.h"
#include "gui/setup/CatNetworkSetupPages.h"
#include "gui/setup/HaliKeyGroup.h"

using namespace Longpath;

class TstCwKeySetupPage : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

    // Die Seite, auf der man nach MIDI sucht, traegt die Gruppe -- und
    // keinen Platzhalter mehr.
    void stehtAufMidiControl()
    {
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        CatMidiControlPage seite(&t);
        HaliKeyGroup* gruppe = seite.findChild<HaliKeyGroup*>();
        QVERIFY2(gruppe != nullptr, "MIDI Control zeigt die HaliKey-Gruppe nicht");
        QCOMPARE(gruppe, seite.haliKeyGroup());
        QVERIFY(gruppe->isEnabledTo(&seite));
        QVERIFY(gruppe->title().contains(QStringLiteral("HaliKey")));
        // Nichts mehr, was etwas verspricht, das es nicht gibt.
        for (const QLabel* l : seite.findChildren<QLabel*>()) {
            QVERIFY2(!l->text().contains(QStringLiteral("will appear here")),
                     qPrintable(l->text()));
        }
        for (const QPushButton* b : seite.findChildren<QPushButton*>()) {
            QVERIFY2(!b->text().startsWith(QStringLiteral("Learn")), qPrintable(b->text()));
        }
    }

    void abWerkNichtsAngehakt()
    {
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        HaliKeyGroup gruppe(&t);
        QVERIFY(!gruppe.readCheck()->isChecked());
        QCheckBox* sender = gruppe.keyTransmitCheck();
        QVERIFY(!sender->isChecked());
        // Ohne „lesen" hat „senden" keinen Gegenstand.
        QVERIFY(!sender->isEnabledTo(&gruppe));
        QCOMPARE(gruppe.statusLabel()->text(), QStringLiteral("Off."));
    }

    void schalterGehenZurTastung()
    {
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        HaliKeyGroup gruppe(&t);
        QCheckBox* lesen = gruppe.readCheck();
        QCheckBox* sender = gruppe.keyTransmitCheck();

        lesen->click();
        QVERIFY(t.eingeschaltet());
        QVERIFY(!t.senderTasten());         // der zweite Schalter bleibt aus
        QVERIFY(sender->isEnabledTo(&gruppe));
        QVERIFY(gruppe.statusLabel()->text().contains(QStringLiteral("Practice")));

        sender->click();
        QVERIFY(t.senderTasten());

        // Und zurueck: die Tastung aendert sich, die Gruppe folgt.
        t.setSenderTasten(false);
        QVERIFY(!sender->isChecked());
    }

    void fusstasterStehtZurWahl()
    {
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        HaliKeyGroup gruppe(&t);
        QComboBox* art = gruppe.typeCombo();
        const int i = art->findData(int(CwTastung::Art::Fusstaster));
        QVERIFY(i >= 0);
        QCOMPARE(art->itemText(i), QStringLiteral("Foot switch (PTT)"));
        art->setCurrentIndex(i);
        emit art->activated(i);
        QCOMPARE(t.art(), CwTastung::Art::Fusstaster);
        // Mit dem Fusstaster an der Buchse und beiden Schaltern sagt die
        // Zustandszeile, was ein Tritt tut.
        t.setEingeschaltet(true);
        t.setSenderTasten(true);
        QVERIFY(gruppe.statusLabel()->text().contains(QStringLiteral("No radio to key"))
                || gruppe.statusLabel()->text().contains(QStringLiteral("foot switch")));
    }

    // Kein Pruefpunkt, ein Werkzeug: mit LONGPATH_SEITENBILD=<Datei.png>
    // legt der Lauf ein Bild der Seite ab (offscreen), damit man sie
    // ansehen kann, ohne das Programm zu starten.
    void seitenbild()
    {
        const QString ziel = qEnvironmentVariable("LONGPATH_SEITENBILD");
        if (ziel.isEmpty()) { QSKIP("LONGPATH_SEITENBILD nicht gesetzt"); }
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        t.setEingeschaltet(true);
        CatMidiControlPage seite(&t);
        seite.resize(860, 620);
        seite.show();
        QVERIFY(QTest::qWaitForWindowExposed(&seite));
        QVERIFY(seite.grab().save(ziel));
    }

    void ohneTastungGesperrt()
    {
        CatMidiControlPage seite(nullptr);
        QVERIFY(seite.haliKeyGroup() != nullptr);
        QVERIFY(!seite.haliKeyGroup()->isEnabledTo(&seite));
    }
};

QTEST_MAIN(TstCwKeySetupPage)
#include "tst_cw_key_setup_page.moc"
