#include "include/sys/AutoRun.hpp"
#include "include/sys/AutoRunTaskXml.hpp"

#include <QCryptographicHash>
#include <QDir>
#include "include/global/Configs.hpp"
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>

#include "3rdparty/WinCommander.hpp"

QString GetTaskName() {
    QString exePath = QDir::cleanPath(QCoreApplication::applicationFilePath()).toLower();

    QByteArray hash = QCryptographicHash::hash(exePath.toUtf8(), QCryptographicHash::Md5).toHex();

    QString shortHash = QString(hash).left(8);

    return QString("Quattro AutoRun %1").arg(shortHash);
}

QString getCurrentUser() {
    QString domain = qEnvironmentVariable("USERDOMAIN");
    QString user = qEnvironmentVariable("USERNAME");
    return domain + "\\" + user;
}

QString autoRunCurrentArguments() {
    const bool useAppdata = Configs::dataManager->settingsRepo->flag_use_appdata;
    const QString profile = QDir::toNativeSeparators(QDir::cleanPath(Configs::GetBasePath()));
    return autoRunArguments(useAppdata, profile);
}

void enable_autorun() {
    QString taskName = GetTaskName();
    QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QString userId = getCurrentUser();

    QString runLevel = (Configs::IsAdmin() && !Configs::dataManager->settingsRepo->disable_run_admin) ? "HighestAvailable" : "LeastPrivilege";

    const QString xmlContent = autoRunTaskXml(userId, runLevel, exePath, autoRunCurrentArguments());

    QString xmlFilePath = QDir::toNativeSeparators(QDir::tempPath() + "\\Quattro_Task.xml");
    QFile xmlFile(xmlFilePath);
    if (xmlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&xmlFile);
        out.setEncoding(QStringConverter::Utf16);
        out.setGenerateByteOrderMark(true);
        out << xmlContent;
        xmlFile.close();
    }

    QStringList args;
    args << "/create" << "/tn" << taskName << "/xml" << "\"" + xmlFilePath + "\"" << "/f";

    QProcess process;
    process.start("schtasks.exe", args);
    process.waitForFinished();

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        WinCommander::runProcessElevated("schtasks.exe", args, "", 0, true);
    }
}

void disable_autorun() {
    QString taskName = GetTaskName();

    QStringList args;
    args << "/delete" << "/tn" << taskName << "/f";

    QProcess process;
    process.start("schtasks.exe", args);
    process.waitForFinished();

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        WinCommander::runProcessElevated("schtasks.exe", args, "", 0, true);
    }
}

void AutoRun_SetEnabled(bool enable) {
    if (enable) {
        enable_autorun();
    } else {
        disable_autorun();
    }
}

bool AutoRun_IsEnabled() {
    QString taskName = GetTaskName();

    QProcess process;
    process.start("schtasks.exe", QStringList() << "/query" << "/tn" << taskName << "/xml");
    process.waitForFinished();

    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
        if (!output.contains("xml")) return false;
        if (output.contains("<Enabled>false</Enabled>", Qt::CaseInsensitive)) return false;
        return true;
    }

    return false;
}

// Tasks registered by older versions carry priority 7 (BELOW_NORMAL_PRIORITY_CLASS); a missing element means the
// same, since that is Task Scheduler's default. A deliberate bump to a higher priority is left alone.
bool autoRun_priorityIsStale(const QString &xml) {
    static const QRegularExpression re("<Priority>\\s*(\\d+)\\s*</Priority>");
    auto match = re.match(xml);
    if (!match.hasMatch()) return true;
    return match.captured(1).toInt() > 6;
}

void AutoRun_FixTaskIfNeeded() {
    QString taskName = GetTaskName();
    QString runLevel = (Configs::IsAdmin() && !Configs::dataManager->settingsRepo->disable_run_admin) ? "HighestAvailable" : "LeastPrivilege";

    QProcess process;
    process.start("schtasks.exe", QStringList() << "/query" << "/tn" << taskName << "/xml");
    process.waitForFinished();

    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
        if (!output.contains("xml")) return;
        bool privilegeIsStale = (runLevel == "HighestAvailable" && !output.contains("HighestAvailable")) || (runLevel == "LeastPrivilege" && output.contains("HighestAvailable"));
        const bool startupModeIsStale = autoRunXmlElement(output, QStringLiteral("Arguments")) != autoRunCurrentArguments() ||
            !autoRunXmlElement(output, QStringLiteral("GroupId")).isNull();
        if (privilegeIsStale || autoRun_priorityIsStale(output) || startupModeIsStale) AutoRun_SetEnabled(true);
    }
}

QString windows_GenAutoRunString() {
    auto appPath = QApplication::applicationFilePath();
    appPath = "\"" + QDir::toNativeSeparators(appPath) + "\"";
    appPath += " -tray";
    return appPath;
}

bool old_autoRun_isEnabled() {
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

    auto appPath = QApplication::applicationFilePath();
    QFileInfo fInfo(appPath);
    QString name = fInfo.baseName();

    return settings.value(name).toString() == windows_GenAutoRunString();
}

void AutoRun_MigrateIfNeeded() {
    if (!old_autoRun_isEnabled()) return;

    auto appPath = QApplication::applicationFilePath();
    QFileInfo fInfo(appPath);
    QString name = fInfo.baseName();
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    settings.remove(name);

    AutoRun_SetEnabled(true);
}
