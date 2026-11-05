#include "include/ui/mainwindow.h"
#include "NkrVersion.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QSaveFile>
#include <QThread>
#include <QUuid>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QMessageBox>
#include <QRegularExpression>

#include "3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.hpp"
#include "include/api/RPC.h"
#include "include/configs/generate.h"
#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/global/Logger.hpp"
#include "include/sys/Process.hpp"
#include "include/ui/mainWindow/MainWindowInternal.h"

#include "include/ui/group/dialog_manage_groups.h"
#include "include/ui/setting/dialog_basic_settings.h"
#include "include/ui/setting/dialog_hotkey.h"
#include "include/ui/setting/dialog_manage_routes.h"
#include "include/ui/setting/dialog_otp_manager.h"
#include "include/ui/setting/dialog_preset_settings.h"
#include "include/ui/setting/dialog_vpn_settings.h"

#ifdef Q_OS_WIN
#include "3rdparty/WinCommander.hpp"
#include "include/sys/windows/WinVersion.h"
#endif
#ifdef Q_OS_LINUX
#include "include/sys/linux/LinuxCap.h"
#endif
#ifdef Q_OS_MACOS
#include "include/sys/macos/MacOS.h"
#endif

qint64 MainWindow::GetCorePid() {
    QMutexLocker lock(&coreProcessMutex);
    return core_process ? core_process->processId() : 0;
}

QString MainWindow::GetRunningConfigName() {
    auto ent = running;
    if (ent == nullptr || ent->outbound == nullptr) return {};
    return ent->outbound->DisplayTypeAndName();
}

void MainWindow::on_menu_basic_settings_triggered() {
    USE_DIALOG(DialogBasicSettings)
}

void MainWindow::on_menu_manage_groups_triggered() {
    USE_DIALOG(DialogManageGroups)
}

void MainWindow::on_menu_routing_settings_triggered() {
    if (dialog_is_using) return;
    dialog_is_using = true;
    auto dialog = new DialogManageRoutes(this);
    connect(dialog, &QDialog::finished, this, [=,this] {
        dialog->deleteLater();
        dialog_is_using = false;
    });
    dialog->show();
}

void MainWindow::on_menu_vpn_settings_triggered() {
    USE_DIALOG(DialogVPNSettings)
}

void MainWindow::on_menu_preset_settings_triggered() {
    USE_DIALOG(DialogPresetSettings)
}

void MainWindow::on_menu_otp_manager_triggered() {
    USE_DIALOG(DialogOtpManager)
}

void MainWindow::on_menu_hotkey_settings_triggered() {
    if (dialog_is_using) return;
    dialog_is_using = true;
    auto dialog = new DialogHotkey(this, getActionsForShortcut());
    connect(dialog, &QDialog::finished, this, [=,this]
    {
        dialog->deleteLater();
        dialog_is_using = false;
    });
    dialog->show();
}

void MainWindow::on_commitDataRequest() {
    qDebug() << "Start of data save";

    auto* settings = Configs::dataManager->settingsRepo.get();

    settings->mainWindowGeometry = this->saveGeometry().toBase64(QByteArray::Base64Encoding);
    if (!isMaximized()) {
        auto news = QString("%1x%2").arg(size().width()).arg(size().height());
        if (settings->mw_size != news) settings->mw_size = news;
    }
    settings->splitter_state = ui->splitter->saveState().toBase64();

    // Backstop for the eager writes in set_spmode_*/UpdateStartedId: this only runs on a
    // graceful exit, so it must never be the sole place the remembered state is recorded.
    if (settings->remember_enable && settings->started_id >= 0) settings->remember_id = settings->started_id;
    settings->remember_system_proxy = settings->spmode_system_proxy;
    settings->remember_tun = settings->spmode_vpn;

    settings->Save();
    qDebug() << "End of data save";
}

void MainWindow::prepare_exit()
{
    qDebug() << "prepare for exit...";
    mu_exit.lock();
    if (Configs::dataManager->settingsRepo->prepare_exit)
    {
        qDebug() << "prepare exit had already succeeded, ignoring...";
        mu_exit.unlock();
        return;
    }
    Configs::dataManager->settingsRepo->prepare_exit = true;
    LOG_INFO("prepare_exit started, tearing down proxy/tun/core");
    //
    if (Configs::dataManager->settingsRepo->spmode_system_proxy) set_system_proxy(false);
    if (Configs::dataManager->settingsRepo->system_dns_set) set_system_dns(false, false);
    RegisterHiddenMenuShortcuts(true);
    RegisterHotkey(true);
    //
    on_commitDataRequest();
    //
    Configs::dataManager->settingsRepo->noSave = true; // don't change Configs::dataManager->settingsRepo after this line
    profile_stop(false, true);

    runOnThread([=, this]()
    {
        core_process->Kill();
    }, DS_cores, true);
    HideWindow(this);
    tray->hide();

    mu_exit.unlock();
    qDebug() << "prepare exit done!";
}

void MainWindow::on_menu_exit_triggered() {
    // Keep the current connection alive until the updater has validated the archive.
    //
    if (exit_reason == ExitReason::RunUpdater) {
        QDir::setCurrent(QApplication::applicationDirPath());
        QStringList updaterArgs;
        const auto launchArgs = QApplication::arguments();
        const int appdataIndex = launchArgs.indexOf("-appdata");
        if (appdataIndex >= 0) {
            updaterArgs << "-appdata";
            if (appdataIndex + 1 < launchArgs.size() && !launchArgs.at(appdataIndex + 1).startsWith('-'))
                updaterArgs << launchArgs.at(appdataIndex + 1);
        }
        const QString handshake = QDir::temp().filePath("mintvpn-update-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        if (!QDir().mkpath(handshake)) {
            exit_reason = ExitReason::None;
            MessageBoxWarning(software_name, tr("Could not prepare the updater. The current connection remains active."));
            return;
        }
        const QString ready = QDir(handshake).filePath("ready");
        const QString ack = QDir(handshake).filePath("ack");
        updaterArgs.prepend(QString::number(QCoreApplication::applicationPid()));
        updaterArgs.prepend("--mint-parent");
        updaterArgs.prepend(ack);
        updaterArgs.prepend("--mint-ack");
        updaterArgs.prepend(ready);
        updaterArgs.prepend("--mint-ready");
#ifdef Q_OS_WIN
        const QString updater = QFile::exists("./MintVPNUpdater.exe")
                ? QStringLiteral("./MintVPNUpdater.exe") : QStringLiteral("./QuattroUpdater.exe");
        const QString updaterCopy = QStringLiteral("./MintVPNUpdater.old.exe");
        const bool copied = (!QFile::exists(updaterCopy) || QFile::remove(updaterCopy))
                && QFile::copy(updater, updaterCopy);
        const bool started = copied && QProcess::startDetached(updaterCopy, updaterArgs);
#else
        const bool started = QProcess::startDetached("./QuattroUpdater", updaterArgs);
#endif
        QElapsedTimer timer;
        timer.start();
        while (started && !QFile::exists(ready) && timer.elapsed() < 30000) QThread::msleep(50);
        if (!started || !QFile::exists(ready)) {
            exit_reason = ExitReason::None;
            MessageBoxWarning(software_name, tr("The updater could not validate the update. The current connection remains active."));
            return;
        }
        QSaveFile confirmation(ack);
        if (!confirmation.open(QIODevice::WriteOnly) || confirmation.write("ok") != 2 || !confirmation.commit()) {
            exit_reason = ExitReason::None;
            MessageBoxWarning(software_name, tr("Could not confirm the update. The current connection remains active."));
            return;
        }
        prepare_exit();
    } else if (exit_reason == ExitReason::Restart || exit_reason == ExitReason::RestartWithTun || exit_reason == ExitReason::RestartWithDns) {
        QDir::setCurrent(QApplication::applicationDirPath());

        auto arguments = Configs::dataManager->settingsRepo->argv;
        const bool wasPortableBootstrap = arguments.contains("-portable-first-run");
        arguments.removeAll("-portable-first-run");
        if (arguments.length() > 0) {
            arguments.removeFirst();
            arguments.removeAll("-tray");
            arguments.removeAll("-flag_restart_tun_on");
            arguments.removeAll("-flag_restart_dns_set");
            arguments.removeAll("-mintvpn-wait-profile");
        }
        arguments << "-mintvpn-wait-profile";
        auto program = QApplication::applicationFilePath();

        if (exit_reason == ExitReason::RestartWithTun || exit_reason == ExitReason::RestartWithDns) {
            // A portable preview can request elevation, but must not reconnect or
            // modify DNS as a side effect of that restart.
            if (!wasPortableBootstrap) {
                if (exit_reason == ExitReason::RestartWithTun) arguments << "-flag_restart_tun_on";
                if (exit_reason == ExitReason::RestartWithDns) arguments << "-flag_restart_dns_set";
            }
#ifdef Q_OS_WIN
            const bool started = WinCommander::startElevatedWithPreflight(program, arguments);
#else
            const bool started = QProcess::startDetached(program, arguments);
#endif
            if (!started) {
                exit_reason = ExitReason::None;
                MessageBoxWarning(software_name, tr("Could not restart. The current connection remains active."));
                return;
            }
        } else if (!QProcess::startDetached(program, arguments)) {
            exit_reason = ExitReason::None;
            MessageBoxWarning(software_name, tr("Could not restart. The current connection remains active."));
            return;
        }
        prepare_exit();
    } else {
        prepare_exit();
    }
    QCoreApplication::quit();
}

void MainWindow::toggle_system_proxy() {
    auto currentState = Configs::dataManager->settingsRepo->spmode_system_proxy;
    if (currentState) {
        set_spmode_system_proxy(false);
    } else {
        set_spmode_system_proxy(true);
    }
}

bool MainWindow::get_elevated_permissions(ExitReason reason) {
    if (Configs::dataManager->settingsRepo->disable_privilege_req)
    {
        MW_show_log(tr("User opted for no privilege req, some features may not work"));
        return true;
    }
    if (Configs::IsAdmin()) return true;
#ifdef Q_OS_LINUX
    if (!Linux_HavePkexec()) {
        MessageBoxWarning(software_name, "Please install \"pkexec\" first.");
        return false;
    }
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please give the core root privileges"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes) {
        runOnNewThread([=,this]
        {
            auto chownArgs = QString("root:root " + Configs::FindCoreRealPath());
            auto ret = Linux_Run_Command("chown", chownArgs);
            if (ret != 0) {
                MW_show_log(QString("Failed to run chown %1 code is %2").arg(chownArgs).arg(ret));
            }
            auto chmodArgs = QString("u+s " + Configs::FindCoreRealPath());
            ret = Linux_Run_Command("chmod", chmodArgs);
            if (ret == 0) {
                StopVPNProcess();
            } else {
                MW_show_log(QString("Failed to run chmod %1").arg(chmodArgs));
            }
        });
        return false;
    }
#endif
#ifdef Q_OS_WIN
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please run Quattro as admin").replace("Quattro", "MintVPN"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes) {
        this->exit_reason = reason;
        on_menu_exit_triggered();
    }
#endif

#ifdef Q_OS_MACOS
    if (Configs::isSetuidSet(Configs::FindCoreRealPath().toStdString()))
    {
        StopVPNProcess();
        return true;
    }
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please give the core root privileges"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes)
    {
        auto Command = QString("sudo chown root:wheel '%1' && sudo chmod u+s '%1'").arg(Configs::FindCoreRealPath());
        auto ret = Mac_Run_Command(Command);
        if (ret == 0) {
            MessageBoxInfo(tr("Requesting permission"), tr("Please Enter your password in the opened terminal, then try again"));
            return false;
        } else {
            MW_show_log(QString("Failed to run %1 with %2").arg(Command).arg(ret));
            return false;
        }
    }
#endif
    return false;
}

void MainWindow::set_system_proxy(bool enable) {
    if (enable) {
        auto socks_port = Configs::dataManager->settingsRepo->inbound_socks_port;
        SetSystemProxy(socks_port, socks_port, Configs::dataManager->settingsRepo->proxy_scheme);
    } else {
        ClearSystemProxy();
    }
}

void MainWindow::set_spmode_system_proxy(bool enable, bool save) {
    if (enable && Configs::dataManager->settingsRepo->disable_mixed_inbound) {
        runOnUiThread([=, this] {
           MessageBoxWarning("Invalid Operation", "Cannot set system proxy when mixed inbound is disabled.");
        });
        ui->checkBox_SystemProxy->setChecked(false);
        return;
    }
    Configs::dataManager->settingsRepo->spmode_system_proxy = enable;
    if (running) {
        set_system_proxy(enable);
        if (!enable && Configs::dataManager->settingsRepo->reset_proxy_on_disable_sp) {
            profile_start(running->id);
        }
    }

    if (save) {
        Configs::dataManager->settingsRepo->remember_system_proxy = enable;
        Configs::dataManager->settingsRepo->Save();
    }

    refresh_status();
}

void MainWindow::set_spmode_vpn(bool enable, bool save) {
    if (enable == Configs::dataManager->settingsRepo->spmode_vpn) return;

    if (enable) {
        bool requestPermission = !Configs::IsAdmin();
        if (requestPermission) {
            if (!get_elevated_permissions()) {
                refresh_status();
                return;
            }
        }
    }

    if (save) {
        // Written here, after the elevation check, so a failed enable is not remembered.
        Configs::dataManager->settingsRepo->remember_tun = enable;
        Configs::dataManager->settingsRepo->Save();
    }

    Configs::dataManager->settingsRepo->spmode_vpn = enable;
    refresh_status();

    if (Configs::dataManager->settingsRepo->started_id >= 0) profile_start(Configs::dataManager->settingsRepo->started_id);
}

bool MainWindow::StopVPNProcess() {
    runOnThread([=, this]
    {
        core_process->Kill();
    }, DS_cores, true);

    return true;
}

namespace {

bool parseReleaseVersion(const QString &value, bool assetName, QList<int> *result) {
    static const QRegularExpression assetPattern(
        QStringLiteral(R"(^(?:Quattro|MintVPN)-[vV]?(\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)(?:[.-]?(\d+))?)?-)")
    );
    static const QRegularExpression versionPattern(
        QStringLiteral(R"(^[vV]?(\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)(?:[.-]?(\d+))?)?$)")
    );
    const auto match = (assetName ? assetPattern : versionPattern).match(value);
    if (!match.hasMatch()) return false;

    int stage = 4; // Stable releases sort after alpha, beta and rc.
    if (match.captured(4) == "alpha") stage = 1;
    if (match.captured(4) == "beta") stage = 2;
    if (match.captured(4) == "rc") stage = 3;
    *result = {match.captured(1).toInt(), match.captured(2).toInt(),
               match.captured(3).toInt(), stage, match.captured(5).toInt()};
    return true;
}

bool isNewer(const QString &assetName) {
    QList<int> available;
    QList<int> current;
    if (!parseReleaseVersion(assetName, true, &available) ||
        !parseReleaseVersion(QStringLiteral(NKR_VERSION), false, &current)) {
        MW_show_log(QStringLiteral("Invalid MintVPN release version: %1 (current: %2)")
                        .arg(assetName, QStringLiteral(NKR_VERSION)));
        return false;
    }
    for (qsizetype i = 0; i < available.size(); ++i) {
        if (available[i] != current[i]) return available[i] > current[i];
    }
    return false;
}

constexpr auto dashboardDownloadURL = "https://github.com/SagerNet/sing-box-dashboard/archive/refs/heads/gh-pages.zip";

bool copyOut(const QString &from, const QString &to) {
    QFile::remove(to);
    if (!QFile::copy(from, to)) return false;
    // Resource files are read-only, and QFile::copy carries that onto the copy.
    return QFile::setPermissions(to, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                     QFileDevice::ReadGroup | QFileDevice::ReadOther);
}

bool unpackBundledDashboard(const QDir &dest) {
    if (!QFile::exists(":/dashboard/index.html")) return false;
    const QDir bundle(":/dashboard");
    QDirIterator it(bundle.path(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto source = it.next();
        const auto target = dest.filePath(bundle.relativeFilePath(source));
        if (!dest.mkpath(QFileInfo(target).path()) || !copyOut(source, target)) return false;
    }
    return true;
}

} // namespace

void MainWindow::SeedDashboard() {
    QDir dashDir(Configs::apiDashboardDir);
    if (!dashDir.exists() && !QDir().mkpath(Configs::apiDashboardDir)) return;
    if (!QFile::exists(dashDir.filePath("index.html"))) unpackBundledDashboard(dashDir);
    // Reinstalling replaces the whole directory, so this cannot be a one-time copy.
    auto src = QFile(":/Quattro/dashboard-bootstrap.html");
    if (!src.open(QIODevice::ReadOnly)) return;
    const auto data = src.readAll();
    src.close();
    if (auto dest = QFile(dashDir.filePath("quattro.html")); dest.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        dest.write(data);
        dest.close();
    }
}

void MainWindow::OpenDashboard() {
    const auto &settings = *Configs::dataManager->settingsRepo;
    const auto port = settings.core_box_api_port;
    if (port <= 0) {
        MessageBoxWarning(software_name, tr("The sing-box API is disabled. Set a listen port in Preferences > Basic Settings > Core."));
        return;
    }
    if (settings.started_id < 0) {
        MessageBoxWarning(software_name, tr("Start a profile first; the dashboard is served by the running core."));
        return;
    }

    const auto show = [this, port] {
        SeedDashboard();
        // Fragment, not query: browsers never send it to the server.
        QUrl url(QString("http://127.0.0.1:%1/dashboard/quattro.html").arg(port));
        url.setFragment(QString("secret=%1&url=127.0.0.1:%2")
                            .arg(QString::fromUtf8(QUrl::toPercentEncoding(Configs::dataManager->settingsRepo->core_box_api_secret)))
                            .arg(port),
                        QUrl::StrictMode);
        QDesktopServices::openUrl(url);
    };

    SeedDashboard();
    if (QFile::exists(QDir(Configs::apiDashboardDir).filePath("index.html"))) {
        show();
        return;
    }

    if (QMessageBox::question(this, tr("Web dashboard"),
                              tr("The dashboard is not installed yet. Download it now?"))
        != QMessageBox::StandardButton::Yes) {
        return;
    }

    runOnNewThread([=, this] {
        if (!mu_download_dashboard.tryLock()) {
            runOnUiThread([=, this] {
                MessageBoxWarning(tr("Cannot start"), tr("A dashboard download is already running"));
            });
            return;
        }
        const auto archive = QString("quattro-dashboard.zip");
        auto error = NetworkRequestHelper::DownloadAsset(dashboardDownloadURL, archive, true);
        if (error.isEmpty()) {
            bool ok = false;
            error = API::defaultClient->InstallDashboard(&ok, Configs::GetBasePath() + "/" + archive,
                                                         QDir(Configs::apiDashboardDir).absolutePath());
            if (!ok && error.isEmpty()) error = tr("The core did not answer.");
        }
        QFile::remove(Configs::GetBasePath() + "/" + archive);
        mu_download_dashboard.unlock();

        runOnUiThread([=, this] {
            if (!error.isEmpty()) {
                MessageBoxWarning(tr("Failed to install the dashboard"), error);
                return;
            }
            show();
        });
    });
}

void MainWindow::CheckUpdate(bool quiet) {
    QString search;
#ifdef Q_OS_WIN
#  ifdef Q_PROCESSOR_ARM_64
    search = "windows-arm64";
#  else
#    ifdef Q_OS_WIN64
        if (WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_10_1809))
            search = "windows-amd64";
        else
	        search = "windowslegacy-amd64";
#    else
	    search = "windowslegacy-386";
#    endif
#  endif
#endif
#ifdef Q_OS_LINUX
#  ifdef Q_PROCESSOR_X86_64
    search = "linux-amd64";
#  else
    search = "linux-arm64";
#  endif
#endif
#ifdef Q_OS_MACOS
#  ifdef Q_PROCESSOR_X86_64
	search = "macos-amd64";
#  else
	search = "macos-arm64";
#  endif
#endif
    if (search.isEmpty()) {
        if (quiet) return;
        runOnUiThread([=,this] {
            MessageBoxWarning(QObject::tr("Update"), QObject::tr("Not official support platform"));
        });
        return;
    }

    const QString feedUrl = QStringLiteral("https://api.github.com/repos/ofnefo/MintVPN/releases");
    const QString assetPrefix = QStringLiteral("MintVPN-");
    auto resp = NetworkRequestHelper::HttpGet(feedUrl);
    if (!resp.error.isEmpty()) {
        if (quiet) { MW_show_log(tr("Automatic app update check failed; retrying on the next scheduled check.")); return; }
        runOnUiThread([=,this] {
            MessageBoxWarning(QObject::tr("Update"), QObject::tr("Requesting update error: %1").arg(resp.error + "\n" + resp.data));
        });
        return;
    }

    QString assets_name, release_download_url, release_url, release_note, note_pre_release;
    bool exitFlag = false;
    QJsonParseError parseError;
    const QJsonDocument releases = QJsonDocument::fromJson(resp.data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !releases.isArray()) {
        if (quiet) { MW_show_log(QStringLiteral("Invalid MintVPN release feed response.")); return; }
        runOnUiThread([=,this] { MessageBoxWarning(tr("Обновления"), tr("Не удалось прочитать ответ источника релизов MintVPN.")); });
        return;
    }
    for (const QJsonValue value : releases.array()) {
        QJsonObject release = value.toObject();
        if (release["prerelease"].toBool() && !Configs::dataManager->settingsRepo->allow_beta_update) continue;
        for (const QJsonValue asset : release["assets"].toArray()) {
            if (asset["name"].toString().startsWith(assetPrefix) && asset["name"].toString().contains(search) && asset["name"].toString().section('.', -1) == QString("zip")) {
                note_pre_release = release["prerelease"].toBool() ? " (Pre-release)" : "";
                release_url = release["html_url"].toString();
                release_note = release["body"].toString();
                assets_name = asset["name"].toString();
                release_download_url = asset["browser_download_url"].toString();
                exitFlag = true;
                break;
            }
        }
        if (exitFlag) break;
    }

    if (release_download_url.isEmpty() || !isNewer(assets_name)) {
        if (quiet) return;
        runOnUiThread([=,this] {
            MessageBoxInfo(QObject::tr("Update"), tr("В источнике MintVPN нет подходящего нового релиза."));
        });
        return;
    }

    if (quiet) {
        runOnUiThread([this, assets_name] {
            tray->showMessage(tr("Обновление MintVPN"),
                tr("Доступна %1. Откройте проверку обновлений в расширенном режиме.").arg(assets_name));
        });
        return;
    }

    runOnUiThread([=,this] {
        QMessageBox box(QMessageBox::Information, QObject::tr("Update") + note_pre_release,
                        QObject::tr("Update found: %1\nRelease note:\n%2").arg(assets_name, release_note));
        box.setInformativeText(tr("Источник: ofnefo/MintVPN на GitHub. Установка обновлений MintVPN ещё не проверена; доступен просмотр релиза."));
        QAbstractButton *open = box.addButton(QObject::tr("Open in browser"), QMessageBox::AcceptRole);
        box.addButton(QObject::tr("Close"), QMessageBox::RejectRole);
        box.exec();
        if (open == box.clickedButton()) QDesktopServices::openUrl(QUrl(release_url));
    });
}
