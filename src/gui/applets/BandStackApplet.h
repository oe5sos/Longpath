// no-port-check: Longpath-eigene Oberflaeche; die Technik dahinter steht in
// src/models/bandstack/ (aus Upstream NereusSDR, Regeln aus Thetis).
// =================================================================
// src/gui/applets/BandStackApplet.h  (Longpath)
// =================================================================
//
// Der Bandstapel als Applet in der Spalte -- Entwurf A, vom Betreiber am
// 2026-11-11 gewaehlt („A und D1"). Das Blatt dazu malt
// tests/tst_bandstapel_entwurf.cpp; diese Klasse ist die echte Fassung.
//
//   Kopfleiste   der Stand als Kapsel („40m 5/7", „40m –/7")
//   Zahnrad      Optionen: Marken im Spektrum, Duplikatschutz,
//                Auto-Speichern, Verfall, Region neu laden, Leeren
//   Inhalt       zwei Zeilen Erklaerung (Hausstil-Regel 6), darunter
//                je Band eine Gruppe; das Band der Scheibe offen, die
//                anderen zu mit Anzahl und gesperrten Eintraegen.
//                Die Stelle der Scheibe ist eine Mulde mit schmalem
//                blauem Strich und der Marke ihres Buchstabens.
//
// Klick auf eine Zeile = hin. Doppelklick = umbenennen. Rechtsklick auf
// eine Zeile = ihr Menue, auf einen Gruppenkopf = das Menue des Bandes.
// Das Applet rechnet nichts selbst: jede Handlung geht an den
// BandStackController.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#pragma once

#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QWidget>

#include "AppletWidget.h"
#include "models/Band.h"
#include "models/bandstack/BandStackStore.h"

class QLabel;
class QLineEdit;
class QMenu;

namespace Longpath {

class BandStackController;

/// Die gemalte Liste. Getrennt vom Applet, damit ein Pruefstand sie
/// ohne Spalte und Gehaeuse aufziehen und abgreifen kann.
class BandStackListe : public QWidget {
    Q_OBJECT
public:
    explicit BandStackListe(QWidget* parent = nullptr);

    /// Stapel und Scheibe setzen und neu aufbauen. `sliceId` -1 = keine.
    void setzeStapel(BandStackController* stapel, int sliceId);
    void neuAufbauen();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return {200, sizeHint().height()}; }

    bool istOffen(Band band) const;
    /// Wo das Namensfeld einer Zeile sitzt (Umbenennen), leer wenn die
    /// Zeile nicht zu sehen ist.
    QRect namensfeldFuer(const QString& id) const;
    void setzeOffen(Band band, bool offen);

    // ── Masze aus dem gewaehlten Blatt ──────────────────────────────
    static constexpr int kGruppeH = 24;
    static constexpr int kZeileH  = 26;
    static constexpr int kZeileLuft = 1;
    static constexpr int kKnopfH  = 26;

    // Pruefstand
    QRect zeileRectForTest(const QString& id) const;
    QRect gruppeRectForTest(Band band) const;
    QRect knopfRectForTest() const;
    QString stelleForTest() const { return m_stelle; }

signals:
    void zeileGeklickt(const QString& id);
    void zeilenMenue(const QString& id, const QPoint& global);
    void bandMenue(Longpath::Band band, const QPoint& global);
    void umbenennenGewuenscht(const QString& id, const QRect& nameRect);
    void hinzufuegenGeklickt();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Zeile {
        enum Art { Gruppe, Eintrag, Knopf } art{Gruppe};
        Band band{Band::GEN};
        QRect rect;
        // Gruppe
        int anzahl{0};
        int gesperrt{0};
        bool offen{false};
        QString rueckkehr;
        // Eintrag
        BandStackEntry eintrag;
        bool stelle{false};
    };

    void anordnen();
    int zeileBei(const QPoint& pos) const;
    QRect nameRect(const Zeile& z) const;
    void maleGruppe(QPainter& p, const Zeile& z) const;
    void maleEintrag(QPainter& p, const Zeile& z, bool schwebt) const;
    void maleKnopf(QPainter& p, const Zeile& z) const;

    QPointer<BandStackController> m_stapel;
    int m_sliceId{-1};
    QChar m_buchstabe;
    Band m_band{Band::GEN};
    QString m_stelle;
    QSet<int> m_aufgeklappt;    // von Hand geoeffnet (Band als int)
    QSet<int> m_zugeklappt;     // von Hand geschlossen (nur das eigene Band)
    QList<Zeile> m_zeilen;
    int m_hoehe{0};
    int m_schwebt{-1};
    bool m_knopfGedrueckt{false};
};

class BandStackApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit BandStackApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId() const override { return QStringLiteral("BandStack"); }
    QString appletTitle() const override { return QStringLiteral("Band Stack"); }
    void syncFromModel() override { auffrischen(); }

    QWidget* kopfleistenWidget() override;
    bool hasExtendedSettings() const override { return true; }
    void openExtendedSettings() override;

    /// Marken im Spektrum zeigen (Zahnrad). Vorgabe an.
    static bool markenImSpektrum();
    static void setzeMarkenImSpektrum(bool an);

    /// Das Menue, das das Zahnrad zeigt; der Aufrufer besitzt es.
    QMenu* baueOptionenMenue(QWidget* parent);
    QMenu* baueZeilenMenue(const QString& id, QWidget* parent);
    QMenu* baueBandMenue(Band band, QWidget* parent);

    // Pruefstand
    BandStackListe* listeForTest() const { return m_liste; }
    QLabel* standForTest() const { return m_stand; }
    QLabel* erklaerungForTest() const { return m_erklaerung; }
    QLineEdit* namensfeldForTest() const { return m_namensfeld; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    /// Ein Satz fuer den Betreiber; MainWindow zeigt ihn als Hinweis.
    void hinweis(const QString& text, bool warnung);
    /// Der Schalter „Show on the spectrum" hat sich geaendert.
    void markenSchalterGeaendert(bool an);

private:
    BandStackController* stapel() const;
    int sliceId() const;
    void auffrischen();
    void zeileGeklickt(const QString& id);
    void umbenennen(const QString& id, const QRect& nameRect);
    void hinzufuegen();
    void melde(bool ok, const QString& grund);

    BandStackListe* m_liste{nullptr};
    QLabel* m_erklaerung{nullptr};
    QLabel* m_stand{nullptr};
    QLineEdit* m_namensfeld{nullptr};
    QString m_umbenennenId;
};

} // namespace Longpath
