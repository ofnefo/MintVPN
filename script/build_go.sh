#!/bin/bash
set -e

TAGS="with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,with_openvpn,with_openconnect,badlinkname,tfogo_checklinkname0"

: "${DEST:?DEST must be set to a build output directory}"
mkdir -p -- "$DEST"

[[ "$GOOS" =~ legacy$ ]] && IS_LEGACY=true && GOCMD="$PWD/golang.org/go/bin/go" && GOOS="${GOOS%legacy}" || { IS_LEGACY=false; GOCMD="go"; }

case "$GOOS" in
  windows)
    export CGO_ENABLED=0
    if ! $IS_LEGACY; then
      TAGS+=",with_purego,with_naive_outbound"
      # Require a separately acquired, provenance-reviewed, pinned DLL.
      "${PYTHON_EXECUTABLE:-python3}" script/stage_cronet.py "$DEST"
    fi
    ;;
  darwin)
    TAGS+=",with_naive_outbound"
    export CGO_ENABLED=1 CGO_LDFLAGS="-weak_framework UniformTypeIdentifiers"
    ;;
  linux)
    TAGS+=",with_naive_outbound"
    export CGO_ENABLED=1
    ;;
esac

#### Go: core ####
pushd core/server

if [[ "$GOOS" == "windows" || "$GOOS" == "linux" ]]; then
    UPDATER_EXT=$([[ "$GOOS" == "windows" ]] && echo ".exe" || true)
    UPDATER_NAME=$([[ "$GOOS" == "windows" ]] && echo "MintVPNUpdater" || echo "QuattroUpdater")
    $GOCMD build -v -o "$DEST/$UPDATER_NAME$UPDATER_EXT" -trimpath -ldflags "-w -s" ./cmd/quattro-updater
fi

pushd gen
protoc -I . --go_out=. --go-grpc_out=. libcore.proto
popd
VERSION_SINGBOX=$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)
CORE_EXT=$([[ "$GOOS" == "windows" ]] && echo ".exe" || true)
CORE_NAME=$([[ "$GOOS" == "windows" ]] && echo "MintVPNCore" || echo "QuattroCore")
$GOCMD build -v -o "$DEST/$CORE_NAME$CORE_EXT" -trimpath -ldflags "-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -X 'internal/godebug.defaultGODEBUG=multipathtcp=0' -checklinkname=0" -tags "$TAGS" .
popd
