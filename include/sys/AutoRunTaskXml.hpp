#pragma once

#include <QString>
#include <QXmlStreamReader>

// Windows CommandLineToArgvW quoting: backslashes before a closing quote must be doubled.
inline QString autoRunQuoteArgument(const QString &value) {
    QString result = QStringLiteral("\"");
    int slashes = 0;
    for (const QChar ch : value) {
        if (ch == QLatin1Char('\\')) { ++slashes; continue; }
        if (ch == QLatin1Char('"')) result += QString(slashes * 2 + 1, QLatin1Char('\\'));
        else result += QString(slashes, QLatin1Char('\\'));
        slashes = 0;
        result += ch;
    }
    result += QString(slashes * 2, QLatin1Char('\\'));
    return result + QLatin1Char('"');
}

inline QString autoRunArguments(bool useAppdata, const QString &profile) {
    if (!useAppdata) return QStringLiteral("-tray");
    return QStringLiteral("-tray -appdata ") + autoRunQuoteArgument(profile);
}

inline QString autoRunTaskXml(const QString &userId, const QString &runLevel, const QString &exePath, const QString &arguments) {
    return QString(
        "<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
        "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
        "  <RegistrationInfo>\n"
        "    <Author>%1</Author>\n"
        "  </RegistrationInfo>\n"
        "  <Triggers>\n"
        "    <LogonTrigger>\n"
        "      <Enabled>true</Enabled>\n"
        "      <UserId>%1</UserId>\n"
        "    </LogonTrigger>\n"
        "  </Triggers>\n"
        "  <Principals>\n"
        "    <Principal id=\"Author\">\n"
        "      <UserId>%1</UserId>\n"
        "      <LogonType>InteractiveToken</LogonType>\n"
        "      <RunLevel>%2</RunLevel>\n"
        "    </Principal>\n"
        "  </Principals>\n"
        "  <Settings>\n"
        "    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\n"
        "    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\n"
        "    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\n"
        "    <AllowHardTerminate>false</AllowHardTerminate>\n"
        "    <StartWhenAvailable>false</StartWhenAvailable>\n"
        "    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>\n"
        "    <IdleSettings>\n"
        "      <StopOnIdleEnd>true</StopOnIdleEnd>\n"
        "      <RestartOnIdle>false</RestartOnIdle>\n"
        "    </IdleSettings>\n"
        "    <AllowStartOnDemand>true</AllowStartOnDemand>\n"
        "    <Enabled>true</Enabled>\n"
        "    <Hidden>false</Hidden>\n"
        "    <RunOnlyIfIdle>false</RunOnlyIfIdle>\n"
        "    <WakeToRun>false</WakeToRun>\n"
        "    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\n"
        // Task Scheduler's default of 7 is BELOW_NORMAL_PRIORITY_CLASS, which the core process inherits from us; 4-6 are the interactive band.
        "    <Priority>5</Priority>\n"
        "  </Settings>\n"
        "  <Actions Context=\"Author\">\n"
        "    <Exec>\n"
        "      <Command>%3</Command>\n"
        "      <Arguments>%4</Arguments>\n"
        "    </Exec>\n"
        "  </Actions>\n"
        "</Task>"
    ).arg(userId.toHtmlEscaped(), runLevel.toHtmlEscaped(), exePath.toHtmlEscaped(), arguments.toHtmlEscaped());
}

inline QString autoRunXmlElement(const QString &xml, const QString &element) {
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == element) return reader.readElementText();
    }
    return QString();
}
