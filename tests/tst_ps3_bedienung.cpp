// no-port-check: Longpath-eigener Prüfstand.
// =================================================================
// tests/tst_ps3_bedienung.cpp  (Longpath)
// =================================================================
//
// Was PureSignal 3 an der Oberfläche hinterlassen hat, und wie es
// entschieden wurde (Entwurfsblätter vom 2026-10-21, Fassung A):
//
// Fünf Bedienelemente im PureSignal-Fenster stellten die Tabelle mit
// den festen Stützstellen ein, aus der WDSP 1.29 die Korrekturkurve
// baute. PureSignal 3 bildet sie als NURB-Anpassung — die Setter
// dahinter gibt es nicht mehr. Sie bleiben sichtbar, aber abgeschaltet,
// damit niemand sie vergeblich sucht; dazu eine Zeile, die den Grund
// nennt.
//
// Der Lauf hält beides fest: dass sie ABGESCHALTET sind (nicht
// versteckt, nicht bedienbar) und dass die Zeile dasteht. Fällt er um,
// hat jemand einen der fünf wieder scharf gemacht — und das Fenster
// würde wieder so tun, als bewirke er etwas.
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Angelegt. Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>

#include "gui/PsForm.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TestPs3Bedienung : public QObject {
    Q_OBJECT

private slots:
    void die_fuenf_sind_abgeschaltet_aber_sichtbar()
    {
        RadioModel rm;
        PsForm form(&rm, nullptr);

        for (const char* name : {"chkPSPin", "chkPSMap", "chkPSStbl",
                                 "chkPSRelaxPtol", "lblPSTint",
                                 "comboPSTint"}) {
            auto* w = form.findChild<QWidget*>(QString::fromLatin1(name));
            QVERIFY2(w != nullptr, name);
            QVERIFY2(!w->isEnabled(), name);          // tut nichts mehr
            QVERIFY2(!w->isHidden(), name);           // ist aber noch da
            QVERIFY2(!w->toolTip().isEmpty(), name);  // und sagt, warum
        }
    }

    void die_zeile_nennt_den_grund()
    {
        RadioModel rm;
        PsForm form(&rm, nullptr);
        auto* l = form.findChild<QLabel*>(QStringLiteral("lblPSNoPs3"));
        QVERIFY(l != nullptr);
        QVERIFY(l->text().contains(QStringLiteral("NURB")));
        // Die Namen der fünf stehen drin, sonst sucht man sie trotzdem.
        for (const char* wort : {"PIN", "MAP", "STBL", "TINT"}) {
            QVERIFY2(l->text().contains(QString::fromLatin1(wort)), wort);
        }
    }

    void was_wirkt_bleibt_bedienbar()
    {
        // Gegenprobe in die andere Richtung: der Umbau darf nicht die
        // halbe Seite mit abgeschaltet haben.
        RadioModel rm;
        PsForm form(&rm, nullptr);
        for (const char* name : {"chkPSAutoAttenuate", "checkLoopback"}) {
            auto* w = form.findChild<QWidget*>(QString::fromLatin1(name));
            QVERIFY2(w != nullptr, name);
            QVERIFY2(w->isEnabled(), name);
        }
    }

    void die_versuchszahl_steht_neben_der_loesungszahl()
    {
        // try.cnt ist ein Longpath-Zusatz, kein Thetis-Feld: ohne ihn
        // kann man am Geraet nicht unterscheiden, ob gar nicht gerechnet
        // wird oder ob gerechnet und verworfen wird.
        RadioModel rm;
        PsForm form(&rm, nullptr);
        auto* cor = form.findChild<QLabel*>(QStringLiteral("lblPSInfo5"));
        auto* versuch = form.findChild<QLabel*>(QStringLiteral("lblPSInfo7"));
        QVERIFY(cor != nullptr);
        QVERIFY2(versuch != nullptr, "try.cnt fehlt im Kalibrierraster");

        // Und er steht wirklich daneben, nicht irgendwo: dieselbe Spalte,
        // eine Zeile tiefer. Sonst liest man die zwei Zahlen nicht als
        // Paar, und genau darum geht es bei ihnen.
        //
        // Gefragt wird das Raster, nicht der Bildpunkt: ein Formular, das
        // nie gezeigt wurde, hat fuer BEIDE Felder y() == 0 -- der erste
        // Versuch dieses Laufs fiel genau darauf um und haette jede
        // Anordnung durchgelassen, sobald man ihn "repariert" haette.
        QCOMPARE(versuch->parentWidget(), cor->parentWidget());
        auto* raster = qobject_cast<QGridLayout*>(
            cor->parentWidget()->layout());
        QVERIFY(raster != nullptr);
        int zeileCor = -1, spalteCor = -1, zeileVer = -1, spalteVer = -1;
        int spanne = 0;
        raster->getItemPosition(raster->indexOf(cor),
                                &zeileCor, &spalteCor, &spanne, &spanne);
        raster->getItemPosition(raster->indexOf(versuch),
                                &zeileVer, &spalteVer, &spanne, &spanne);
        QCOMPARE(spalteVer, spalteCor);
        QCOMPARE(zeileVer, zeileCor + 1);
    }
};

QTEST_MAIN(TestPs3Bedienung)
#include "tst_ps3_bedienung.moc"
