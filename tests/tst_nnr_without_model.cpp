// SPDX-License-Identifier: GPL-2.0-or-later
// tests/tst_nnr_without_model.cpp (Longpath)
//
// NNR (WDSP 2.10) laeuft ohne Modelldatei als Durchreiche — das ist
// Absicht und steht so im Kopf von nnet.c. Was NICHT Absicht war: ein
// solcher NNET hat keinen Rauschkopf (n->df bleibt NULL, weil
// nnet_build() vorher aussteigt), und vier Funktionen fassten ihn
// trotzdem ungeprueft an. nnr.c laeuft mit NNR_ALL_MODELS() ueber JEDEN
// angelegten NNET, nicht nur ueber die geladenen, also reichte ein
// SetRXANNRAlpha(), um das Programm abzuraeumen.
//
// Gefunden am 2026-09-22 an der HL2-Werkbank: Longpath stuerzte auf einer
// Maschine ohne die Modelldateien bei JEDER Verbindung ab, mitten in
// RadioModel::connectToRadio() (setNnrTuning → SetRXANNRAlpha →
// setAlpha_dfhead auf einem Kopf, den es nicht gibt).
//
// Dieser Pruefstand geht ueber die NNET-API selbst, ohne Kanal und ohne
// Funkgeraet: Modellpfad auf eine Datei, die es nicht gibt, NNET anlegen,
// alle vier Funktionen aufrufen. Er besteht, wenn der Prozess lebt.
// no-port-check: Longpath-original.

#include <QtTest/QtTest>

extern "C" {
struct _nnet;
typedef struct _nnet* NNET;
NNET   create_nnet_slot(int slot, int nbins, int lookahead, double floor_db);
void   destroy_nnet(NNET n);
int    isReady_nnet(NNET n);
void   setAlpha_nnet(NNET n, double alpha);
double getAlpha_nnet(NNET n);
void   setKnee_nnet(NNET n, double knee_db);
double getKnee_nnet(NNET n);
void   SetNNRModelPathSlot(int slot, const char* path);
}

class TstNnrWithoutModel : public QObject { Q_OBJECT
private slots:
    void alphaAndKneeSurviveAModellessNet()
    {
        // Ein Pfad, den es garantiert nicht gibt — so wie bei jedem, der
        // die 6,5 MB Gewichte nicht neben dem Programm liegen hat.
        SetNNRModelPathSlot(0, "/nonexistent/longpath/wdsp_nnr_0.bin");

        // nbins/lookahead/floor wie nnr.c sie fuer 48 kHz anlegt.
        NNET n = create_nnet_slot(0, 257, 0, -30.0);
        QVERIFY2(n != nullptr, "create_nnet_slot() lieferte nichts");
        QVERIFY2(isReady_nnet(n) == 0,
                 "Der Pruefstand setzt voraus, dass ohne Modelldatei kein Modell geladen wird");

        // Vor dem 2026-09-22 endete die naechste Zeile mit SIGSEGV.
        setAlpha_nnet(n, 2.5);
        setKnee_nnet(n, -6.0);

        // Die Abfragen antworten mit den Vorgaben aus create_dfhead(),
        // statt durch einen NULL-Kopf zu lesen.
        QCOMPARE(getAlpha_nnet(n), 1.0);
        QCOMPARE(getKnee_nnet(n), -10.0);

        destroy_nnet(n);
    }

    void aModelIsStillLoadedWhenThePathIsRight()
    {
        // Die Gegenprobe: mit der mitgelieferten Datei MUSS ein Modell
        // geladen werden — sonst prueft der Test oben nur noch sich selbst.
        // Der Pfad kommt aus dem Quellbaum; ohne ihn wird uebersprungen.
        const QString root = qEnvironmentVariable("LONGPATH_SOURCE_DIR");
        const QString model = root + QStringLiteral("/third_party/wdsp/models/wdsp_nnr_0.bin");
        if (root.isEmpty() || !QFile::exists(model)) {
            QSKIP("wdsp_nnr_0.bin liegt nicht im Quellbaum (LONGPATH_SOURCE_DIR)");
        }
        SetNNRModelPathSlot(0, model.toUtf8().constData());
        NNET n = create_nnet_slot(0, 257, 0, -30.0);
        QVERIFY(n != nullptr);
        QVERIFY2(isReady_nnet(n) != 0,
                 "Mit der mitgelieferten Modelldatei wurde trotzdem kein Modell geladen");
        setAlpha_nnet(n, 2.5);
        QCOMPARE(getAlpha_nnet(n), 2.5);
        destroy_nnet(n);
        SetNNRModelPathSlot(0, "");
    }
};

QTEST_MAIN(TstNnrWithoutModel)
#include "tst_nnr_without_model.moc"
