package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestPartialInstallRestoresExistingAndRemovesNewFiles(t *testing.T) {
	root := t.TempDir()
	src := filepath.Join(root, "src")
	dst := filepath.Join(root, "dst")
	for _, d := range []string{src, dst, filepath.Join(dst, "config")} {
		if err := os.MkdirAll(d, 0755); err != nil {
			t.Fatal(err)
		}
	}
	for path, data := range map[string]string{filepath.Join(src, "a-new"): "new", filepath.Join(src, "b-old"): "updated", filepath.Join(src, "c-fail"): "bad", filepath.Join(dst, "b-old"): "old", filepath.Join(dst, "config", "profile"): "profile"} {
		if err := os.WriteFile(path, []byte(data), 0600); err != nil {
			t.Fatal(err)
		}
	}
	tx, err := prepareTransaction(src, dst)
	if err != nil {
		t.Fatal(err)
	}
	if err = os.Remove(filepath.Join(src, "c-fail")); err != nil {
		t.Fatal(err)
	}
	if err = tx.install(); err == nil {
		t.Fatal("expected partial install failure")
	}
	if err = tx.rollback(); err != nil {
		t.Fatal(err)
	}
	if _, err = os.Stat(filepath.Join(dst, "a-new")); !os.IsNotExist(err) {
		t.Fatalf("new file remains: %v", err)
	}
	for name, want := range map[string]string{"b-old": "old", filepath.Join("config", "profile"): "profile"} {
		got, err := os.ReadFile(filepath.Join(dst, name))
		if err != nil || string(got) != want {
			t.Fatalf("%s: %q %v", name, got, err)
		}
	}
}
