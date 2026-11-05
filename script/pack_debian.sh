#!/bin/bash
set -e

VERSION="$1"
ARCH="$2"

mkdir -p Quattro/DEBIAN
mkdir -p Quattro/opt
cp -r linux-$ARCH$([[ $3 == "systemqt" ]] && echo "-system-qt") Quattro/opt
mv Quattro/opt/linux-$ARCH$([[ $3 == "systemqt" ]] && echo "-system-qt") Quattro/opt/Quattro
rm Quattro/opt/Quattro/Quattro.debug

# basic
cat >Quattro/DEBIAN/control <<-EOF
Package: Quattro
Version: $VERSION
Architecture: $ARCH
Maintainer: Quattro Project <222894393+ofnefo@users.noreply.github.com>
Depends: desktop-file-utils$([[ $3 == "systemqt" ]] && echo ", libqt6core6, libqt6gui6, libqt6network6, libqt6widgets6, qt6-qpa-plugins, qt6-wayland, qt6-gtk-platformtheme, qt6-xdgdesktopportal-platformtheme, libxcb-cursor0, fonts-noto-color-emoji")
Description: Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
EOF

cat >Quattro/DEBIAN/postinst <<-EOF
cat >/usr/share/applications/Quattro.desktop<<-END
[Desktop Entry]
Name=Quattro
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Exec=sh -c "PATH=/opt/Quattro:\$PATH /opt/Quattro/Quattro -appdata"
Icon=/opt/Quattro/Quattro.png
Terminal=false
Type=Application
Categories=Network;Application;
END

update-desktop-database
EOF

sudo chmod 0755 Quattro/DEBIAN/postinst

# desktop && PATH

sudo dpkg-deb --build Quattro
