// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_cw_key_setup_page.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Die Gruppe „Foot switch and CW key (HaliKey MIDI)"
// auf Setup > Keyboard > Shortcuts: ab Werk ist nichts angehakt, „Key
// the transmitter" ist ohne den ersten Schalter nicht zu erreichen, und
// was die Seite zeigt, ist, was die Tastung tut.
//
// Modification history (Longpath):
//   2026-11-17 - Created for Longpath by Martin Fischer (OE5SOS),
//                AI-assisted via Anthropic Claude.
// =================================================================
// no-port-check: Longpath-original test.

#include <QtTest>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>

#include "core/AppSettings.h"
#include "core/cw/CwTastung.h"
#include "gui/setup/KeyboardSetupPages.h"

using namespace Longpath;

class TstCwKeySetupPage : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

    void abWerkNichtsAngehakt()
    {
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        KeyboardShortcutsPage seite(nullptr, &t);
        QVERIFY(seite.cwKeyGroup() != nullptr);
        QVERIFY(seite.cwKeyGroup()->isEnabledTo(&seite));
        QCheckBox* sender = seite.cwKeyTransmitCheck();
        QVERIFY(!sender->isChecked());
        // Ohne „lesen" hat „senden" keinen Gegenstand.
        QVERIFY(!sender->isEnabledTo(&seite));
        QCOMPARE(seite.cwKeyStatus()->text(), QStringLiteral("Off."));
    }

    void schalterGehenZurTastung()
    {
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        KeyboardShortcutsPage seite(nullptr, &t);
        const QList<QCheckBox*> haken = seite.cwKeyGroup()->findChildren<QCheckBox*>();
        QVERIFY(haken.size() >= 2);
        QCheckBox* lesen = haken.first();
        QCheckBox* sender = seite.cwKeyTransmitCheck();
        QVERIFY(lesen != sender);

        lesen->click();
        QVERIFY(t.eingeschaltet());
        QVERIFY(!t.senderTasten());         // der zweite Schalter bleibt aus
        QVERIFY(sender->isEnabledTo(&seite));
        QVERIFY(seite.cwKeyStatus()->text().contains(QStringLiteral("Practice")));

        sender->click();
        QVERIFY(t.senderTasten());

        // Und zurueck: die Tastung aendert sich, die Seite folgt.
        t.setSenderTasten(false);
        QVERIFY(!sender->isChecked());
    }

    void fusstasterStehtZurWahl()
    {
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        KeyboardShortcutsPage seite(nullptr, &t);
        QComboBox* art = nullptr;
        for (QComboBox* c : seite.cwKeyGroup()->findChildren<QComboBox*>()) {
            if (c->findData(int(CwTastung::Art::Fusstaster)) >= 0) { art = c; }
        }
        QVERIFY(art != nullptr);
        QCOMPARE(art->itemText(art->findData(int(CwTastung::Art::Fusstaster))),
                 QStringLiteral("Foot switch (PTT)"));
        t.setArt(CwTastung::Art::Fusstaster);
        QCOMPARE(art->currentData().toInt(), int(CwTastung::Art::Fusstaster));
    }

    void ohneTastungGesperrt()
    {
        KeyboardShortcutsPage seite(nullptr, nullptr);
        QVERIFY(!seite.cwKeyGroup()->isEnabledTo(&seite));
    }
};

QTEST_MAIN(TstCwKeySetupPage)
#include "tst_cw_key_setup_page.moc"
