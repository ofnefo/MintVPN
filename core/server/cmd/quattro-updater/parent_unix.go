//go:build !windows

package main

import (
	"errors"
	"syscall"
	"time"
)

type parentProcess struct{ pid int }

func openParent(pid int) (*parentProcess, error) {
	if err := syscall.Kill(pid, 0); err != nil {
		return nil, err
	}
	return &parentProcess{pid}, nil
}
func (p *parentProcess) close() {}
func (p *parentProcess) wait(timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		err := syscall.Kill(p.pid, 0)
		if errors.Is(err, syscall.ESRCH) {
			return nil
		}
		if err != nil {
			return err
		}
		time.Sleep(50 * time.Millisecond)
	}
	return errors.New("timeout")
}
