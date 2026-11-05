#include "include/ui/setting/Icon.hpp"

#include "include/global/Configs.hpp"

#include <QPainter>
#include <QPen>


QPixmap Icon::GetTrayIcon(TrayIconStatus status) {
    QPixmap quattro(QStringLiteral(":/Quattro/Quattro-Tray.png"));
    if (!quattro.isNull()) {
        quattro = quattro.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPainter painter(&quattro);
        painter.setRenderHint(QPainter::Antialiasing);
        QColor indicator = QColor(QStringLiteral("#8b9098"));
        if (status == RUNNING) indicator = QColor(QStringLiteral("#18a957"));
        else if (status == VPN) indicator = QColor(QStringLiteral("#ef1717"));
        else if (status == SYSTEM_PROXY || status == SYSTEM_PROXY_DNS)
            indicator = QColor(QStringLiteral("#3976e8"));
        else if (status == DNS) indicator = QColor(QStringLiteral("#8b5cf6"));
        painter.setPen(QPen(Qt::white, 3));
        painter.setBrush(indicator);
        painter.drawEllipse(QRectF(43, 43, 18, 18));
        painter.end();
        return quattro;
    }

    QPixmap pixmap;
    QPixmap pixmap_read;

    if (status == NONE)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Off" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Quattro/") + "Off" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == RUNNING)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Quattro" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Quattro/") + "Quattro" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == SYSTEM_PROXY_DNS)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Proxy-Dns" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Quattro/") + "Proxy-Dns" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == SYSTEM_PROXY)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Proxy" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Quattro/") + "Proxy" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == DNS)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Dns" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Quattro/") + "Dns" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == VPN)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Tun" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Quattro/") + "Tun" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else
    {
        MW_show_log("Icon::GetTrayIcon: Unknown status");
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Off" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Quattro/") + "Off" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    }

    return pixmap;
}
