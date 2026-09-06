#include "DevAutomationServer.h"
#include "LogCategories.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>

#ifdef NEREUS_GPU_SPECTRUM
#include <QRhiWidget>
#endif

#include "ConnectionState.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace Longpath {

namespace {

// Best-effort "current value" for the widget types a driver actually needs
// to assert on. Returns a null QString when the type isn't one of these --
// callers check isNull(), not isEmpty(), so an empty-but-real value (an
// empty QLineEdit) is still reported.
QString automationWidgetValue(const QWidget* w)
{
    if (auto* le = qobject_cast<const QLineEdit*>(w)) {
        return le->text();
    }
    if (auto* pte = qobject_cast<const QPlainTextEdit*>(w)) {
        return pte->toPlainText();
    }
    if (auto* lbl = qobject_cast<const QLabel*>(w)) {
        return lbl->text();
    }
    if (auto* cb = qobject_cast<const QComboBox*>(w)) {
        return cb->currentText();
    }
    if (auto* sl = qobject_cast<const QAbstractSlider*>(w)) {
        return QString::number(sl->value());
    }
    if (auto* sb = qobject_cast<const QSpinBox*>(w)) {
        return QString::number(sb->value());
    }
    if (auto* btn = qobject_cast<const QAbstractButton*>(w); btn && !btn->isCheckable()) {
        return btn->text();
    }
    return {};
}

QImage grabAutomationWidget(QWidget* w)
{
#ifdef NEREUS_GPU_SPECTRUM
    // QRhiWidget::grab() (inherited from QWidget) returns an empty pixmap for
    // a GPU surface -- grabFramebuffer() is the real readback, and reads back
    // whatever was last rendered without forcing a synchronous repaint. Qt
    // 6.7+; Longpath targets 6.11, so this is always available in a
    // GPU-spectrum build.
    if (auto* rhi = qobject_cast<QRhiWidget*>(w)) {
        return rhi->grabFramebuffer();
    }
#endif
    return w->grab().toImage();
}

// The class name without a "Longpath::" (or any) namespace prefix -- the
// bare name is what a caller actually types ("SpectrumWidget"), matching
// AetherSDR's own convention for the same fallback.
QString shortClassName(const QWidget* w)
{
    const QString full = QString::fromUtf8(w->metaObject()->className());
    const int sep = full.lastIndexOf(QStringLiteral("::"));
    return sep < 0 ? full : full.mid(sep + 2);
}

// Depth-first search for the first widget (in any top-level window) whose
// objectName matches exactly, falling back to an exact (namespace-stripped)
// class-name match -- e.g. "SpectrumWidget" resolves the live panadapter,
// which carries no objectName of its own. Confirmed necessary by dogfooding
// against the real app (2026-09-06): the panadapter is exactly the widget an
// agent most wants to grab, and it has no objectName to look it up by.
// Phase 0 stops here -- no accessibleName fallback, no scoped "Scope/Name"
// disambiguation, no "first visible wins" tie-break for duplicate classes
// (SpectrumWidget instances are unique today; multi-panadapter needs this
// widened, a Phase 1 concern once #3F work makes duplicates real).
QWidget* resolveAutomationTarget(QWidget* root, const QString& name)
{
    if (root->objectName() == name) {
        return root;
    }
    const QList<QWidget*> kids = root->findChildren<QWidget*>(
        QString(), Qt::FindChildrenRecursively);
    for (QWidget* k : kids) {
        if (k->objectName() == name) {
            return k;
        }
    }
    if (shortClassName(root) == name) {
        return root;
    }
    for (QWidget* k : kids) {
        if (shortClassName(k) == name) {
            return k;
        }
    }
    return nullptr;
}

} // namespace

QJsonObject describeAutomationWidget(const QWidget* w)
{
    QJsonObject o;
    if (!w) { return o; }

    o[QStringLiteral("class")] = QString::fromUtf8(w->metaObject()->className());
    if (!w->objectName().isEmpty()) {
        o[QStringLiteral("objectName")] = w->objectName();
    }
    if (!w->accessibleName().isEmpty()) {
        o[QStringLiteral("accessibleName")] = w->accessibleName();
    }
    if (!w->toolTip().isEmpty()) {
        o[QStringLiteral("toolTip")] = w->toolTip();
    }
    o[QStringLiteral("enabled")] = w->isEnabled();
    o[QStringLiteral("visible")] = w->isVisible();

    const QPoint gp = w->mapToGlobal(QPoint(0, 0));
    o[QStringLiteral("geometry")] = QJsonObject{
        {QStringLiteral("x"), gp.x()}, {QStringLiteral("y"), gp.y()},
        {QStringLiteral("w"), w->width()}, {QStringLiteral("h"), w->height()},
    };

    // A checkable button's own text names which control it is -- "checked"
    // alone doesn't (the six DSP-style method buttons in a row would all
    // look identical without it).
    if (auto* b = qobject_cast<const QAbstractButton*>(w); b && b->isCheckable()) {
        if (!b->text().isEmpty()) {
            o[QStringLiteral("text")] = b->text();
        }
        o[QStringLiteral("checked")] = b->isChecked();
    }

    if (auto* sl = qobject_cast<const QAbstractSlider*>(w)) {
        o[QStringLiteral("range")] = QJsonObject{
            {QStringLiteral("min"), sl->minimum()}, {QStringLiteral("max"), sl->maximum()}};
    } else if (auto* sb = qobject_cast<const QSpinBox*>(w)) {
        o[QStringLiteral("range")] = QJsonObject{
            {QStringLiteral("min"), sb->minimum()}, {QStringLiteral("max"), sb->maximum()}};
    }

    if (auto* cb = qobject_cast<const QComboBox*>(w)) {
        QJsonArray items;
        for (int i = 0; i < cb->count(); ++i) { items.append(cb->itemText(i)); }
        o[QStringLiteral("items")] = items;
    }

    const QString val = automationWidgetValue(w);
    if (!val.isNull()) {
        o[QStringLiteral("value")] = val;
    }

    return o;
}

namespace {

QJsonObject describeSubtree(const QWidget* w)
{
    QJsonObject node = describeAutomationWidget(w);
    QJsonArray children;
    for (const QObject* child : w->children()) {
        if (auto* cw = qobject_cast<const QWidget*>(child)) {
            children.append(describeSubtree(cw));
        }
    }
    if (!children.isEmpty()) {
        node[QStringLiteral("children")] = children;
    }
    return node;
}

} // namespace

DevAutomationServer::DevAutomationServer(QObject* parent)
    : QObject(parent)
{
}

DevAutomationServer::~DevAutomationServer()
{
    stop();
}

bool DevAutomationServer::start()
{
    if (m_server) { return isListening(); }

    QString socketName = qEnvironmentVariable("LONGPATH_AUTOMATION_SOCKET");
    if (socketName.isEmpty()) {
        socketName = QStringLiteral("longpath-automation");
    }

    m_server = new QLocalServer(this);
    // Drop a stale socket left behind by a killed prior instance before
    // trying to listen -- same recovery AetherSDR's own bridge needed.
    QLocalServer::removeServer(socketName);
    if (!m_server->listen(socketName)) {
        qCWarning(lcAutomation) << "DevAutomationServer: could not listen on"
                                 << socketName << "-" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QLocalServer::newConnection,
            this, &DevAutomationServer::onNewConnection);
    qCInfo(lcAutomation) << "DevAutomationServer: listening on" << socketName;
    return true;
}

void DevAutomationServer::stop()
{
    if (!m_server) { return; }
    m_server->close();
    m_server->deleteLater();
    m_server = nullptr;
}

bool DevAutomationServer::isListening() const
{
    return m_server && m_server->isListening();
}

void DevAutomationServer::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket* sock = m_server->nextPendingConnection();
        connect(sock, &QLocalSocket::readyRead, this, &DevAutomationServer::onReadyRead);
        connect(sock, &QLocalSocket::disconnected, sock, &QLocalSocket::deleteLater);
    }
}

void DevAutomationServer::onReadyRead()
{
    auto* sock = qobject_cast<QLocalSocket*>(sender());
    if (!sock) { return; }

    while (sock->canReadLine()) {
        const QByteArray line = sock->readLine().trimmed();
        if (line.isEmpty()) { continue; }
        const QJsonObject reply = handleLine(line);
        sock->write(QJsonDocument(reply).toJson(QJsonDocument::Compact));
        sock->write("\n");
    }
    sock->flush();
}

QJsonObject DevAutomationServer::handleLine(const QByteArray& line)
{
    const QString text = QString::fromUtf8(line);
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("\\s+")),
                                          Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return QJsonObject{{QStringLiteral("ok"), false},
                            {QStringLiteral("error"), QStringLiteral("empty command")}};
    }

    const QString verb = parts.first();
    if (verb == QStringLiteral("ping")) {
        return doPing();
    }
    if (verb == QStringLiteral("dumpTree")) {
        return doDumpTree();
    }
    if (verb == QStringLiteral("grab")) {
        if (parts.size() < 2) {
            return QJsonObject{{QStringLiteral("ok"), false},
                                {QStringLiteral("error"), QStringLiteral("grab needs a target")}};
        }
        return doGrab(parts.at(1));
    }
    if (verb == QStringLiteral("get")) {
        if (parts.size() < 2) {
            return QJsonObject{{QStringLiteral("ok"), false},
                                {QStringLiteral("error"), QStringLiteral("get needs a model (radio|slice)")}};
        }
        return doGet(parts.at(1), parts.size() > 2 ? parts.at(2) : QString());
    }

    return QJsonObject{{QStringLiteral("ok"), false},
                        {QStringLiteral("error"),
                         QStringLiteral("unknown command: ") + verb +
                             QStringLiteral(" (known: ping, dumpTree, grab, get)")}};
}

QJsonObject DevAutomationServer::doPing() const
{
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("pong"), true}};
}

QJsonObject DevAutomationServer::doDumpTree() const
{
    QJsonArray windows;
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (!top) { continue; }
        windows.append(describeSubtree(top));
    }
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("windows"), windows}};
}

QJsonObject DevAutomationServer::doGrab(const QString& target) const
{
    QWidget* found = nullptr;
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if ((found = resolveAutomationTarget(top, target))) { break; }
    }
    if (!found) {
        return QJsonObject{{QStringLiteral("ok"), false},
                            {QStringLiteral("error"),
                             QStringLiteral("no widget with objectName or class '") + target + QStringLiteral("'")}};
    }

    const QImage img = grabAutomationWidget(found);
    if (img.isNull()) {
        return QJsonObject{{QStringLiteral("ok"), false},
                            {QStringLiteral("error"), QStringLiteral("grab produced an empty image")}};
    }

    QString safe = target;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    const QString outPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("longpath-grab-") + safe + QStringLiteral(".png"));
    if (!img.save(outPath, "PNG")) {
        return QJsonObject{{QStringLiteral("ok"), false},
                            {QStringLiteral("error"), QStringLiteral("failed to write PNG: ") + outPath}};
    }

    qCInfo(lcAutomation) << "grabbed" << found->metaObject()->className()
                          << "->" << outPath << img.size();

    return QJsonObject{
        {QStringLiteral("ok"), true},
        {QStringLiteral("target"), target},
        {QStringLiteral("class"), QString::fromUtf8(found->metaObject()->className())},
        {QStringLiteral("path"), outPath},
        {QStringLiteral("width"), img.width()},
        {QStringLiteral("height"), img.height()},
        {QStringLiteral("bytes"), static_cast<qint64>(QFileInfo(outPath).size())},
    };
}

namespace {

// RadioModel is a QObject child somewhere under MainWindow, not a widget
// itself -- findChild reaches it the same way tests do
// (tst_dev_automation_server.cpp and friends already rely on this being
// discoverable this way). Returns nullptr before a MainWindow exists yet,
// which callers must treat as "no radio", not an error.
RadioModel* findAutomationRadioModel()
{
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (auto* rm = top->findChild<RadioModel*>()) {
            return rm;
        }
    }
    return nullptr;
}

QJsonObject describeAutomationSlice(const SliceModel* s)
{
    if (!s) { return QJsonObject{}; }
    return QJsonObject{
        {QStringLiteral("frequencyHz"), s->frequency()},
        {QStringLiteral("mode"), SliceModel::modeName(s->dspMode())},
        {QStringLiteral("filterLowHz"), s->filterLow()},
        {QStringLiteral("filterHighHz"), s->filterHigh()},
        {QStringLiteral("band"), bandKeyName(bandFromFrequency(s->frequency()))},
        {QStringLiteral("rxAntenna"), s->rxAntenna()},
    };
}

} // namespace

QJsonObject DevAutomationServer::doGet(const QString& model, const QString& selector) const
{
    RadioModel* radio = findAutomationRadioModel();
    if (!radio) {
        return QJsonObject{{QStringLiteral("ok"), false},
                            {QStringLiteral("error"), QStringLiteral("no RadioModel found (no MainWindow yet?)")}};
    }

    if (model == QStringLiteral("radio")) {
        QJsonObject o{
            {QStringLiteral("ok"), true},
            {QStringLiteral("connectionState"), connectionStateName(radio->connectionState())},
            {QStringLiteral("model"), radio->model()},
        };
        if (SliceModel* active = radio->activeSlice()) {
            o[QStringLiteral("activeSliceIndex")] = active->sliceIndex();
        }
        return o;
    }

    if (model == QStringLiteral("slice")) {
        SliceModel* s = nullptr;
        if (selector.isEmpty() || selector == QStringLiteral("active")) {
            s = radio->activeSlice();
        } else {
            bool okIndex = false;
            const int id = selector.toInt(&okIndex);
            if (okIndex) { s = radio->sliceById(id); }
        }
        if (!s) {
            return QJsonObject{{QStringLiteral("ok"), false},
                                {QStringLiteral("error"), QStringLiteral("no such slice: ") + selector}};
        }
        QJsonObject o = describeAutomationSlice(s);
        o[QStringLiteral("ok")] = true;
        o[QStringLiteral("sliceIndex")] = s->sliceIndex();
        return o;
    }

    return QJsonObject{{QStringLiteral("ok"), false},
                        {QStringLiteral("error"),
                         QStringLiteral("unknown get model: ") + model +
                             QStringLiteral(" (known: radio, slice)")}};
}

} // namespace Longpath
