package main

import (
	"archive/zip"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"time"
)

const (
	archiveName = "Quattro.zip"
	extractName = "Quattro_update"
	payloadName = "Quattro"
)

func main() {
	if err := run(); err != nil {
		message := fmt.Sprintf("MintVPN update failed: %v\n", err)
		log.Print(message)
		_ = os.WriteFile("MintVPNUpdateError.log", []byte(message), 0o600)
		os.Exit(1)
	}
}

func run() error {
	workingDir, err := os.Getwd()
	if err != nil {
		return err
	}
	args := os.Args[1:]
	// A verified parent handshake is mandatory: an old GUI must never cause an
	// update to race a running client or tear down a connection prematurely.
	if len(args) < 6 || args[0] != "--mint-ready" || args[2] != "--mint-ack" || args[4] != "--mint-parent" {
		return errors.New("missing updater handshake")
	}
	ready, ack := args[1], args[3]
	pid, err := strconv.Atoi(args[5])
	if err != nil || pid <= 0 {
		return errors.New("invalid parent pid")
	}
	userArgs := args[6:]
	parent, err := openParent(pid)
	if err != nil {
		return fmt.Errorf("open parent: %w", err)
	}
	defer parent.close()
	archivePath := filepath.Join(workingDir, archiveName)
	extractDir, err := os.MkdirTemp(workingDir, ".mintvpn-extract-")
	if err != nil {
		return fmt.Errorf("create extraction directory: %w", err)
	}
	defer os.RemoveAll(extractDir)
	if err = unzipSafe(archivePath, extractDir); err != nil {
		return err
	}
	payloadDir := filepath.Join(extractDir, payloadName)
	info, err := os.Stat(payloadDir)
	if err != nil || !info.IsDir() {
		return fmt.Errorf("release must contain a top-level %s directory", payloadName)
	}
	if info, err = os.Stat(filepath.Join(payloadDir, guiExecutableName(runtime.GOOS))); err != nil || !info.Mode().IsRegular() {
		return errors.New("release GUI executable is unavailable")
	}
	tx, err := prepareTransaction(payloadDir, workingDir)
	if err != nil {
		return err
	}
	defer func() {
		if tx.disposable {
			_ = os.RemoveAll(tx.backupDir)
		}
	}()
	if err = os.WriteFile(ready, []byte("ready"), 0600); err != nil {
		return fmt.Errorf("signal ready: %w", err)
	}
	deadline := time.Now().Add(45 * time.Second)
	for {
		if _, err = os.Stat(ack); err == nil {
			break
		}
		if time.Now().After(deadline) {
			return errors.New("GUI did not confirm update")
		}
		time.Sleep(50 * time.Millisecond)
	}
	if err = parent.wait(90 * time.Second); err != nil {
		return fmt.Errorf("GUI did not exit: %w", err)
	}
	err = tx.install()
	if err == nil {
		err = restartQuattro(workingDir, userArgs)
	}
	if err != nil {
		rollbackErr := tx.rollback()
		if rollbackErr != nil {
			tx.disposable = false
			return fmt.Errorf("update failed: %v; rollback failed: %v; backup retained at %s", err, rollbackErr, tx.backupDir)
		}
		if restartErr := restartQuattro(workingDir, userArgs); restartErr != nil {
			return fmt.Errorf("update failed: %v; old GUI restart failed: %w", err, restartErr)
		}
		return fmt.Errorf("update failed, old files restored and GUI started: %w", err)
	}
	_ = os.Remove(archivePath)
	_ = os.Remove("MintVPNUpdateError.log")
	return nil
}

type updateFile struct {
	src, dst, backup string
	mode             fs.FileMode
	existed          bool
}
type transaction struct {
	files      []updateFile
	touched    int
	backupDir  string
	disposable bool
}

func prepareTransaction(sourceRoot, destinationRoot string) (*transaction, error) {
	backupDir, err := os.MkdirTemp(destinationRoot, ".mintvpn-backup-")
	if err != nil {
		return nil, err
	}
	tx := &transaction{backupDir: backupDir, disposable: true}
	err = filepath.WalkDir(sourceRoot, func(path string, entry fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		rel, err := filepath.Rel(sourceRoot, path)
		if err != nil || rel == "." {
			return err
		}
		if strings.EqualFold(strings.Split(rel, string(os.PathSeparator))[0], "config") {
			if entry.IsDir() {
				return filepath.SkipDir
			}
			return nil
		}
		if entry.IsDir() {
			return nil
		}
		info, err := entry.Info()
		if err != nil {
			return err
		}
		if !info.Mode().IsRegular() {
			return fmt.Errorf("non-regular payload: %s", rel)
		}
		f := updateFile{src: path, dst: filepath.Join(destinationRoot, rel), backup: filepath.Join(backupDir, rel), mode: info.Mode()}
		old, err := os.Lstat(f.dst)
		if err == nil {
			if !old.Mode().IsRegular() {
				return fmt.Errorf("non-regular destination: %s", rel)
			}
			f.existed = true
			if err = replaceFile(f.dst, f.backup, old.Mode()); err != nil {
				return err
			}
		} else if !errors.Is(err, os.ErrNotExist) {
			return err
		}
		tx.files = append(tx.files, f)
		return nil
	})
	if err != nil {
		_ = os.RemoveAll(backupDir)
		return nil, fmt.Errorf("prepare update: %w", err)
	}
	return tx, nil
}
func (tx *transaction) install() error {
	for i, f := range tx.files {
		tx.touched = i + 1
		if err := replaceFile(f.src, f.dst, f.mode); err != nil {
			return err
		}
	}
	return nil
}
func (tx *transaction) rollback() error {
	var failures []string
	for i := tx.touched - 1; i >= 0; i-- {
		f := tx.files[i]
		var err error
		if f.existed {
			var info fs.FileInfo
			info, err = os.Stat(f.backup)
			if err == nil {
				err = replaceFile(f.backup, f.dst, info.Mode())
			}
		} else {
			err = os.Remove(f.dst)
			if errors.Is(err, os.ErrNotExist) {
				err = nil
			}
		}
		if err != nil {
			failures = append(failures, fmt.Sprintf("%s: %v", f.dst, err))
		}
	}
	if len(failures) > 0 {
		return errors.New(strings.Join(failures, "; "))
	}
	return nil
}

func unzipSafe(archivePath, destination string) error {
	reader, err := zip.OpenReader(archivePath)
	if err != nil {
		return fmt.Errorf("open update archive: %w", err)
	}
	defer reader.Close()

	destination = filepath.Clean(destination)
	prefix := destination + string(os.PathSeparator)
	for _, entry := range reader.File {
		cleanName := filepath.Clean(filepath.FromSlash(entry.Name))
		if cleanName == "." || filepath.IsAbs(cleanName) || cleanName == ".." || strings.HasPrefix(cleanName, ".."+string(os.PathSeparator)) {
			return fmt.Errorf("unsafe archive path %q", entry.Name)
		}
		target := filepath.Join(destination, cleanName)
		if target != destination && !strings.HasPrefix(target, prefix) {
			return fmt.Errorf("archive path escapes destination: %q", entry.Name)
		}
		if entry.FileInfo().IsDir() {
			if err = os.MkdirAll(target, 0o755); err != nil {
				return err
			}
			continue
		}
		if err = os.MkdirAll(filepath.Dir(target), 0o755); err != nil {
			return err
		}
		source, openErr := entry.Open()
		if openErr != nil {
			return openErr
		}
		mode := entry.Mode()
		if mode == 0 {
			mode = 0o644
		}
		output, createErr := os.OpenFile(target, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, mode)
		if createErr != nil {
			source.Close()
			return createErr
		}
		_, copyErr := io.Copy(output, source)
		closeErr := output.Close()
		source.Close()
		if copyErr != nil {
			return copyErr
		}
		if closeErr != nil {
			return closeErr
		}
	}
	return nil
}

func installTree(sourceRoot, destinationRoot string) error {
	return filepath.WalkDir(sourceRoot, func(path string, entry fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		relative, err := filepath.Rel(sourceRoot, path)
		if err != nil || relative == "." {
			return err
		}
		parts := strings.Split(relative, string(os.PathSeparator))
		if strings.EqualFold(parts[0], "config") {
			if entry.IsDir() {
				return filepath.SkipDir
			}
			return nil
		}

		destination := filepath.Join(destinationRoot, relative)
		if entry.IsDir() {
			return os.MkdirAll(destination, 0o755)
		}
		info, err := entry.Info()
		if err != nil {
			return err
		}
		return replaceFile(path, destination, info.Mode())
	})
}

func replaceFile(source, destination string, mode fs.FileMode) error {
	input, err := os.Open(source)
	if err != nil {
		return err
	}
	defer input.Close()

	if err = os.MkdirAll(filepath.Dir(destination), 0o755); err != nil {
		return err
	}
	temporary := destination + ".quattro-new"
	_ = os.Remove(temporary)
	output, err := os.OpenFile(temporary, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, mode)
	if err != nil {
		return err
	}
	if _, err = io.Copy(output, input); err != nil {
		output.Close()
		return err
	}
	if err = output.Close(); err != nil {
		return err
	}
	if err = os.Chmod(temporary, mode); err != nil {
		return err
	}

	for attempt := 0; attempt < 20; attempt++ {
		_ = os.Remove(destination)
		if err = os.Rename(temporary, destination); err == nil {
			return nil
		}
		time.Sleep(250 * time.Millisecond)
	}
	return fmt.Errorf("replace %s: %w", filepath.Base(destination), err)
}

func restartQuattro(workingDir string, args []string) error {
	executable := filepath.Join(workingDir, guiExecutableName(runtime.GOOS))
	if _, err := os.Stat(executable); err != nil {
		return fmt.Errorf("updated executable is unavailable: %w", err)
	}
	command := exec.Command(executable, args...)
	command.Dir = workingDir
	if err := command.Start(); err != nil {
		return fmt.Errorf("restart MintVPN: %w", err)
	}
	if command.Process == nil {
		return errors.New("restart MintVPN: process was not created")
	}
	return command.Process.Release()
}

func guiExecutableName(goos string) string {
	if goos == "windows" {
		return "MintVPN.exe"
	}
	return "Quattro"
}
