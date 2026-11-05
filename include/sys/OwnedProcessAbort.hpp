#pragma once

#include <QProcess>
#include <QVariant>

namespace Configs_sys {
    constexpr auto explicitAbortProperty = "mintExplicitCoreAbort";
    constexpr auto abortGenerationProperty = "mintCoreAbortGeneration";

    inline bool abortOwnedProcess(QProcess *process) {
        if (process == nullptr) return false;
        process->setProperty(explicitAbortProperty, true);
        process->setProperty(abortGenerationProperty, process->property(abortGenerationProperty).toULongLong() + 1);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(5000);
        }
        return process->state() == QProcess::NotRunning;
    }
}
