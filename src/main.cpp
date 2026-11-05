#include <csignal>
#include <memory>

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QTranslator>
#include <QMessageBox>
#include <QInputDialog>
#include <QComboBox>
#include <QAbstractButton>
#include <QPushButton>
#include <QTimer>
#include <QStandardPaths>
#include <QLocalSocket>
#include <QLocalServer>
#include <QLockFile>
#include <QThread>
#include <QDateTime>
#include <3rdparty/WinCommander.hpp>


#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/configs/generate.h"
#include "include/configs/ControlledClaudeProxy.h"
#include "include/database/RoutesRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/GroupsRepo.h"
#include "include/ui/setting/RawRouteItem.h"
#include <QJsonDocument>
#include <QJsonArray>

#include "include/ui/mainwindow_interface.h"
#include "include/stats/traffic/TrafficStatsManager.hpp"
#include "include/api/RPC.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include "include/sys/windows/MiniDump.h"
#include "include/sys/windows/eventHandler.h"
#include "include/sys/windows/WinVersion.h"
#include "include/sys/windows/guihelper.h"
#include <qfontdatabase.h>
#endif
#ifdef Q_OS_LINUX
#include <include/sys/linux/coreDump.h>
#include <qfontdatabase.h>
#include <QSocketNotifier>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#ifdef Q_OS_MACOS
#include <QFileOpenEvent>

// On macOS the OS reuses the running app and delivers quattro:// URLs, as well as
// files opened with the app, as a QFileOpenEvent to the application object (never
// via argv). This filter feeds both into the common pipelines.
class MacOpenEventFilter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::FileOpen) {
            const auto openEvent = static_cast<QFileOpenEvent *>(event);
            const QString url = openEvent->url().toString();
            if (url.startsWith("quattro://")) {
                Deeplink_Submit(url);
                return true;
            }
            const QString file = openEvent->file().isEmpty() ? openEvent->url().toLocalFile() : openEvent->file();
            if (!file.isEmpty()) {
                LaunchFiles_Submit({file});
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }
};
#endif

void signal_handler(int signum) {
    Q_UNUSED(signum)
    if (auto *mw = GetMainWindow()) mw->prepare_exit();
    qApp->quit();
}

#ifdef Q_OS_LINUX
namespace {
    int g_signalPipe[2] = {-1, -1};

    // Async-signal-safe: a write() to the self-pipe is all that is allowed here. The
    // teardown itself (Qt widgets, QProcess, SQLite) runs from the notifier below, on
    // the main thread, so a session-manager SIGTERM can no longer be delivered on a
    // worker thread or re-enter a lock the interrupted thread was already holding.
    void posix_signal_handler(int signum) {
        const auto byte = static_cast<char>(signum);
        [[maybe_unused]] const ssize_t written = ::write(g_signalPipe[1], &byte, 1);
    }

    void install_termination_handlers() {
        if (::pipe(g_signalPipe) != 0) {
            // Without the pipe, the unsafe direct handler still beats no handler at all.
            signal(SIGTERM, signal_handler);
            signal(SIGINT, signal_handler);
            return;
        }
        for (const int fd : g_signalPipe) {
            ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
            // Non-blocking: a full pipe must fail the write, never block in signal context.
            ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
        }

        auto *notifier = new QSocketNotifier(g_signalPipe[0], QSocketNotifier::Read, qApp);
        QObject::connect(notifier, &QSocketNotifier::activated, qApp, [notifier] {
            notifier->setEnabled(false); // one teardown is enough; later signals just fill the pipe
            char drain[16];
            while (::read(g_signalPipe[0], drain, sizeof(drain)) > 0) {}
            signal_handler(0);
        });

        struct sigaction sa{};
        sa.sa_handler = posix_signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGTERM, &sa, nullptr);
        sigaction(SIGINT, &sa, nullptr);
    }
}
#endif

QTranslator* trans = nullptr;
QTranslator* trans_qt = nullptr;

void loadTranslate(const QString& locale) {
    QT_TRANSLATE_NOOP("QPlatformTheme", "Cancel");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Apply");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Yes");
    QT_TRANSLATE_NOOP("QPlatformTheme", "No");
    QT_TRANSLATE_NOOP("QPlatformTheme", "OK");
    if (trans != nullptr) {
        trans->deleteLater();
    }
    if (trans_qt != nullptr) {
        trans_qt->deleteLater();
    }
    trans = new QTranslator;
    trans_qt = new QTranslator;
    QLocale::setDefault(QLocale(locale));
    //
    const QString diskPath = QCoreApplication::applicationDirPath()+"/translations/" + locale + ".qm";
    const QString qrcPath = ":/translations/" + locale + ".qm";
    bool loadOK=false;
    if (QFileInfo::exists(diskPath)) {
        loadOK = trans->load(diskPath);
    }
    if (!loadOK) {
        loadOK = trans->load(qrcPath);
    }
    if (loadOK) {
        QCoreApplication::installTranslator(trans);
    }
}

namespace {
    constexpr auto FALLBACK_MARKER = "config/.install-dir-unwritable";

    // QFileInfo::isWritable reports the read-only attribute, not what a UAC-filtered
    // token may actually do under Program Files.
    bool DirIsWritable(const QDir &dir) {
        if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) return false;
        QFile probe(dir.absoluteFilePath(".quattro-write-test"));
        if (!probe.open(QIODevice::WriteOnly)) return false;
        probe.close();
        probe.remove();
        return true;
    }

    bool ConfigDirIsUsable(const QDir &configDir) {
        if (!DirIsWritable(configDir)) return false;
        const QString db = configDir.absoluteFilePath("quattro.db");
        if (!QFile::exists(db)) return true;
        QFile file(db);
        return file.open(QIODevice::ReadWrite);
    }

    void CopyDirContents(const QString &from, const QString &to) {
        QDir().mkpath(to);
        QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            it.next();
            const QString target = QDir(to).absoluteFilePath(it.fileName());
            if (it.fileInfo().isDir()) CopyDirContents(it.filePath(), target);
            else if (!QFile::exists(target)) QFile::copy(it.filePath(), target);
        }
    }

    // An elevated relaunch finds the install dir writable again, so the fallback is
    // pinned by a marker or the two runs land on different databases.
    bool AdoptUserConfigDir(const QDir &installWd, const QDir &userWd) {
        QFile marker(userWd.absoluteFilePath(FALLBACK_MARKER));
        if (marker.open(QIODevice::ReadOnly)) {
            const bool pinnedHere = QString::fromUtf8(marker.readAll()).trimmed() == installWd.absolutePath();
            marker.close();
            if (pinnedHere) return true;
        }

        const QString installConfig = installWd.absoluteFilePath("config");
        if (ConfigDirIsUsable(QDir(installConfig))) return false;

        const QString userConfig = userWd.absoluteFilePath("config");
        QDir().mkpath(userConfig);
        if (!QFile::exists(userConfig + "/quattro.db") && QFile::exists(installConfig + "/quattro.db")) {
            CopyDirContents(installConfig, userConfig);
            LOG_WARN(QString("copied existing config from %1").arg(installConfig));
        }
        if (marker.open(QIODevice::WriteOnly)) {
            marker.write(installWd.absolutePath().toUtf8());
            marker.close();
        }
        LOG_WARN(QString("%1 is not writable, using %2").arg(installConfig, userConfig));
        return true;
    }
} // namespace

#define LOCAL_SERVER_PREFIX "quattro-"

// Portable previews may reopen only a directory originally initialized by this
// mode. An unrelated nonempty directory is never adopted as an app profile.
static bool ensurePortablePreviewDirectory(const QDir &dir) {
    const QString markerPath = dir.filePath(".mintvpn-preview-profile");
    const QFileInfo markerInfo(markerPath);
    if (markerInfo.isSymLink() || markerInfo.isJunction()) return false;
    QFile marker(markerPath);
    if (markerInfo.exists()) {
        if (!markerInfo.isFile() || !marker.open(QIODevice::ReadOnly)) return false;
        return marker.readAll() == QByteArray("MintVPN portable preview profile v1\n");
    }
    if (!dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).isEmpty()) return false;
    if (!marker.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return false;
    const QByteArray value("MintVPN portable preview profile v1\n");
    return marker.write(value) == value.size() && marker.flush();
}

int main(int argc, char* argv[]) {
    // Validate test-only profile location before Qt or any persistent hooks.
    QStringList initialArguments;
    for (int i = 0; i < argc; ++i) initialArguments << QString::fromLocal8Bit(argv[i]);
    const bool initialSmoke = initialArguments.contains("-isolated-smoke") ||
        initialArguments.contains("-isolated-ams-integration") ||
        initialArguments.contains("-isolated-profile-fixture");
    const bool portableFirstRun = initialArguments.contains("-portable-first-run");
    if (initialSmoke || portableFirstRun) {
        const int i = initialArguments.indexOf("-appdata");
        if (initialArguments.size() != 4 || i < 1 || i + 1 >= initialArguments.size() ||
            initialArguments.at(i + 1).startsWith('-')) return 2;
        const QFileInfo path(initialArguments.at(i + 1));
        if (!path.isAbsolute()) return 2;
        for (QFileInfo ancestor = path; ; ancestor = QFileInfo(ancestor.dir().absolutePath())) {
            if (ancestor.isSymLink() || ancestor.isJunction()) return 2;
            if (ancestor.dir().absolutePath() == ancestor.absoluteFilePath()) break;
        }
        const QDir dir(path.absoluteFilePath());
        if (!path.isDir() || (portableFirstRun ? !ensurePortablePreviewDirectory(dir) :
            !dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).isEmpty())) return 2;
        const QByteArray root = path.absoluteFilePath().toLocal8Bit();
        qputenv("APPDATA", root); qputenv("LOCALAPPDATA", root);
        qputenv("TEMP", root); qputenv("TMP", root);
    }
    Logging::InstallQtMessageHandler();

    // Scheduled Task's default priority is BelowNormal. A VPN and its child
    // core must remain responsive while desktop renders occupy the CPU.
#ifdef Q_OS_WIN
    const DWORD processPriority = GetPriorityClass(GetCurrentProcess());
    if (processPriority == IDLE_PRIORITY_CLASS || processPriority == BELOW_NORMAL_PRIORITY_CLASS) {
        if (!SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS))
            qWarning() << "MintVPN could not restore normal process priority:" << GetLastError();
    }
#endif

    // Core dump
#ifdef Q_OS_WIN
    if (!initialSmoke && !portableFirstRun) Windows_SetCrashHandler();
#endif
#ifdef Q_OS_LINUX
    if (!initialSmoke) enable_core_dumps();
#endif

    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication a(argc, argv);

#ifdef Q_OS_MACOS
    // Install before the event loop so launch-by-deeplink FileOpen events are caught.
    a.installEventFilter(new MacOpenEventFilter(&a));
#endif

    // Use the operating system emoji font fallback.

    QStringList arguments = QApplication::arguments();
#ifdef Q_OS_WIN
    const int preflightIndex = arguments.indexOf("-mintvpn-preflight-pipe");
    if (preflightIndex >= 0) {
        if (preflightIndex + 1 >= arguments.size() || arguments.count("-mintvpn-preflight-pipe") != 1 ||
            !Windows_IsInAdmin() || !WinCommander::completeElevatedPreflight(arguments.at(preflightIndex + 1))) return 1;
        arguments.removeAt(preflightIndex + 1);
        arguments.removeAt(preflightIndex);
    }
#endif
    const bool isolatedIntegration = arguments.contains("-isolated-ams-integration");
    const bool isolatedProfileFixture = arguments.contains("-isolated-profile-fixture");
#ifndef MINTVPN_TEST_HARNESS
    if (isolatedIntegration || isolatedProfileFixture) return 2;
#endif
    const bool isolatedSmoke = arguments.contains("-isolated-smoke") || isolatedIntegration || isolatedProfileFixture;
    if ((isolatedSmoke || portableFirstRun) && arguments != initialArguments) return 2;
    // A smoke run must never adopt an existing profile or accept restart flags.
    if (isolatedSmoke || portableFirstRun) {
        const int i = arguments.indexOf("-appdata");
        if (arguments.size() != 4 || i < 1 || i + 1 >= arguments.size() ||
            arguments.at(i + 1).startsWith('-')) return 2;
        const QFileInfo path(arguments.at(i + 1));
        // Validation and actual use must refer to the same directory even when
        // the process changes its working directory below.
        if (!path.isAbsolute()) return 2;
        for (QFileInfo ancestor = path; ; ancestor = QFileInfo(ancestor.dir().absolutePath())) {
            if (ancestor.isSymLink() || ancestor.isJunction()) return 2;
            if (ancestor.dir().absolutePath() == ancestor.absoluteFilePath()) break;
        }
        const QDir dir(path.absoluteFilePath());
        if (!path.isDir() || (portableFirstRun ? !ensurePortablePreviewDirectory(dir) :
            !dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).isEmpty())) return 2;
    }
    // A quattro:// URL may be passed as a launch argument (Windows/Linux), and so may
    // config files opened with the app. Both are delivered after the window is up, or
    // forwarded to the primary instance via the socket below. Files are resolved
    // before the working directory moves, since their paths may be relative to it.
    const QString launchDeeplink = Deeplink_ExtractFromArgs(arguments);
    const QStringList launchFiles = LaunchFiles_ExtractFromArgs(arguments, QDir::current());

    // Clean
    QDir::setCurrent(QApplication::applicationDirPath());
    if (!isolatedSmoke && QFile::exists("QuattroUpdater.old.exe")) {
        QFile::remove("QuattroUpdater.old.exe");
    }
    if (!isolatedSmoke && QFile::exists("MintVPNUpdater.old.exe")) {
        QFile::remove("MintVPNUpdater.old.exe");
    }

    // dirs & clean
    auto wd = QDir(QApplication::applicationDirPath());
    bool useAppdata = false;
    QString appdataDir;
    if (arguments.contains("-appdata")) {
        useAppdata = true;
        int appdataIndex = arguments.indexOf("-appdata");
        if (arguments.size() > appdataIndex + 1 && !arguments.at(appdataIndex + 1).startsWith("-")) {
            appdataDir = arguments.at(appdataIndex + 1);
        }
    }
#ifdef NKR_CPP_USE_APPDATA
    useAppdata = true; // Example: Package & MacOS
#endif
    QApplication::setApplicationName("Quattro");
    QApplication::setApplicationDisplayName("MintVPN");
    if(useAppdata) {
        if (!appdataDir.isEmpty()) {
            wd.setPath(appdataDir);
            // Resolve before switching the process working directory to config/.
            appdataDir = wd.absolutePath();
        } else {
            wd.setPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        }
    } else {
        const QDir userWd(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        if (AdoptUserConfigDir(wd, userWd)) {
            wd = userWd;
            useAppdata = true;
        }
    }
    if (!wd.exists()) wd.mkpath(wd.absolutePath());
    if (!wd.exists("config")) wd.mkdir("config");
    const QString configDir = wd.absoluteFilePath("config");
    // Claim this profile before touching its database, temporary files or settings.
    // The handoff flag lets a replacement wait until the previous process exits.
    QByteArray hashBytes = QCryptographicHash::hash(wd.absolutePath().toUtf8(), QCryptographicHash::Md5).toBase64(QByteArray::OmitTrailingEquals);
    hashBytes.replace('+', '0').replace('/', '1');
    const auto serverName = LOCAL_SERVER_PREFIX + QString::fromUtf8(hashBytes);
    auto notifyExisting = [&]() {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (!socket.waitForConnected(250)) return;
        QStringList payload;
        if (!launchDeeplink.isEmpty()) payload << launchDeeplink;
        for (const auto &file : launchFiles) payload << QUrl::fromLocalFile(file).toString();
        if (!payload.isEmpty()) {
            socket.write(payload.join('\n').toUtf8());
            socket.flush();
            socket.waitForBytesWritten(250);
        }
        socket.disconnectFromServer();
    };
    QLockFile profileLock(QDir(configDir).filePath("mintvpn-instance.lock"));
    if (!isolatedSmoke) {
        const bool handoff = arguments.contains("-mintvpn-wait-profile");
        if (!profileLock.tryLock(handoff ? 30000 : 0)) {
            if (!handoff) notifyExisting();
            return 1;
        }
        // Also detect older builds, which do not yet create this lock file.
        QLocalSocket oldInstance;
        oldInstance.connectToServer(serverName);
        if (oldInstance.waitForConnected(250)) {
            oldInstance.disconnectFromServer();
            notifyExisting();
            return 0;
        }
    }
    QDir::setCurrent(configDir);
    QDir("temp").removeRecursively();

#ifdef MINTVPN_TEST_HARNESS
    if (isolatedProfileFixture) {
        // Only this previously prepared synthetic fixture is accepted. Re-check
        // the copy, so replacement between the read and copy cannot seed a user DB.
        const QString fixture = QDir::temp().filePath("mintvpn-synthetic-profile.db");
        const QByteArray expected("1568738bce739a634d3bf0b73dc9fe4ef91ace0ae8053860e5bd010eff1c812b");
        QFile input(fixture);
        if (!input.open(QIODevice::ReadOnly) ||
            QCryptographicHash::hash(input.readAll(), QCryptographicHash::Sha256).toHex() != expected) return 31;
        input.close();
        if (!QFile::copy(fixture, "quattro.db")) return 32;
        QFile copied("quattro.db");
        if (!copied.open(QIODevice::ReadOnly) ||
            QCryptographicHash::hash(copied.readAll(), QCryptographicHash::Sha256).toHex() != expected) return 33;
    }
#endif

    // Record app start for the Runtime Stats uptime readout.
    appStartEpoch = QDateTime::currentSecsSinceEpoch();

    // Only a newly created portable profile receives safe first-run defaults.
    // A marked profile with an existing database is the user's saved state.
    const bool freshPortableProfile = portableFirstRun && !QFileInfo::exists("quattro.db");

    // Load database
    Configs::initDB(QString(QDir::currentPath() + QDir::separator() + "quattro.db").toStdString());
    if (freshPortableProfile) {
        // A fresh portable profile cannot activate the tunnel or system proxy at startup.
        auto& settings = *Configs::dataManager->settingsRepo;
        settings.remember_enable = false;
        settings.spmode_vpn = false;
        settings.spmode_system_proxy = false;
        settings.remember_tun = false;
        settings.remember_system_proxy = false;
        settings.system_dns_set = false;
        settings.windows_set_admin = false;
        settings.flag_dns_set = false;
        settings.flag_restart_tun_on = false;
    }

    // This harness is only reachable with the same empty absolute appdata checks
    // as the disabled-GUI smoke mode. It does not start the core or any tunnel.
#ifdef MINTVPN_TEST_HARNESS
    if (isolatedProfileFixture) {
        // Exit before normal startup: no core, system proxy, updater or RPC.
        QJsonObject report;
        QJsonArray groups, profiles, routes;
        auto& manager = *Configs::dataManager;
        for (int id : manager.groupsRepo->GetAllGroupIds()) {
            auto item = manager.groupsRepo->GetGroup(id);
            if (!item) return 34;
            groups.append(QJsonObject{{"id", id}, {"name", item->name}});
        }
        for (int id : manager.profilesRepo->GetAllProfileIds()) {
            auto item = manager.profilesRepo->GetProfile(id);
            if (!item) return 35;
            profiles.append(QJsonObject{{"id", id}, {"name", item->name}});
        }
        for (int id : manager.routesRepo->GetAllRouteProfileIds()) {
            auto item = manager.routesRepo->GetRouteProfile(id);
            if (!item) return 36;
            routes.append(QJsonObject{{"id", id}, {"name", item->name}, {"rules", int(item->Rules.size())}});
        }
        report.insert("groups", groups);
        report.insert("profiles", profiles);
        report.insert("routes", routes);
        report.insert("config_dir", configDir);
        QFile output("fixture-report.json");
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 37;
        output.write(QJsonDocument(report).toJson());
        output.close();
        QMessageBox view;
        view.setWindowTitle("MintVPN: synthetic profile test");
        view.setText(QString("Groups: %1; profiles: %2; routes: %3")
                         .arg(groups.size()).arg(profiles.size()).arg(routes.size()));
        QTimer::singleShot(1500, &view, &QMessageBox::accept);
        view.exec();
        return 0;
    }
    if (isolatedIntegration) {
        QJsonObject report;
        MW_show_log = [](const QString&) {};
        auto& manager = *Configs::dataManager;
        auto group = Configs::GroupsRepo::NewGroup();
        group->name = "integration only";
        if (!manager.groupsRepo->AddGroup(group)) return 21;
        auto endpoint = Configs::ProfilesRepo::NewProfile("socks");
        if (!endpoint || !endpoint->Socks()) return 22;
        endpoint->name = "local SOCKS";
        endpoint->Socks()->server = "127.0.0.1";
        endpoint->Socks()->server_port = 12345;
        if (!manager.profilesRepo->AddProfile(endpoint, group->id)) return 23;
        auto route = Configs::RoutesRepo::NewRouteProfile();
        route->name = "strict integration";
        route->isRaw = true;
        route->preventModifications = true;
        route->rawRoute = QString::fromUtf8(QJsonDocument(QJsonObject{
            {"_mintvpn_strict_amsterdam_outbound", endpoint->id},
            {"final", endpoint->id}, {"rules", QJsonArray{}}
        }).toJson(QJsonDocument::Compact));
        if (!manager.routesRepo->AddRouteProfile(route)) return 24;
        const int routeId = route->id;
        route.reset();
        route = manager.routesRepo->GetRouteProfile(routeId);
        if (!route || !route->isRaw || !route->preventModifications) return 25;
        manager.settingsRepo->current_route_id = routeId;
        auto built = Configs::BuildSingBoxConfig(endpoint);
        report["valid_error"] = built->error;
        report["config"] = built->coreConfig;
        auto sshProfile = Configs::ProfilesRepo::NewProfile("ssh");
        if (!sshProfile || !sshProfile->SSH()) return 29;
        sshProfile->name = "synthetic pinned SSH";
        sshProfile->outbound->name = sshProfile->name;
        sshProfile->SSH()->server = "100.64.0.1";
        sshProfile->SSH()->server_port = 22;
        sshProfile->SSH()->user = "test";
        sshProfile->SSH()->private_key_path = "E:/synthetic/id_ed25519";
        sshProfile->SSH()->host_key = {"ssh-ed25519 AAAA"};
        sshProfile->SSH()->host_key_algorithms = {"ssh-ed25519"};
        if (!manager.profilesRepo->AddProfile(sshProfile, group->id)) return 30;
        bool uiChecked = false;
        if (qEnvironmentVariableIsSet("MINTVPN_ISOLATED_UI_TEST")) {
            if (QApplication::platformName() != "windows") return 36;
            QFile uiTrace(QDir::current().absoluteFilePath("ui-trace.txt"));
            if (!uiTrace.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 42;
            auto trace = [&](const char* stage) { uiTrace.write(stage); uiTrace.write("\n"); uiTrace.flush(); };
            trace("construct-editor");
            RawRouteItem editor(nullptr, route);
            trace("editor-ready");
            QPushButton* templateButton = nullptr;
            for (auto* button : editor.findChildren<QPushButton*>())
                if (button->text() == "Amsterdam route template") templateButton = button;
            if (!templateButton) return 37;
            QTimer modalCloser;
            QObject::connect(&modalCloser, &QTimer::timeout, [&] {
                for (auto* window : QApplication::topLevelWidgets()) {
                    if (!window->isVisible()) continue;
                    if (auto* input = qobject_cast<QInputDialog*>(window)) {
                        trace("input-dialog");
                        auto* combo = input->findChild<QComboBox*>();
                        const auto choice = QString("%1 [%2]").arg(sshProfile->name).arg(sshProfile->id);
                        const int index = combo ? combo->findText(choice) : -1;
                        if (index < 0) { trace("choice-missing"); input->reject(); }
                        else { combo->setCurrentIndex(index); input->accept(); }
                    } else if (auto* question = qobject_cast<QMessageBox*>(window)) {
                        trace("confirmation-dialog");
                        if (auto* yes = question->button(QMessageBox::Yes)) yes->click();
                        else question->reject();
                    }
                }
            });
            modalCloser.start(10);
            QTimer::singleShot(5000, &editor, &QDialog::reject);
            QTimer::singleShot(0, &editor, [&] {
                trace("template-click"); templateButton->click(); trace("template-return");
                QMetaObject::invokeMethod(&editor, "accept");
            });
            const int dialogResult = editor.exec();
            trace("dialog-return");
            modalCloser.stop();
            if (dialogResult != QDialog::Accepted || !editor.chain->preventModifications) return 38;
            const auto uiRoute = QJsonDocument::fromJson(editor.chain->rawRoute.toUtf8()).object();
            if (uiRoute.value("_mintvpn_strict_amsterdam_outbound").toInt(-1) != sshProfile->id ||
                uiRoute.value("final").toInt(-1) != sshProfile->id) return 39;
            if (!manager.routesRepo->Save(editor.chain)) return 40;
            auto saved = manager.routesRepo->GetRouteProfile(routeId);
            if (!saved || saved->rawRoute != editor.chain->rawRoute || !saved->preventModifications) return 41;
            uiChecked = true;
        }
        report["offscreen_ui_template_saved"] = uiChecked;
        route->rawRoute = QString::fromUtf8(QJsonDocument(QJsonObject{
            {"_mintvpn_strict_amsterdam_outbound", sshProfile->id},
            {"final", sshProfile->id}, {"rules", QJsonArray{}}
        }).toJson(QJsonDocument::Compact));
        if (!manager.routesRepo->Save(route)) return 31;
        auto sshBuilt = Configs::BuildSingBoxConfig(sshProfile);
        report["ssh_error"] = sshBuilt->error;
        report["ssh_config"] = sshBuilt->coreConfig;
        QJsonObject controlledProxy;
        const auto controlledError = Configs::ControlledClaudeProxy::build(sshBuilt->coreConfig, 19081, controlledProxy);
        report["controlled_proxy_error"] = controlledError;
        report["controlled_proxy_config"] = controlledProxy;
        QJsonObject unchanged{{"sentinel", true}};
        auto badProxyConfig = sshBuilt->coreConfig;
        badProxyConfig["outbounds"] = QJsonArray{};
        const auto absentError = Configs::ControlledClaudeProxy::build(badProxyConfig, 19081, unchanged);
        const bool rejectsMissingSsh = !absentError.isEmpty() && unchanged == QJsonObject{{"sentinel", true}};
        report["controlled_proxy_rejects_missing_ssh"] = rejectsMissingSsh;
        auto variant = sshBuilt->coreConfig;
        auto variantOutbounds = variant.value("outbounds").toArray();
        const auto strictTag = variant.value("route").toObject().value("final").toString();
        for (int i = 0; i < variantOutbounds.size(); ++i) {
            auto outbound = variantOutbounds[i].toObject();
            if (outbound.value("tag").toString() == strictTag) {
                outbound["client_version"] = "SSH-2.0-MintVPN-test";
                variantOutbounds[i] = outbound;
            }
        }
        variant["outbounds"] = variantOutbounds;
        QJsonObject variantProxy;
        const auto variantError = Configs::ControlledClaudeProxy::build(variant, 19081, variantProxy);
        report["controlled_proxy_variant_error"] = variantError;
        report["controlled_proxy_variant_config"] = variantProxy;
        for (int i = 0; i < variantOutbounds.size(); ++i) {
            auto outbound = variantOutbounds[i].toObject();
            if (outbound.value("tag").toString() == strictTag) {
                outbound["connect_timeout"] = "5s";
                variantOutbounds[i] = outbound;
            }
        }
        variant["outbounds"] = variantOutbounds;
        QJsonObject unchangedVariant{{"sentinel", true}};
        const auto unsupportedError = Configs::ControlledClaudeProxy::build(variant, 19081, unchangedVariant);
        report["controlled_proxy_rejects_extra_option"] = !unsupportedError.isEmpty() && unchangedVariant == QJsonObject{{"sentinel", true}};
        QJsonObject selectiveRoute{
            {"_mintvpn_amsterdam_process_outbound", sshProfile->id},
            {"final", -1}, {"rules", QJsonArray{
                QJsonObject{{"process_name", QJsonArray{"claude.exe"}}, {"outbound", sshProfile->id}},
                QJsonObject{{"domain_suffix", QJsonArray{"example.test"}}, {"outbound", -2}}
            }}
        };
        route->rawRoute = QString::fromUtf8(QJsonDocument(selectiveRoute).toJson(QJsonDocument::Compact));
        if (!manager.routesRepo->Save(route)) return 33;
        auto splitBuilt = Configs::BuildSingBoxConfig(endpoint);
        report["selective_error"] = splitBuilt->error;
        report["selective_config"] = splitBuilt->coreConfig;
        selectiveRoute.remove("_mintvpn_amsterdam_process_outbound");
        route->rawRoute = QString::fromUtf8(QJsonDocument(selectiveRoute).toJson(QJsonDocument::Compact));
        if (!manager.routesRepo->Save(route)) return 34;
        auto baselineBuilt = Configs::BuildSingBoxConfig(endpoint);
        report["selective_baseline_error"] = baselineBuilt->error;
        const bool sameRoute = splitBuilt->coreConfig.value("route") == baselineBuilt->coreConfig.value("route");
        const bool sameDns = splitBuilt->coreConfig.value("dns") == baselineBuilt->coreConfig.value("dns");
        report["selective_route_preserved"] = sameRoute;
        report["selective_dns_preserved"] = sameDns;
        route->rawRoute = QString::fromUtf8(QJsonDocument(QJsonObject{
            {"_mintvpn_strict_amsterdam_outbound", sshProfile->id},
            {"final", sshProfile->id}, {"rules", QJsonArray{}}
        }).toJson(QJsonDocument::Compact));
        if (!manager.routesRepo->Save(route)) return 35;
        sshProfile->SSH()->host_key.clear();
        if (!manager.profilesRepo->Save(sshProfile)) return 32;
        auto sshMissingKey = Configs::BuildSingBoxConfig(sshProfile);
        report["ssh_missing_key_error"] = sshMissingKey->error;
        route->rawRoute = QString::fromUtf8(QJsonDocument(QJsonObject{
            {"_mintvpn_strict_amsterdam_outbound", endpoint->id},
            {"final", -2}, {"rules", QJsonArray{}}
        }).toJson(QJsonDocument::Compact));
        if (!manager.routesRepo->Save(route)) return 26;
        auto rejected = Configs::BuildSingBoxConfig(endpoint);
        report["direct_error"] = rejected->error;
        QFile result(QDir::current().absoluteFilePath("ams-integration.json"));
        if (!result.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 27;
        result.write(QJsonDocument(report).toJson());
        return !built->error.isEmpty() && !rejected->error.isEmpty() &&
            sshBuilt->error.isEmpty() && controlledError.isEmpty() && rejectsMissingSsh && !sshMissingKey->error.isEmpty() &&
            !splitBuilt->error.isEmpty() && baselineBuilt->error.isEmpty() ? 0 : 28;
    }

#endif
    Logging::SetLevel(Logging::LevelFromString(Configs::dataManager->settingsRepo->log_file_level));

    // Start traffic-statistics maintenance (startup downsample + background rollup).
    Stats::trafficStatsManager->Init();

    // Store Flags
    Configs::dataManager->settingsRepo->argv = arguments;
    if (Configs::dataManager->settingsRepo->argv.contains("-many")) Configs::dataManager->settingsRepo->flag_many = true;
    if (Configs::dataManager->settingsRepo->argv.contains("-tray")) Configs::dataManager->settingsRepo->flag_tray = true;
    if (Configs::dataManager->settingsRepo->argv.contains("-debug")) Configs::dataManager->settingsRepo->flag_debug = true;
    if (Configs::dataManager->settingsRepo->argv.contains("-flag_restart_tun_on")) Configs::dataManager->settingsRepo->flag_restart_tun_on = true;
    if (Configs::dataManager->settingsRepo->argv.contains("-flag_restart_dns_set")) Configs::dataManager->settingsRepo->flag_dns_set = true;
    Configs::dataManager->settingsRepo->flag_use_appdata = useAppdata;
    if(useAppdata && !appdataDir.isEmpty()) Configs::dataManager->settingsRepo->appdataDir = appdataDir;
#ifdef NKR_CPP_DEBUG
    Configs::dataManager->settingsRepo->flag_debug = true;
#endif

#ifdef Q_OS_LINUX
    QApplication::addLibraryPath(QApplication::applicationDirPath() + "/usr/plugins");
#endif

    // dispatchers
    DS_cores = new QThread;
    DS_cores->start();

    LogThread = new QThread;
    LogThread->start();

// icons
    QIcon::setFallbackSearchPaths(QStringList{
        ":/icon",
    });

    // icon for no theme
    if (QIcon::themeName().isEmpty()) {
        QIcon::setThemeName("breeze");
    }

#ifdef Q_OS_WIN
    if (!isolatedSmoke && Configs::dataManager->settingsRepo->windows_set_admin && !Configs::IsAdmin() && !Configs::dataManager->settingsRepo->disable_run_admin)
    {
        // Preserve the selected profile across elevation without replaying links.
        QStringList restartArguments;
        restartArguments << "-mintvpn-wait-profile";
        // Bootstrap is one-shot. The child reopens the same profile normally.
        const int appdataIndex = arguments.indexOf("-appdata");
        if (appdataIndex >= 0) {
            restartArguments << "-appdata";
            if (appdataIndex + 1 < arguments.size() && !arguments.at(appdataIndex + 1).startsWith('-'))
                restartArguments << arguments.at(appdataIndex + 1);
        }
        const bool childPreflight = WinCommander::startElevatedWithPreflight(
            QApplication::applicationFilePath(), restartArguments);
        if (childPreflight) {
            QApplication::quit();
            return 0;
        }
        // Keep the original session open if the elevated process was not created.
        qWarning() << "Administrator elevation was cancelled or failed; continuing without elevation";
    }
#endif

    // dataManager->settingsRepo & Flags
    if (Configs::dataManager->settingsRepo->start_minimal) Configs::dataManager->settingsRepo->flag_tray = true;

    // Translate
    QString locale;
    switch (Configs::dataManager->settingsRepo->language) {
        case 1: // English
            break;
        case 2:
            locale = "zh_CN";
            break;
        case 3:
            locale = "fa_IR"; // farsi(iran)
            break;
        case 4:
            locale = "ru_RU"; // Russian
            break;
        default:
            locale = QLocale().name();
    }
    QGuiApplication::tr("QT_LAYOUT_DIRECTION");
    loadTranslate(locale);

    // Must follow the single-instance check: opening the log earlier truncates
    // the running instance's file and leaves a marker it would report as a crash.
    Logging::Init(configDir);
    LOG_INFO(QString("appdata mode: %1").arg(useAppdata ? "yes" : "no"));
#ifdef Q_OS_WIN
    if (!isolatedSmoke && !portableFirstRun) { Windows_SetCrashDumpPath(); Windows_ConfigureWER(); }
#endif

    // QLocalServer
    QLocalServer server(qApp);
    server.setSocketOptions(QLocalServer::WorldAccessOption);
    if (!isolatedSmoke && !server.listen(serverName)) {
        qWarning() << "Failed to start QLocalServer! Error:" << server.errorString();
        Logging::Shutdown();
        return 1;
    }
    QObject::connect(&server, &QLocalServer::newConnection, qApp, [&] {
        auto s = server.nextPendingConnection();
        qDebug() << "Another instance tried to wake us up on " << serverName << s;
        // The waking instance may forward deeplinks and opened files as payload, one
        // url per line. Only whole lines are handled as they arrive; the tail, which
        // carries no trailing newline, is flushed once the peer is done.
        auto pending = std::make_shared<QByteArray>();
        auto handleLine = [](const QString &line) {
            if (line.startsWith("quattro://")) {
                Deeplink_Submit(line);
            } else if (line.startsWith("file://")) {
                LaunchFiles_Submit({QUrl(line).toLocalFile()});
            }
        };
        auto readPayload = [s, pending, handleLine](bool last) {
            pending->append(s->readAll());
            while (true) {
                const auto at = pending->indexOf('\n');
                if (at < 0) break;
                handleLine(QString::fromUtf8(pending->first(at)).trimmed());
                pending->remove(0, at + 1);
            }
            if (last) {
                handleLine(QString::fromUtf8(*pending).trimmed());
                pending->clear();
            }
        };
        QObject::connect(s, &QLocalSocket::readyRead, s, [readPayload] { readPayload(false); });
        QObject::connect(s, &QLocalSocket::disconnected, s, [readPayload] { readPayload(true); });
        QObject::connect(s, &QLocalSocket::disconnected, s, &QLocalSocket::deleteLater);
        readPayload(false); // in case the payload already arrived
        // raise main window
        MW_dialog_message(MwMessage::Raise, {});
    });
    QObject::connect(qApp, &QApplication::aboutToQuit, [&]
    {
        server.close();
        if (!isolatedSmoke) QLocalServer::removeServer(serverName);
        // Every quit path lands here; missing it is reported as a crash next start.
        Logging::Shutdown();
    });

#ifdef Q_OS_LINUX
    install_termination_handlers();
#endif

#ifdef Q_OS_WIN
    auto eventFilter = new PowerOffTaskkillFilter(signal_handler);
    a.installNativeEventFilter(eventFilter);
#endif

#ifdef Q_OS_MACOS
    QObject::connect(qApp, &QGuiApplication::commitDataRequest, [&](QSessionManager &manager)
    {
        Q_UNUSED(manager);
        signal_handler(0);
    });
#endif

    API::defaultClient = new API::Client();

    UI_InitMainWindow();

#ifdef MINTVPN_TEST_HARNESS
    if (arguments.contains("-isolated-smoke")) {
        QTimer::singleShot(2000, qApp, [] {
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (qobject_cast<MainWindow *>(widget) && widget->isVisible()) {
                    qApp->exit(widget->grab().save("gui-only.png") ? 0 : 38);
                    return;
                }
            }
            qApp->exit(39);
        });
    }
#endif
    Configs::dataManager->RunDeferredMaintenance();

    if (Logging::PreviousSessionCrashed()) {
        MW_show_log(QObject::tr("[Warn] Quattro did not shut down cleanly last time. "
                                "Diagnostics were saved to: %1").replace("Quattro", "MintVPN").arg(Logging::LogDir()));
    }

    // Deliver a deeplink and any files passed on the command line (cold start), then
    // replay whatever arrived during startup (e.g. a macOS FileOpen event before the
    // window existed).
    if (!launchDeeplink.isEmpty()) Deeplink_Submit(launchDeeplink);
    Deeplink_FlushPending();
    LaunchFiles_Submit(launchFiles);
    LaunchFiles_FlushPending();

    return QApplication::exec();
}
