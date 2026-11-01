#pragma once

// =================================================================
// src/gui/applets/KombiApplet.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Ein leeres Feld, in das man Panels zieht ─────────────────────────
//
// Betreiber, 2026-11-01, zuerst am Beispiel: „ein widget, RX und TX
// oben im Panel zum umschalten" -- und dann die eigentliche Sache:
// „in dieses leere widget sollte man generell unterschiedlich widget
// reinziehen können".
//
// Also kein RX/TX-Applet, sondern ein BEHAELTER. RX und TX zusammen
// sind dann nur der erste Fall: man zieht beide hinein und hat sie
// auf einem Platz. Jedes andere Paar geht genauso.
//
// Der Gewinn ist Platz: zwei Dinge, die nie gleichzeitig bedient
// werden, kosten sonst doppelt so viel Flaeche wie noetig.
//
// ── Warum es die Panels ADOPTIERT, statt neue zu bauen ───────────────
//
// `RxApplet` und `TxApplet` haengen an einem halben Dutzend Dingen,
// die MainWindow ihnen von auszen reicht: Board-Faehigkeiten, SKU,
// Mikrofonprofile, Zweiton-Steuerung, der CFC-Dialog, die Bindung an
// den aktiven Empfaenger. Ein zweites Paar waere ein zweites Paar
// Verdrahtung -- und die haette genau EINEN Zustand gebraucht, den
// jemand zu wiederholen vergisst.
//
// Hier wechselt ein Panel nur den Elternteil. Jede Verbindung, jedes
// Profil, jede Bindung bleibt, woran sie haengt. Herausloesen gibt
// dasselbe Objekt unversehrt zurueck.
//
// Dasselbe Muster traegt `InstrumentApplet` schon: „ein Applet mit
// beiden Formen darin, nicht zwei Applets", damit ein Umschalten
// nicht etwas abbaut und wieder aufbaut.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-01 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/AppletWidget.h"

#include <QList>
#include <QPointer>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;

namespace Longpath {

class KombiApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit KombiApplet(RadioModel* model, QWidget* parent = nullptr);
    ~KombiApplet() override;

    /// Alle lebenden Kombi-Felder, egal in welchem Gehaeuse.
    ///
    /// Am 2026-11-01 vom Betreiber gefunden: „kann das rx panel nicht
    /// reinziehen". Sein RX sitzt in der SPALTE (im Fenster „RX1 Main
    /// Panel"), das Kombi-Feld schwebt DANEBEN als eigenes Fenster.
    /// Beide Ablegewege suchten aber nur in ihrem eigenen Gehaeuse --
    /// die Spalte in ihren Feldern, das Fenster in der Kennungstabelle
    /// des Hauptfensters. Ueber die Fenstergrenze hinweg fand keiner
    /// etwas.
    ///
    /// Diese Liste kennt keine Gehaeuse. Wer ein Panel ablegt, fragt
    /// sie und vergleicht Rechtecke auf dem SCHIRM.
    static QList<KombiApplet*> alle();
    /// Das sichtbare Feld unter dieser Stelle, oder nullptr.
    static KombiApplet* unter(const QPoint& globalPos);

    QString appletId() const override    { return QStringLiteral("Kombi"); }
    QString appletTitle() const override;
    void    syncFromModel() override;

    /// Der Umschalter gehoert in die Kopfleiste des Gehaeuses, nicht
    /// in einen eigenen Streifen -- Entwurf 3, vom Betreiber gewaehlt
    /// (2026-11-01): "3 bitte ... aber vielleicht gleich direkt in
    /// der kopfleiste?"
    QWidget* kopfleistenWidget() override;

    /// Das Zahnrad gehoert der Seite, die gerade zu sehen ist: wer TX
    /// sieht und das Zahnrad drueckt, meint die TX-Einstellungen.
    bool hasExtendedSettings() const override;
    void openExtendedSettings() override;

    /// Ein Panel aufnehmen (Elternwechsel, kein Neubau). Doppelt
    /// aufnehmen tut nichts.
    ///
    /// Jedes QWidget, nicht nur ein Applet. Betreiber, 2026-11-01:
    /// „ich will da auch den rotor und andere sache reinziehen
    /// können" -- und der Rotor ist kein Applet, sondern der Inhalt
    /// eines ToolWindow. Der Behaelter darf also nicht nach der Art
    /// fragen, sondern nur nach dem Widget.
    void nimmAuf(QWidget* panel);
    /// Wieder hergeben -- unversehrt, ohne Elternteil. Wer es bekommt,
    /// legt es zurueck in ein eigenes Feld.
    void gibFrei(QWidget* panel);
    /// Alle hergeben; Reihenfolge wie aufgenommen.
    QList<QWidget*> gibAlleFrei();

    /// Aufgeklappt: ALLE Seiten stehen untereinander, man sieht die
    /// anderen Panels mit. Zugeklappt: Reiter, feste Hoehe.
    ///
    /// Betreiber, 2026-11-01: „es soll eine größe geben, die fix
    /// bleibt, mit doppelklick auf die panelleiste soll dann quasi
    /// das menü aufgeklappt sein, sodass man die anderen widget auch
    /// sieht. doppelklick wieder auf die leiste oben, wieder
    /// standardgröße."
    void setAufgeklappt(bool an);
    bool aufgeklappt() const { return m_aufgeklappt; }

    QList<QWidget*> seiten() const;

    /// Der Name einer Seite: der Applet-Titel, sonst der Fenstertitel.
    /// Oeffentlich, damit ein Pruefstand ihn festhalten kann -- auf
    /// dem Reiter stand am 2026-11-01 „Panel", weil der Rotor seinen
    /// Namen am Fenster trug und nicht am Inhalt.
    static QString nameVon(const QWidget* w);
    int  seite() const;
    void zeigeSeite(int welche);
    bool istLeer() const;

signals:
    /// Rechtsklick auf das Feld: der Aufrufer zeigt eine Liste mit
    /// allem, was hineinkann, und legt das Gewaehlte mit `nimmAuf`
    /// hinein.
    ///
    /// Betreiber, 2026-11-01: „oder man öffnet das leere combined
    /// fenster, rechte moustaste und dann sieht man alles, was in das
    /// fenster kommen kann und klickt es an." Ziehen ist der eine
    /// Weg; wer das Feld gerade erst geholt hat, will nicht erst
    /// suchen, was es aufnehmen kann.
    void listeGewuenscht(const QPoint& globalPos);
    /// Jemand hat ein Panel wieder herausgeloest -- der Aufrufer legt
    /// es zurueck in ein eigenes Feld.
    void herausgeloest(QWidget* panel);
    /// Ein Reiter wurde HERAUSGEZOGEN: dasselbe wie oben, aber mit der
    /// Stelle, an der losgelassen wurde -- dort soll das Panel als
    /// eigenes Fenster stehen.
    ///
    /// Betreiber, 2026-11-01: „frequenz lässt sich reinziehen, soll
    /// sich aber wieder rausziehen lassen!" Hinein geht es durch
    /// Ziehen; hinaus ging es nur ueber einen Rechtsklick, den man
    /// kennen musz. Was hineingeht, musz auf demselben Weg wieder
    /// hinaus.
    void herausgezogen(QWidget* panel, const QPoint& globalPos);
    void seiteGewechselt(int welche);

protected:
    bool eventFilter(QObject* watched, QEvent* ev) override;

private:
    void baueReiter();
    void reiterNeu();
    void merkeStand() const;
    void chipNachziehen();
    void ordneSeitenEin();

    QStackedWidget* m_stapel{nullptr};
    /// Nimmt der breitesten Seite das Vorrecht auf die Breite -- die
    /// Spalte ist schmal und hoch (Betreiber, 2026-11-01).
    QScrollArea* m_rollbereich{nullptr};
    QWidget*        m_reiterleiste{nullptr};
    QHBoxLayout*    m_reiterReihe{nullptr};
    QLabel*         m_hinweis{nullptr};
    QList<QPointer<QWidget>> m_inhalte;
    QList<QPushButton*> m_reiter;
    QPushButton* m_chip{nullptr};   ///< der Umschalter in der Kopfleiste
    /// Zugbeginn auf einem Reiter -- erst ab einer Strecke wird daraus
    /// ein Herausziehen statt eines Klicks.
    QPoint m_zugStart;
    QPointer<QWidget> m_zugPanel;
    QWidget* m_aufklapp{nullptr};   ///< traegt die Seiten, wenn offen
    bool     m_aufgeklappt{false};
};

} // namespace Longpath
