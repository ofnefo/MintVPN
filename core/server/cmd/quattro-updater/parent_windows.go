//go:build windows

package main

import (
	"fmt"
	"syscall"
	"time"
)

type parentProcess struct{ handle syscall.Handle }

func openParent(pid int) (*parentProcess, error) {
	h, err := syscall.OpenProcess(0x00100000, false, uint32(pid)) // SYNCHRONIZE
	if err != nil {
		return nil, err
	}
	return &parentProcess{h}, nil
}
func (p *parentProcess) close() { _ = syscall.CloseHandle(p.handle) }
func (p *parentProcess) wait(timeout time.Duration) error {
	result, err := syscall.WaitForSingleObject(p.handle, uint32(timeout/time.Millisecond))
	if err != nil {
		return err
	}
	if result != 0 {
		return fmt.Errorf("wait result %d", result)
	}
	return nil
}
