#pragma once

// =================================================================
// src/gui/widgets/WidgetPicker.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Das Plus ─────────────────────────────────────────────────────────
//
// OE5SOS, 2026-08-15:
//
//   „Ich möchte mit einem PLUS Widget hinzufügen können und entfernen,
//    um so mein eigenes Profil selbst zu gestalten."
//
// Hinzufügen und Entfernen gab es schon — AppletVisibilityController
// blendet jedes Widget ein und aus. Was fehlte, war die AUFFORDERUNG:
// es steckte im Menü „Containers", und ein Menüeintrag lädt niemanden
// ein, sein Fenster umzubauen. Ein Plus schon.
//
// ── Die Form kommt aus der Vorlage ───────────────────────────────────
//
// Die erste Fassung war eine flache Hakenliste. Der Screenshot des
// Betreibers zeigt etwas anderes:
//
//     ADD PANEL                                              ✕
//     ┌──────────┬──────────────────────────────────────────┐
//     │ ALL      │  Search panels…                          │
//     │ SPECTRUM │  ┌────────────────────────────────────┐  │
//     │ METERS   │  │ TX Stage Meters                    │  │
//     │ DSP      │  │ tx · power · swr · alc · meters    │  │
//     │ LOG      │  └────────────────────────────────────┘  │
//     └──────────┴──────────────────────────────────────────┘
//
// Kategorien links, Suchfeld oben, Karten mit fettem Namen und einer
// Schlagwortzeile darunter. Die Schlagwörter sind der Grund, warum das
// Suchfeld etwas taugt: wer „swr" tippt, findet die Messanzeigen, ohne
// ihren Namen zu kennen. Ein Suchfeld über Titeln findet nur das, was
// man ohnehin schon gefunden hätte.
//
// ── Zwei Ebenen, wie bei Zeus ────────────────────────────────────────
//
// Der Controller kennt zwei Achsen, und die Vorlage benutzt beide:
// „Features" entscheidet, was es überhaupt gibt, „Add Panel"
// entscheidet, was davon auf dem Schirm liegt. Ein abgeschaltetes
// Feature steht hier trotzdem drin — mit ABGESCHALTET und dem Hinweis,
// wo man es einschaltet. Ein Plus, das etwas anbietet und dann nichts
// tut, ist schlimmer als eines, das sagt warum.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-15 — Aus der flachen Hakenliste wurde der Dialog aus der
//                 Vorlage: Kategorien, Suche, Karten.
// =================================================================

#include <QHash>
#include <functional>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

class QLineEdit;
class QListWidget;
class QVBoxLayout;

namespace Longpath {

class AppletVisibilityController;

class WidgetPicker : public QWidget {
    Q_OBJECT
public:
    explicit WidgetPicker(AppletVisibilityController* vis,
                          QWidget* parent = nullptr);

    /// Zustand nachziehen, ohne die Karten neu zu bauen.
    void refresh();

    // ── Filter ───────────────────────────────────────────────────────

    /// Kategorie wählen. allCategory() zeigt alles.
    void setCategory(const QString& category);
    QString currentCategory() const { return m_category; }
    /// Die Kategorien der Spalte links, mit allCategory() vorne.
    QStringList categoryColumn() const;

    void setSearch(const QString& needle);
    QString search() const { return m_needle; }

    static QString allCategory();

    // ── Für Tests ────────────────────────────────────────────────────

    /// Die gerade sichtbaren Karten, in Anmeldereihenfolge.
    QStringList entries() const;
    bool isChecked(const QString& id) const;
    bool isEnabled(const QString& id) const;
    /// Eine Karte auslösen, als hätte jemand geklickt. false, wenn sie
    /// nicht sichtbar oder nicht bedienbar ist.
    bool toggle(const QString& id);

signals:
    void toggled(const QString& id, bool visible);

private:
    struct Card {
        QWidget* frame{nullptr};
        QString id;
        bool enabled{false};
        bool checked{false};
        // Der Klick liegt hier, nicht auf dem Widget. Sonst braeuchte
        // toggle() ein qobject_cast auf den Kartentyp — und der lebt in
        // einem anonymen Namensraum in der .cpp und hat kein Q_OBJECT.
        // Ein static_cast waere die Alternative gewesen: richtig,
        // solange niemand einen zweiten Widget-Typ in diese Liste haengt.
        std::function<void()> click;
    };

    void rebuild();
    void applyFilter();
    Card* card(const QString& id);
    const Card* card(const QString& id) const;

    QPointer<AppletVisibilityController> m_vis;
    QListWidget* m_categories{nullptr};
    QLineEdit* m_search{nullptr};
    QVBoxLayout* m_cards{nullptr};
    QHash<QString, Card> m_byId;
    QStringList m_order;
    QString m_category;
    QString m_needle;
};

/// Das Plus. Ein Knopf mit gestricheltem Rand, der den Dialog aufklappt
/// — bei Zeus sitzt er unten in der Seitenschiene.
class AddWidgetButton : public QWidget {
    Q_OBJECT
public:
    explicit AddWidgetButton(AppletVisibilityController* vis,
                             QWidget* parent = nullptr);

    void openPicker();
    WidgetPicker* picker() const { return m_picker; }

    /// Wo der Auswähler aufgehen soll: unter dem Plus, rechtsbündig,
    /// und in den sichtbaren Bildschirm geschoben. Öffentlich, damit
    /// ein Test die Klemmung prüfen kann, ohne ein Fenster zu zeigen.
    QPoint placeNear(const QSize& want) const;

    static constexpr int kSide = 34;

signals:
    void toggled(const QString& id, bool visible);

private:
    QPointer<AppletVisibilityController> m_vis;
    WidgetPicker* m_picker{nullptr};
};

} // namespace Longpath
