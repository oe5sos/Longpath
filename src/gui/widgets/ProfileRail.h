#pragma once

// =================================================================
// src/gui/widgets/ProfileRail.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Schiene ──────────────────────────────────────────────────────
//
// OE5SOS, 2026-08-15:
//
//   „Wenn dies alles erledigt ist, soll die Möglichkeit entstehen, ein
//    zweites Profil oder auch ein drittes anzulegen, welches ich mir
//    selbst wieder individuell gestalten kann."
//
// In der Vorlage sind das runde Abzeichen am linken Rand — ein Buchstabe je
// Arbeitsfläche, darunter ein gestricheltes Plus. Genau so hier.
//
// ── Warum ein Buchstabe und nicht der Name ───────────────────────────
//
// Die Schiene ist 44 Pixel breit; ein Name passt nicht hinein. Das
// Abzeichen trägt den ersten Buchstaben, der volle Name steht im
// Tooltip und im Rechtsklickmenü. Das ist kein Kompromiss, sondern der
// Punkt: die Schiene soll man mit dem Augenwinkel lesen, nicht
// studieren.
//
// ── Was die Schiene NICHT tut ────────────────────────────────────────
//
// Sie schaltet nicht von selbst um. LayoutProfiles::profileFor() kann
// ein Profil an Band und Modus binden, und die Verdrahtung wäre ein
// Signalanschluss — sie bleibt aus, weil der Betreiber es so
// entschieden hat (2026-08-15): „Nein, nur von Hand. Kein Fenster, das
// sich beim Bandwechsel unter dir verändert."
//
// Die Fenstergröße gehört ebenfalls nicht dazu, aus demselben Grund:
// ein Profilwechsel soll Panels tauschen und nicht das Fenster
// umspringen lassen.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QHash>
#include <QPointer>
#include <QString>
#include <QWidget>

class QBoxLayout;
class QPushButton;

namespace Longpath {

class LayoutProfiles;

class ProfileRail : public QWidget {
    Q_OBJECT
public:
    /// Waagrecht oder senkrecht. Senkrecht ist die Schiene am linken
    /// Rand, wie seit 2026-08-15; waagrecht sitzt sie in der
    /// Kommandoleiste neben der Rate.
    ///
    /// Betreiber am 2026-10-06: „meine profile links im eck sollten oben
    /// in die taksleite neben 48 khz". Es ist DASSELBE Bauteil, nur
    /// anders gelegt -- damit bleiben Rechtsklickmenue, das X am
    /// Abzeichen und das gestrichelte Plus erhalten, statt sie in einer
    /// zweiten Fassung nachzubauen.
    /// Die Richtung hat KEINEN Vorgabewert, mit Absicht: mit einem
    /// band der alte Aufruf ProfileRail(profiles, parent) das Elternteil
    /// stillschweigend an die Richtung, und der Fehler faellt erst im
    /// Bild auf. Ohne Vorgabe meldet ihn der Uebersetzer
    /// (2026-10-06 in tst_menus_of_native_fields genau so passiert).
    explicit ProfileRail(LayoutProfiles* profiles,
                         Qt::Orientation richtung,
                         QWidget* parent = nullptr);

    /// Wie die Schiene gerade liegt.
    Qt::Orientation richtung() const { return m_richtung; }

    /// Abzeichen neu aufbauen. Wird auf profilesChanged() gerufen.
    void rebuild();

    // ── Für Tests ────────────────────────────────────────────────────

    /// Die Namen der Abzeichen, von oben nach unten.
    QStringList badges() const;
    /// Das Umhaengen melden, ohne das Menue zu oeffnen -- damit ein
    /// Pruefstand den INHALT der Meldung sehen kann und nicht nur, dass
    /// es das Signal gibt.
    void meldeUmhaengenForTest()
    {
        emit placementToggleRequested(m_richtung == Qt::Horizontal
                                          ? Qt::Vertical : Qt::Horizontal);
    }

    /// Der Name des hervorgehobenen Abzeichens, oder leer.
    QString activeBadge() const;
    /// Ein Abzeichen anklicken. false, wenn es das nicht gibt.
    bool clickBadge(const QString& name);
    /// Der Beschriftungsbuchstabe eines Namens — auch für Namen, die
    /// mit Leerzeichen oder einem Sonderzeichen anfangen.
    static QString initialFor(const QString& name);

    static constexpr int kWidth      = 44;
    static constexpr int kBadgeSide  = 30;
    /// Die Hoehe der SCHIENE in der Leiste. Dort steht sie neben den
    /// Pillen von CommandBar, und Scheiben von 30 px zwischen
    /// Rechtecken liessen sich als Fremdkoerper lesen -- genau das war
    /// am 2026-10-07 der Befund am Bildschirm.
    ///
    /// Es sind zwei Pixel mehr als CommandBar::kPillHeight, und das ist
    /// kein Versehen: in Qt gilt `min-height` im Stilblatt fuer die
    /// INHALTSflaeche, der 1 px starke Rand kommt oben und unten dazu.
    /// Eine Pille mit kPillHeight = 27 ist also 29 Pixel hoch. Die
    /// Schiene auf 27 festzunageln hiesse, die Abzeichen zwei Pixel
    /// flacher als ihre Nachbarn zu machen.
    ///
    /// Die Abzeichen selbst bekommen hier KEINE Hoehe gesetzt -- die
    /// kommt aus pillStyle(), damit sie gar nicht erst abweichen kann.
    /// tst_profilschiene_richtung stellt Schiene, Abzeichen und eine
    /// echte Pille nebeneinander, damit diese Zahl nicht stillschweigend
    /// verrutscht.
    static constexpr int kLeistenSeite = 29;

signals:
    /// Der Betreiber will ein neues Profil. Den Namen erfragt der
    /// Empfänger — die Schiene kennt keinen Dialog, damit sie ohne
    /// Oberfläche prüfbar bleibt.
    /// Der Betreiber will die Schiene woanders haben. Die Schiene legt
    /// sich NICHT selbst um -- sie haengt im Aufbau des Hauptfensters,
    /// und ein Bauteil, das sich im Rechtsklick selbst umhaengt, waere
    /// an zwei Stellen gleichzeitig zustaendig. Also meldet sie nur.
    void placementToggleRequested(Qt::Orientation neueRichtung);

    void newProfileRequested();
    void renameRequested(const QString& name);
    void duplicateRequested(const QString& name);
    void removeRequested(const QString& name);
    /// Betreiber 2026-08-28: "vielleicht sollte es ein profil speichern
    /// auch geben" -- der jetzige Stand sichert sich sonst nur beim
    /// Umschalten oder Beenden. Wer ohne Verbindung gestaltet und gleich
    /// wissen will, dass es sitzt, statt der Beenden-Sequenz zu
    /// vertrauen, kann hier von Hand nachhelfen.
    void saveRequested(const QString& name);
    /// Betreiber 2026-08-30: "sollten auf dem Desktop zur Sicherheit
    /// abspeicherbar sein" -- eine Sicherung neben AppSettings' XML, die
    /// man sehen, verschieben und einem Backup beilegen kann. Die Schiene
    /// kennt wie bei saveRequested() keinen Dateidialog; das erledigt der
    /// Empfaenger.
    void exportRequested(const QString& name);
    /// Betreiber 2026-08-30: "DIE gespeicherten profile sollte man auch
    /// mit rechter moustaste importiren können" -- das Gegenstueck zu
    /// exportRequested(). Traegt den Namen wie exportRequested(): ein
    /// Import ERSETZT den Zustand DIESES Profils (Betreiber, selber
    /// Tag: "wenn ich importiere will ich es nicht als neues profil
    /// importiren").
    void importRequested(const QString& name);

private:
    void showMenuFor(const QString& name, const QPoint& globalPos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // Je Abzeichen ein Loeschkreuz, sichtbar beim Ueberfahren.
    QHash<QString, QPushButton*> m_closers;

    QPointer<LayoutProfiles> m_profiles;
    /// Quer zur Laufrichtung mittig: senkrecht waagrecht zentriert,
    /// waagrecht senkrecht zentriert.
    Qt::Alignment mittig() const
    {
        return m_richtung == Qt::Vertical ? Qt::AlignHCenter : Qt::AlignVCenter;
    }

    /// Die Kantenlaenge eines Abzeichens am jetzigen Platz. Am Rand die
    /// Scheibe, in der Leiste die Pillenhoehe -- siehe kLeistenSeite.
    int seite() const
    {
        return m_richtung == Qt::Vertical ? kBadgeSide : kLeistenSeite;
    }

    /// Hoehe und Breite eines Knopfes am jetzigen Platz. Begruendung
    /// an der Umsetzung -- kurz: in der Leiste darf die Breite NICHT
    /// fest sein, sonst schneidet der geerbte Innenabstand die Schrift ab.
    void setzeMasse(QPushButton* b) const;

    QBoxLayout*  m_column{nullptr};   // VBox oder HBox, je nach Richtung
    Qt::Orientation m_richtung{Qt::Vertical};
    QPushButton* m_plus{nullptr};
    QHash<QString, QPushButton*> m_badges;
    QStringList m_order;
};

} // namespace Longpath
