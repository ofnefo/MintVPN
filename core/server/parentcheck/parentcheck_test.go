//go:build !debug

package parentcheck

import "testing"

func TestAllowedWindowsParent(t *testing.T) {
	for _, tc := range []struct {
	parentDir, selfDir, name, core string
		want                     bool
	}{
		{`C:\app`, `c:\APP`, "MintVPN.exe", "MintVPNCore.exe", true},
		{`C:\app`, `C:\app`, "Quattro.exe", "QuattroCore.exe", true},
		{`C:\other`, `C:\app`, "MintVPN.exe", "MintVPNCore.exe", false},
		{`C:\app`, `C:\app`, "other.exe", "MintVPNCore.exe", false},
	} {
		if got := allowedWindowsPair(tc.parentDir, tc.selfDir, tc.name, tc.core); got != tc.want {
			t.Fatalf("allowedWindowsPair(%q, %q, %q, %q) = %t, want %t", tc.parentDir, tc.selfDir, tc.name, tc.core, got, tc.want)
		}
	}
}
