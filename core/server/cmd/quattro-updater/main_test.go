package main

import (
	"archive/zip"
	"os"
	"path/filepath"
	"testing"
)

func writeTestArchive(t *testing.T, path string, files map[string]string) {
	t.Helper()
	output, err := os.Create(path)
	if err != nil {
		t.Fatal(err)
	}
	writer := zip.NewWriter(output)
	for name, content := range files {
		entry, createErr := writer.Create(name)
		if createErr != nil {
			t.Fatal(createErr)
		}
		if _, writeErr := entry.Write([]byte(content)); writeErr != nil {
			t.Fatal(writeErr)
		}
	}
	if err = writer.Close(); err != nil {
		t.Fatal(err)
	}
	if err = output.Close(); err != nil {
		t.Fatal(err)
	}
}

func TestUnzipSafeRejectsTraversal(t *testing.T) {
	root := t.TempDir()
	archive := filepath.Join(root, "bad.zip")
	writeTestArchive(t, archive, map[string]string{"../outside.txt": "no"})

	if err := unzipSafe(archive, filepath.Join(root, "extract")); err == nil {
		t.Fatal("expected path traversal to be rejected")
	}
	if _, err := os.Stat(filepath.Join(root, "outside.txt")); !os.IsNotExist(err) {
		t.Fatalf("archive escaped extraction directory: %v", err)
	}
}

func TestInstallTreePreservesConfig(t *testing.T) {
	root := t.TempDir()
	source := filepath.Join(root, "source")
	destination := filepath.Join(root, "destination")
	if err := os.MkdirAll(filepath.Join(source, "config"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(filepath.Join(destination, "config"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(source, "Quattro.exe"), []byte("new"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(source, "config", "quattro.db"), []byte("release"), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(destination, "config", "quattro.db"), []byte("user"), 0o600); err != nil {
		t.Fatal(err)
	}

	if err := installTree(source, destination); err != nil {
		t.Fatal(err)
	}
	installed, err := os.ReadFile(filepath.Join(destination, "Quattro.exe"))
	if err != nil || string(installed) != "new" {
		t.Fatalf("application file was not installed: %q, %v", installed, err)
	}
	config, err := os.ReadFile(filepath.Join(destination, "config", "quattro.db"))
	if err != nil || string(config) != "user" {
		t.Fatalf("user config was overwritten: %q, %v", config, err)
	}
}

func TestGUIExecutableName(t *testing.T) {
	if got := guiExecutableName("windows"); got != "MintVPN.exe" {
		t.Fatalf("windows GUI: %q", got)
	}
	if got := guiExecutableName("linux"); got != "Quattro" {
		t.Fatalf("linux GUI: %q", got)
	}
}
