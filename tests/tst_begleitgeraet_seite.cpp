// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Einstellseite fuer das Begleitgeraet am Transverter -- gebaut,
// bedient und abgelichtet, ohne Geraet und ohne Schirm.
//
// Warum das hier und nicht am laufenden Programm: eine Setup-Seite
// wird bei jedem Oeffnen neu gebaut, also ist sie ohne alles
// pruefbar. Und das Bild am Ende ist der Beleg, den man sonst nur per
// Hand bekommt -- Longpath macht das bei Dialogen seit den
// Logbuch-Kennzahlen so.
//
// Der Lauf schreibt ein PNG; der Pfad steht in der Ausgabe.

#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "gui/setup/CompanionRigPage.h"
#include "core/AppSettings.h"
#include "core/TransverterSetup.h"

using namespace Longpath;

class TstBegleitgeraetSeite : public QObject
{
    Q_OBJECT

private:
    template <typename T>
    static T* feld(CompanionRigPage* p, const char* name)
    {
        T* w = p->findChild<T*>(QString::fromLatin1(name));
        return w;
    }

private slots:
    void initTestCase()
    {
        // Ein sauberer Ausgangszustand, sonst faerbt ein frueherer
        // Lauf in diesem Sandkasten ab.
        TransverterSetup t;
        t.enabled  = false;
        t.ifLowHz  = 28000000;
        t.ifHighHz = 30000000;
        t.offsetHz = 116000000;
        t.save();
    }

    void dieSeiteHatIhreFelder()
    {
        CompanionRigPage p;

        QVERIFY(feld<QCheckBox>(&p, "companionFollow"));
        QVERIFY(feld<QComboBox>(&p, "companionRigModel"));
        QVERIFY(feld<QLineEdit>(&p, "companionRigDevice"));
        QVERIFY(feld<QComboBox>(&p, "companionRigBaud"));
        QVERIFY(feld<QPushButton>(&p, "companionRigTest"));
        QVERIFY(feld<QLabel>(&p, "companionRigStatus"));

        QVERIFY(feld<QCheckBox>(&p, "transverterEnabled"));
        QVERIFY(feld<QComboBox>(&p, "transverterPreset"));
        QVERIFY(feld<QDoubleSpinBox>(&p, "transverterIfLow"));
        QVERIFY(feld<QDoubleSpinBox>(&p, "transverterIfHigh"));
        QVERIFY(feld<QDoubleSpinBox>(&p, "transverterOffset"));
        QVERIFY(feld<QLabel>(&p, "transverterSummary"));

        // Und die Seite traegt ihren Namen, so wie der Dialog sie
        // registriert.
        QCOMPARE(p.pageTitle(), QStringLiteral("Companion Rig"));
    }

    void dieMhzUmrechnungRundetNichtDaneben()
    {
        // 28.000001 * 1e6 ist in Gleitkomma nicht 28000001.
        QCOMPARE(CompanionRigPage::mhzZuHz(28.000001), 28000001);
        QCOMPARE(CompanionRigPage::mhzZuHz(116.0),    116000000);
        QCOMPARE(CompanionRigPage::mhzZuHz(1152.0),  1152000000);
        QCOMPARE(CompanionRigPage::mhzZuHz(-116.0), -116000000);
        QCOMPARE(CompanionRigPage::mhzZuHz(0.0),              0);
    }

    void dieVorgabeFuelltDieFelderUndDenSatz()
    {
        CompanionRigPage p;
        auto* vorgabe = feld<QComboBox>(&p, "transverterPreset");
        auto* von     = feld<QDoubleSpinBox>(&p, "transverterIfLow");
        auto* bis     = feld<QDoubleSpinBox>(&p, "transverterIfHigh");
        auto* versatz = feld<QDoubleSpinBox>(&p, "transverterOffset");
        auto* satz    = feld<QLabel>(&p, "transverterSummary");
        QVERIFY(vorgabe && von && bis && versatz && satz);

        // Die 23-cm-Vorgabe auf 144-MHz-ZF: andere Zahlen als die
        // Vorbelegung, also sieht man, ob wirklich gefuellt wird.
        int ziel = -1;
        const QVector<TransverterSetup::Preset> liste =
            TransverterSetup::presets();
        for (int i = 0; i < liste.size(); ++i) {
            if (liste.at(i).offsetHz == 1152000000) { ziel = i; break; }
        }
        QVERIFY(ziel >= 0);
        vorgabe->setCurrentIndex(vorgabe->findData(ziel));

        QCOMPARE(CompanionRigPage::mhzZuHz(von->value()),     144000000);
        QCOMPARE(CompanionRigPage::mhzZuHz(bis->value()),     146000000);
        QCOMPARE(CompanionRigPage::mhzZuHz(versatz->value()), 1152000000);

        QVERIFY(satz->text().contains(QStringLiteral("1296.000")));
        QVERIFY(satz->text().contains(QStringLiteral("+1152.000")));
    }

    void einstellungenUeberlebenDieSeite()
    {
        {
            CompanionRigPage p;
            feld<QCheckBox>(&p, "transverterEnabled")->setChecked(true);

            auto* vorgabe = feld<QComboBox>(&p, "transverterPreset");
            int ziel = -1;
            const QVector<TransverterSetup::Preset> liste =
                TransverterSetup::presets();
            for (int i = 0; i < liste.size(); ++i) {
                if (liste.at(i).offsetHz == 404000000) { ziel = i; break; }
            }
            QVERIFY(ziel >= 0);
            vorgabe->setCurrentIndex(vorgabe->findData(ziel));

            feld<QCheckBox>(&p, "companionFollow")->setChecked(true);
            feld<QLineEdit>(&p, "companionRigDevice")
                ->setText(QStringLiteral("/dev/cu.usbserial-TEST"));
            emit feld<QLineEdit>(&p, "companionRigDevice")->editingFinished();
        }

        const TransverterSetup t = TransverterSetup::load();
        QCOMPARE(t.enabled,  true);
        QCOMPARE(t.offsetHz, 404000000);
        QCOMPARE(t.rfFrequencyHz(28300000), 432300000);

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("CompanionRigEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(QStringLiteral("CompanionRigDevice")).toString(),
                 QStringLiteral("/dev/cu.usbserial-TEST"));

        // Und die neue Seite zeigt wieder, was gespeichert wurde.
        CompanionRigPage zweite;
        QVERIFY(feld<QCheckBox>(&zweite, "transverterEnabled")->isChecked());
        QVERIFY(feld<QCheckBox>(&zweite, "companionFollow")->isChecked());
        QCOMPARE(feld<QLineEdit>(&zweite, "companionRigDevice")->text(),
                 QStringLiteral("/dev/cu.usbserial-TEST"));
    }

    void vonHandGetipptHeisstCustom()
    {
        CompanionRigPage p;
        auto* vorgabe = feld<QComboBox>(&p, "transverterPreset");
        auto* versatz = feld<QDoubleSpinBox>(&p, "transverterOffset");

        // Erst eine Vorgabe, damit die Auswahl nicht schon auf Custom steht.
        int ziel = -1;
        const QVector<TransverterSetup::Preset> liste =
            TransverterSetup::presets();
        for (int i = 0; i < liste.size(); ++i) {
            if (liste.at(i).offsetHz == 116000000) { ziel = i; break; }
        }
        QVERIFY(ziel >= 0);
        vorgabe->setCurrentIndex(vorgabe->findData(ziel));
        QVERIFY(vorgabe->currentData().toInt() >= 0);

        // Jetzt eine Zahl, die keiner Vorgabe entspricht.
        versatz->setValue(130.0);
        emit versatz->editingFinished();

        QCOMPARE(vorgabe->currentData().toInt(), -1);   // Custom
        QCOMPARE(TransverterSetup::load().offsetHz, 130000000);
    }

    void dieGeraeteauswahlBehauptetNichtsFalsches()
    {
        CompanionRigPage p;
        auto* geraete = feld<QComboBox>(&p, "companionRigModel");
        QVERIFY(geraete);

        // Zwei Faelle, beide richtig: Hamlib ist da -> echte Liste mit
        // echten Nummern. Hamlib fehlt -> EIN Eintrag, der das sagt,
        // und die Auswahl ist gesperrt. Was nicht passieren darf: eine
        // einprogrammierte Liste, die Nummern behauptet.
        if (geraete->isEnabled()) {
            QVERIFY(geraete->count() > 1);
            bool wenigstensEineEchteNummer = false;
            for (int i = 0; i < geraete->count(); ++i) {
                if (geraete->itemData(i).toInt() > 0) {
                    wenigstensEineEchteNummer = true;
                    break;
                }
            }
            QVERIFY(wenigstensEineEchteNummer);
        } else {
            QCOMPARE(geraete->count(), 1);
            QCOMPARE(geraete->itemData(0).toInt(), 0);
            QVERIFY(geraete->itemText(0).contains(QStringLiteral("Hamlib")));
        }

        QVERIFY(!CompanionRigPage::bevorzugteGeraete().isEmpty());
    }

    // Kein Geraet, kein Modell -> der Test sagt, was fehlt, statt
    // stillschweigend nichts zu tun.
    void derTestknopfSagtWasFehlt()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("CompanionRigModel"), 0);

        CompanionRigPage p;
        auto* geraete = feld<QComboBox>(&p, "companionRigModel");
        if (geraete->isEnabled()) {
            // Mit Hamlib liegt immer ein echtes Modell vorne; dann ist
            // dieser Fall nicht herstellbar, ohne die Auswahl zu
            // verbiegen. Also herbeifuehren.
            geraete->setCurrentIndex(-1);
        }
        feld<QCheckBox>(&p, "companionFollow")->setChecked(true);

        auto* status = feld<QLabel>(&p, "companionRigStatus");
        const QString vorher = status->text();
        feld<QPushButton>(&p, "companionRigTest")->click();

        QVERIFY(status->text() != vorher);
        QVERIFY(status->text().contains(QStringLiteral("Pick a rig"))
                || status->text().contains(QStringLiteral("not found")));
        // Und der Knopf bleibt ein Testknopf: nichts laeuft.
        QCOMPARE(feld<QPushButton>(&p, "companionRigTest")->text(),
                 QStringLiteral("Test"));
    }

    void bildFuerDieAkte()
    {
        CompanionRigPage p;
        feld<QCheckBox>(&p, "companionFollow")->setChecked(true);
        feld<QLineEdit>(&p, "companionRigDevice")
            ->setText(QStringLiteral("/dev/cu.usbserial-1410"));
        feld<QCheckBox>(&p, "transverterEnabled")->setChecked(true);

        auto* vorgabe = feld<QComboBox>(&p, "transverterPreset");
        int ziel = -1;
        const QVector<TransverterSetup::Preset> liste =
            TransverterSetup::presets();
        for (int i = 0; i < liste.size(); ++i) {
            if (liste.at(i).offsetHz == 116000000) { ziel = i; break; }
        }
        QVERIFY(ziel >= 0);
        vorgabe->setCurrentIndex(vorgabe->findData(ziel));

        p.resize(620, 620);
        const QPixmap bild = p.grab();
        QVERIFY(!bild.isNull());

        const QString pfad = QDir(QDir::tempPath())
            .filePath(QStringLiteral("longpath-companion-rig.png"));
        QVERIFY2(bild.save(pfad), qPrintable(pfad));
        qInfo() << "Bild der Seite:" << pfad;

        // Und das, worauf es im Bild ankommt, steht wirklich drin.
        QVERIFY(feld<QLabel>(&p, "transverterSummary")->text()
                    .contains(QStringLiteral("144.000")));
    }
};

QTEST_MAIN(TstBegleitgeraetSeite)
#include "tst_begleitgeraet_seite.moc"
