// IPC fixture only: does not import the VPN core or alter system networking.
package main

import (
	"QuattroCore/ipc"
	"QuattroCore/parentcheck"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"github.com/tailscale/go-winio"
)

func main() {
	self, err := os.Executable()
	if err != nil { panic(err) }
	if strings.EqualFold(filepath.Base(self), "MintVPNCore.exe") {
		parentcheck.CheckParentProcess()
		conn, err := ipc.ConnectIPC(os.Getenv("QUATTRO_CORE_SOCKET"), parentcheck.ParentPID)
		if err != nil { panic(err) }
		defer conn.Close()
		if _, err = conn.Write([]byte("fixture-ipc-ok")); err != nil { panic(err) }
		return
	}
	if !strings.EqualFold(filepath.Base(self), "MintVPN.exe") { panic("unexpected fixture name") }
	pipe := fmt.Sprintf(`\\.\pipe\mintvpn-fixture-%d`, os.Getpid())
	listener, err := winio.ListenPipe(pipe, nil)
	if err != nil { panic(err) }
	defer listener.Close()
	child := exec.Command(filepath.Join(filepath.Dir(self), "MintVPNCore.exe"))
	child.SysProcAttr = &syscall.SysProcAttr{HideWindow: true, CreationFlags: 0x08000000}
	child.Env = append(os.Environ(), "QUATTRO_CORE_SOCKET="+pipe)
	child.Stdout, child.Stderr = os.Stdout, os.Stderr
	if err = child.Start(); err != nil { panic(err) }
	defer func(){ if child.ProcessState == nil { _ = child.Process.Kill(); _ = child.Wait() } }()
	type result struct { value string; err error }
	done := make(chan result, 1)
	go func() { c, e := listener.Accept(); if e != nil { done <- result{"", e}; return }; defer c.Close(); b, e := io.ReadAll(io.LimitReader(c, 64)); done <- result{string(b), e} }()
	select {
	case r := <-done:
		if r.err != nil || r.value != "fixture-ipc-ok" { panic("fixture handshake failed") }
	case <-time.After(10*time.Second): panic("fixture handshake timeout")
	}
	if err = child.Wait(); err != nil { panic(err) }
	fmt.Println("fixture IPC and parent-pair PASS; no VPN core was run")
}
