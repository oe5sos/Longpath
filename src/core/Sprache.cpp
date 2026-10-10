// =================================================================
// src/core/Sprache.cpp  (Longpath)
// =================================================================
// Siehe Sprache.h. Die Tabelle selbst steht in SpracheTabelle.cpp.
//
// Modification history (Longpath):
//   2026-10-10 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "core/Sprache.h"

#include "core/AppSettings.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QTabWidget>
#include <QWidget>

namespace Longpath::Sprache {

// Aus SpracheTabelle.cpp.
const QHash<QString, QString>& tabelle();

namespace {

Lang g_lang = Lang::Deutsch;
bool g_gelesen = false;

void einmalLesen()
{
    if (g_gelesen) { return; }
    g_gelesen = true;
    const QString v = AppSettings::instance()
                          .value(QStringLiteral("Sprache"),
                                 QStringLiteral("de")).toString();
    g_lang = (v.compare(QStringLiteral("en"), Qt::CaseInsensitive) == 0)
                 ? Lang::English : Lang::Deutsch;
}

} // namespace

Lang sprache()
{
    einmalLesen();
    return g_lang;
}

void setzeSprache(Lang l)
{
    einmalLesen();
    g_lang = l;
    AppSettings::instance().setValue(
        QStringLiteral("Sprache"),
        l == Lang::English ? QStringLiteral("en") : QStringLiteral("de"));
}

QString T(const QString& englisch)
{
    if (sprache() == Lang::English) { return englisch; }
    const auto it = tabelle().constFind(englisch);
    return it != tabelle().constEnd() ? it.value() : englisch;
}

int eintraege() { return tabelle().size(); }

bool hatUebersetzung(const QString& englisch)
{
    return tabelle().contains(englisch);
}

void uebersetzeBaum(QWidget* wurzel)
{
    if (wurzel == nullptr || sprache() == Lang::English) { return; }

    // Den Titel des Fensters selbst, nicht nur der Kinder.
    if (!wurzel->windowTitle().isEmpty()) {
        wurzel->setWindowTitle(T(wurzel->windowTitle()));
    }

    const auto kurzhilfe = [](QWidget* w) {
        if (!w->toolTip().isEmpty()) { w->setToolTip(T(w->toolTip())); }
    };

    for (QWidget* w : wurzel->findChildren<QWidget*>()) {
        kurzhilfe(w);

        if (auto* l = qobject_cast<QLabel*>(w)) {
            if (!l->text().isEmpty()) { l->setText(T(l->text())); }
            continue;
        }
        if (auto* b = qobject_cast<QAbstractButton*>(w)) {
            if (!b->text().isEmpty()) { b->setText(T(b->text())); }
            continue;
        }
        if (auto* g = qobject_cast<QGroupBox*>(w)) {
            if (!g->title().isEmpty()) { g->setTitle(T(g->title())); }
            continue;
        }
        if (auto* c = qobject_cast<QComboBox*>(w)) {
            // Nur der angezeigte Text; die hinterlegten Daten
            // (itemData) bleiben unberührt -- an ihnen haengen die
            // Einstellungen.
            for (int i = 0; i < c->count(); ++i) {
                c->setItemText(i, T(c->itemText(i)));
            }
            continue;
        }
        if (auto* t = qobject_cast<QTabWidget*>(w)) {
            for (int i = 0; i < t->count(); ++i) {
                t->setTabText(i, T(t->tabText(i)));
            }
            continue;
        }
    }
    kurzhilfe(wurzel);
}

} // namespace Longpath::Sprache
