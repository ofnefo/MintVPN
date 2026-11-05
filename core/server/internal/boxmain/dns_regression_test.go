package boxmain

import (
	"context"
	"fmt"
	"net"
	"strings"
	"testing"
	"time"

	_ "QuattroCore/internal/distro/all"
	"github.com/miekg/dns"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing/service"
)

// Start the actual core with no inbounds: no TUN, listeners or system DNS edits.
// The invalid detour used by the first local patch must reproduce the failure;
// the corrected transport must initialize without it.
func TestExternalTailscaleDNSStartup(t *testing.T) {
	for _, tc := range []struct {
		name, detour string
		wantError    bool
	}{
		{"reproduces_invalid_direct_detour", `,"detour":"direct"`, true},
		{"direct_transport_without_detour", "", false},
	} {
		t.Run(tc.name, func(t *testing.T) {
			config := fmt.Sprintf(`{
				"log":{"disabled":true},
				"dns":{"servers":[{"type":"udp","tag":"dns-external-tailscale","server":"100.100.100.100"%s}],
				"rules":[{"domain_suffix":["ts.net"],"action":"route","server":"dns-external-tailscale"}]},
				"outbounds":[{"type":"direct","tag":"direct"}]
			}`, tc.detour)
			instance, cancel, err := Create([]byte(config))
			if err == nil {
				defer cancel()
				defer instance.Close()
			}
			if tc.wantError {
				if err == nil || !strings.Contains(err.Error(), "detour to an empty direct outbound") {
					t.Fatalf("expected the reported DNS startup regression, got %v", err)
				}
				t.Log(err)
			} else if err != nil {
				t.Fatalf("corrected DNS transport failed to initialize: %v", err)
			}
		})
	}
}

func TestExternalTailscaleDNSExchangeBeforeFakeIP(t *testing.T) {
	packet, err := net.ListenPacket("udp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	server := &dns.Server{PacketConn: packet, Handler: dns.HandlerFunc(func(w dns.ResponseWriter, query *dns.Msg) {
		answer := new(dns.Msg)
		answer.SetReply(query)
		answer.Answer = []dns.RR{&dns.A{Hdr: dns.RR_Header{Name: query.Question[0].Name, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: 60}, A: net.IPv4(100, 64, 0, 12)}}
		_ = w.WriteMsg(answer)
	})}
	go func() { _ = server.ActivateAndServe() }()
	defer server.Shutdown()
	defer packet.Close()
	config := fmt.Sprintf(`{
		"log":{"disabled":true},
		"dns":{"servers":[
			{"type":"udp","tag":"dns-external-tailscale","server":"127.0.0.1","server_port":%d},
			{"type":"fakeip","tag":"fake","inet4_range":"198.18.0.0/15","inet6_range":"fc00::/18"}],
			"rules":[{"domain_suffix":["ts.net"],"action":"route","server":"dns-external-tailscale"},
			{"query_type":["A","AAAA"],"action":"route","server":"fake"}]},
		"outbounds":[{"type":"direct","tag":"direct"}]
	}`, packet.LocalAddr().(*net.UDPAddr).Port)
	instance, cancel, err := Create([]byte(config))
	if err != nil {
		t.Fatal(err)
	}
	defer cancel()
	defer instance.Close()
	ctx, stop := context.WithTimeout(context.Background(), 3*time.Second)
	defer stop()
	router := service.FromContext[adapter.DNSRouter](instance.Context())
	response, err := router.Exchange(ctx, new(dns.Msg).SetQuestion("host.test.ts.net.", dns.TypeA), adapter.DNSQueryOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if len(response.Answer) != 1 {
		t.Fatalf("unexpected DNS response: %v", response)
	}
	record, ok := response.Answer[0].(*dns.A)
	if !ok || !record.A.Equal(net.IPv4(100, 64, 0, 12)) {
		t.Fatalf("private DNS was replaced by FakeIP: %v", response)
	}
}
