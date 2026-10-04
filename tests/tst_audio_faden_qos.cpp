// Prüfstand: eine ausdrückliche Qt-Priorität verhindert auf macOS die
// QoS-Klasse — der Grund, warum Audio-Fäden ohne Prioritätsangabe starten.
//
// Befund vom 2026-10-03, aus Martins Betriebslog:
//
//   WRN: pthread_set_qos_class_self_np(USER_INTERACTIVE) failed;
//        thread continues at default QoS.
//
// Die Zeile kam vom Sende-Faden. `TxWorkerThread::startPump` rief
// `QThread::start(QThread::HighPriority)`, und Qt setzt dafür auf macOS die
// Ablaufparameter des Fadens. Darwin verweigert danach jede QoS-Klasse.
// Damit hob die Priorität genau die Behandlung auf, die `run()` zwei
// Bildschirmseiten weiter unten ausdrücklich haben will — und zwar auf der
// Seite, auf der es auf die Luft geht.
//
// Der Empfangs-Faden war nie betroffen: `m_dspThread->start()` ohne Argument.
//
// Gemessen (drei Fäden, sonst gleich):
//
//   start()                      set() -> 0   QoS danach = 0x21 USER_INTERACTIVE
//   start(HighPriority)          set() -> 1   QoS danach = 0x00 UNSPECIFIED
//   start(TimeCriticalPriority)  set() -> 1   QoS danach = 0x00 UNSPECIFIED
//
// Geprüft wird hier die REGEL, nicht die Aufrufstelle: wer sie kennt, baut
// die Priorität nicht wieder ein. Die Aufrufstelle selbst zu prüfen hieße,
// einen Sende-Faden samt Mikrofonquelle und WDSP-Kanal hochzufahren.

#include <QtTest>
#include <QThread>

#ifdef Q_OS_MAC
#include <pthread/qos.h>
#endif

namespace {

/** Setzt auf dem eigenen Faden die QoS-Klasse und merkt sich das Ergebnis. */
class QosFaden : public QThread {
public:
    int setzErgebnis = -999;
    unsigned klasseDanach = 0;

    void run() override
    {
#ifdef Q_OS_MAC
        setzErgebnis = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
        qos_class_t k = QOS_CLASS_UNSPECIFIED;
        int rel = 0;
        pthread_get_qos_class_np(pthread_self(), &k, &rel);
        klasseDanach = static_cast<unsigned>(k);
#endif
    }
};

}  // namespace

class TestAudioFadenQos : public QObject {
    Q_OBJECT

private slots:

#ifdef Q_OS_MAC

    /** Ohne Prioritätsangabe: die QoS-Klasse lässt sich setzen. Das ist die
     *  Zusage, auf der die ganze Audio-Erhöhung steht. */
    void ohnePrioritaetGehtDieQosKlasse()
    {
        QosFaden f;
        f.start();
        QVERIFY(f.wait(10000));
        QCOMPARE(f.setzErgebnis, 0);
        QCOMPARE(f.klasseDanach, static_cast<unsigned>(QOS_CLASS_USER_INTERACTIVE));
    }

    /** Mit ausdrücklicher Qt-Priorität: Darwin verweigert die QoS-Klasse, und
     *  der Faden bleibt auf UNSPECIFIED stehen.
     *
     *  Diese Prüfung ist der eigentliche Beleg. Schlägt sie eines Tages um
     *  (Qt oder Darwin ändern das Verhalten), ist die Begründung in
     *  TxWorkerThread.cpp hinfällig und gehört neu geschrieben — dann soll
     *  jemand hier darüber stolpern. */
    void mitQtPrioritaetScheitertDieQosKlasse()
    {
        for (const QThread::Priority p : {QThread::HighPriority,
                                          QThread::TimeCriticalPriority}) {
            QosFaden f;
            f.start(p);
            QVERIFY(f.wait(10000));
            QVERIFY2(f.setzErgebnis != 0,
                     "Darwin hat die QoS-Klasse trotz ausdruecklicher "
                     "Qt-Prioritaet angenommen — die Begruendung in "
                     "TxWorkerThread.cpp stimmt dann nicht mehr");
            QCOMPARE(f.klasseDanach, static_cast<unsigned>(QOS_CLASS_UNSPECIFIED));
        }
    }

#else

    /** Auf Windows und Linux gibt es diese Wechselwirkung nicht; die
     *  Erhöhung läuft dort über MMCSS bzw. SCHED_FIFO. */
    void nurAufMacOsRelevant()
    {
        QSKIP("QoS-Klassen gibt es nur auf macOS");
    }

#endif
};

QTEST_MAIN(TestAudioFadenQos)
#include "tst_audio_faden_qos.moc"
