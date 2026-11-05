//go:build !debug

package parentcheck

import (
	"log"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

func allowedWindowsPair(parentDir, selfDir, parentBase, selfBase string) bool {
	matchedPair := (strings.EqualFold(parentBase, "Quattro.exe") && strings.EqualFold(selfBase, "QuattroCore.exe")) ||
		(strings.EqualFold(parentBase, "MintVPN.exe") && strings.EqualFold(selfBase, "MintVPNCore.exe"))
	return strings.EqualFold(parentDir, selfDir) && matchedPair
}

func CheckParentProcess() {
	parentPath, err := getParentExePath(ParentPID)
	if err != nil {
		log.Fatalf("parent check: cannot read parent executable: %v", err)
	}
	parentPath = resolveFinalPath(parentPath)

	selfPath, err := os.Executable()
	if err != nil {
		log.Fatalf("parent check: cannot read own executable: %v", err)
	}
	selfPath = resolveFinalPath(selfPath)

	selfDir := filepath.Dir(selfPath)
	parentDir := filepath.Dir(parentPath)
	parentBase := filepath.Base(parentPath)

	if runtime.GOOS == "windows" {
		selfBase := filepath.Base(selfPath)
		if !allowedWindowsPair(parentDir, selfDir, parentBase, selfBase) {
			log.Fatalf("parent check failed: unexpected parent %q, selfPath is %q", parentPath, selfPath)
		}
		return
	}

	if parentDir != selfDir || parentBase != "Quattro" {
		log.Fatalf("parent check failed: unexpected parent %q, selfPath is %q", parentPath, selfPath)
	}
}
