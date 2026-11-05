//go:build !debug

package parentcheck

import "testing"

func TestWindowsPairs(t *testing.T) {
	for _, tc := range []struct { dir, selfDir, parent, core string; want bool }{
		{"same", "same", "Quattro.exe", "QuattroCore.exe", true},
		{"same", "SAME", "mintvpn.EXE", "MintVPNCore.exe", true},
		{"same", "different", "MintVPN.exe", "MintVPNCore.exe", false},
		{"same", "same", "MintVPN.exe", "QuattroCore.exe", false},
		{"same", "same", "Other.exe", "MintVPNCore.exe", false},
	} {
		if got := allowedWindowsPair(tc.dir, tc.selfDir, tc.parent, tc.core); got != tc.want {
			t.Errorf("%+v: got %v", tc, got)
		}
	}
}
