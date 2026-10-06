// Der Schalter fuer den Driftausgleich — tut er, was er sagt?
//
// Ein Haekchen, das nichts schaltet, ist schlimmer als kein Haekchen: man
// legt es um, hoert hin, hoert keinen Unterschied und schliesst daraus auf
// den Regler statt auf die Verdrahtung. Genau diese Verwechslung soll der
// Stand unmoeglich machen.
//
// Geprueft wird die BEDIENUNG, nicht das Vorhandensein: das Haekchen wird
// angeklickt (toggle), und danach muss der Wert in den Einstellungen
// stehen -- und beim naechsten Aufbau der Seite wieder herauskommen.
//
// Offscreen (QT_QPA_PLATFORM=offscreen in longpath_add_test), damit beim
// Pruefen kein Fenster aufgeht und niemand es fuer Longpath haelt.

#include "core/AppSettings.h"
#include "gui/setup/AudioAdvancedPage.h"
#include "models/RadioModel.h"

#include <QtTest>
#include <QCheckBox>

using Longpath::AppSettings;
using Longpath::AudioAdvancedPage;
using Longpath::RadioModel;

namespace {
const QString kSchluessel = QStringLiteral("RxDriftAusgleich");

/// Sucht das Haekchen an seiner Beschriftung -- so, wie der Bediener es
/// sucht, und nicht ueber einen internen Namen, der sich lautlos aendern
/// koennte.
QCheckBox* findeHaken(QWidget* seite)
{
    const auto haken = seite->findChildren<QCheckBox*>();
    for (QCheckBox* h : haken) {
        if (h->text().contains(QStringLiteral("Uhrendrift"))) { return h; }
    }
    return nullptr;
}
}  // namespace

class TstRxDriftSchalter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().remove(kSchluessel); }
    void cleanup()      { AppSettings::instance().remove(kSchluessel); }

    void derHakenIstDa();
    void abWerkAus();
    void anklickenSchreibtDieEinstellung();
    void gesetzterWertKommtZurueck();
};

void TstRxDriftSchalter::derHakenIstDa()
{
    RadioModel modell;
    AudioAdvancedPage seite(&modell);
    QCheckBox* h = findeHaken(&seite);
    QVERIFY2(h, "Kein Haekchen mit 'Uhrendrift' auf der Seite Audio -> Advanced");
    // Der Hinweis auf die Wirkung gehoert daneben: der Regler greift erst
    // beim naechsten Start der Tonausgabe, und wer das nicht weiss, hoert
    // vergeblich hin.
    QVERIFY2(seite.findChildren<QLabel*>().size() > 0, "keine Hinweistexte");
}

void TstRxDriftSchalter::abWerkAus()
{
    // Ein neuer Umtaster im Hoerweg einer laufenden Station gehoert nicht
    // ungefragt eingeschaltet.
    AppSettings::instance().remove(kSchluessel);
    RadioModel modell;
    AudioAdvancedPage seite(&modell);
    QCheckBox* h = findeHaken(&seite);
    QVERIFY(h);
    QVERIFY2(!h->isChecked(), "Der Driftausgleich stand ab Werk auf EIN");
}

void TstRxDriftSchalter::anklickenSchreibtDieEinstellung()
{
    AppSettings::instance().remove(kSchluessel);
    RadioModel modell;
    AudioAdvancedPage seite(&modell);
    QCheckBox* h = findeHaken(&seite);
    QVERIFY(h);

    h->toggle();                       // wie ein Klick
    QCOMPARE(AppSettings::instance().value(kSchluessel).toString(),
             QStringLiteral("True"));

    h->toggle();
    QCOMPARE(AppSettings::instance().value(kSchluessel).toString(),
             QStringLiteral("False"));
}

void TstRxDriftSchalter::gesetzterWertKommtZurueck()
{
    AppSettings::instance().setValue(kSchluessel, QStringLiteral("True"));
    RadioModel modell;
    AudioAdvancedPage seite(&modell);
    QCheckBox* h = findeHaken(&seite);
    QVERIFY(h);
    QVERIFY2(h->isChecked(),
             "Die Seite zeigt den gespeicherten Wert nicht an -- der Bediener "
             "legt das Haekchen dann ein zweites Mal um und schaltet es aus");
}

QTEST_MAIN(TstRxDriftSchalter)
#include "tst_rx_drift_schalter.moc"
