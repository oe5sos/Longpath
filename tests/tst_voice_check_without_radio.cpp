// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_voice_check_without_radio.cpp  (Longpath)
// =================================================================
// Eine Stimme messen braucht ein Mikrofon, kein Funkgeraet.
//
// Der Stimmcheck las das Mikrofon immer durch die Sendekette. Das ist
// richtig, WENN eine da ist -- gemessen wird dann, was das Geraet
// tatsaechlich bekommt, nach Mikrofonwahl und Mikrofonverstaerkung.
// Nur stand damit eine Verbindung vor genau dem, was der
// Sendeton-Optimierer verlangt: fuenfzehn Sekunden sprechen, ohne
// Einstellungen. Der Betreiber am Geraet, 2026-10-09, mit
// ausgeschaltetem Funkgeraet: „aufnahme geht nicht."
//
// Geprueft wird die Absage, nicht der Erfolg. Ob sich auf dieser
// Maschine wirklich ein Mikrofon oeffnen laesst, ist nichts, was ein
// Pruefstand erzwingen kann -- im CI gibt es keines. Was er erzwingen
// kann: dass die ALTE Absage („No transmit channel. Connect to the
// radio first") nicht mehr kommt, denn die war die Sperre.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

#include "gui/applets/TxVoiceCheckDialog.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

QPushButton* buttonSaying(QWidget* w, const QString& fragment)
{
    for (QPushButton* b : w->findChildren<QPushButton*>()) {
        if (b->text().contains(fragment, Qt::CaseInsensitive)) { return b; }
    }
    return nullptr;
}

bool anyLabelSays(QWidget* w, const QString& fragment)
{
    for (QLabel* l : w->findChildren<QLabel*>()) {
        if (l->text().contains(fragment, Qt::CaseInsensitive)) { return true; }
    }
    return false;
}

} // namespace

class TstVoiceCheckWithoutRadio : public QObject
{
    Q_OBJECT
private slots:

    // Ohne Funkgeraet darf der Aufnahmeknopf nicht mehr auf eine
    // Verbindung verweisen.
    void recordingNoLongerDemandsARadio()
    {
        RadioModel radio;                       // nichts verbunden
        QVERIFY2(radio.txChannel() == nullptr,
                 "der Fall, um den es geht: keine Sendekette");

        TxVoiceCheckDialog dlg(&radio, nullptr, /*embedded=*/true);

        QPushButton* rec = buttonSaying(&dlg, QStringLiteral("Record"));
        QVERIFY2(rec != nullptr, "der Aufnahmeknopf ist da");
        QVERIFY2(rec->isEnabled(),
                 "und er ist bedienbar, auch ohne Funkgeraet");

        rec->click();
        QTest::qWait(50);

        QVERIFY2(!anyLabelSays(&dlg,
                     QStringLiteral("No transmit channel")),
                 "die alte Absage ist weg -- sie war die Sperre");
        QVERIFY2(!anyLabelSays(&dlg,
                     QStringLiteral("transmit pump is not running")),
                 "und die zweite Absage desselben Hergangs auch");

        // Entweder laeuft die Aufnahme jetzt, oder diese Maschine hat
        // kein Mikrofon und sagt das. Beides ist richtig; still
        // nichts tun waere es nicht.
        const bool recording = dlg.findChild<QProgressBar*>() != nullptr;
        QVERIFY(recording
                || anyLabelSays(&dlg, QStringLiteral("microphone")));
    }

    // Das Messblatt haengt auch ohne Funkgeraet im Fenster -- sonst
    // waere der ganze Weg oben fuer nichts.
    void theMeasurementSheetIsThereWithoutARadioToo()
    {
        RadioModel radio;
        TxVoiceCheckDialog dlg(&radio, nullptr, /*embedded=*/true);
        QVERIFY(dlg.findChild<QWidget*>(
                    QString(), Qt::FindChildrenRecursively) != nullptr);
        QVERIFY2(anyLabelSays(&dlg,
                     QStringLiteral("each transmit channel")),
                 "das Messblatt ist im Fenster");
    }

    // Ein Bild, fuer die Durchsicht. Schreibt nur, wenn
    // LONGPATH_RENDER_DIR gesetzt ist -- im CI also gar nicht.
    void rendersForTheRecord()
    {
        const QByteArray dir = qgetenv("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_RENDER_DIR nicht gesetzt"); }

        RadioModel radio;
        TxVoiceCheckDialog dlg(&radio, nullptr, /*embedded=*/true);
        dlg.setStyleSheet(QStringLiteral("background: #0c0c0e; color: #dcdce1;"));
        dlg.resize(1000, 980);
        dlg.grab().save(QString::fromUtf8(dir)
                        + QStringLiteral("/stimmcheck-schritte.png"));
    }
};

QTEST_MAIN(TstVoiceCheckWithoutRadio)
#include "tst_voice_check_without_radio.moc"
