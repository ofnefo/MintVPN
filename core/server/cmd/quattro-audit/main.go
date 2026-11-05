// quattro-audit probes a subscription without installing routes or changing DNS.
// Read the sing-box subscription from stdin; output only names and measurements.
package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"
	"strings"
	"time"

	"QuattroCore/internal/boxmain"
	_ "QuattroCore/internal/distro/all"
	"QuattroCore/test_utils"
)

var probeURL = flag.String("url", "https://www.gstatic.com/generate_204", "HTTPS endpoint to probe")

func main() {
	flag.Parse()
	if err := audit(); err != nil {
		// Core errors can contain credentials, hostnames or entire configs.
		fmt.Fprintln(os.Stderr, "Audit could not initialize; check subscription format and supported protocols.")
		os.Exit(1)
	}
}

func audit() error {
	var subscription struct {
		Outbounds []map[string]any `json:"outbounds"`
	}
	if err := json.NewDecoder(io.LimitReader(os.Stdin, 16<<20)).Decode(&subscription); err != nil {
		return err
	}
	outbounds := []map[string]any{{"type": "direct", "tag": "audit-direct"}}
	var tags []string
	for _, outbound := range subscription.Outbounds {
		tag, _ := outbound["tag"].(string)
		if outbound["server"] == nil || tag == "" || strings.ContainsAny(tag, "⬇⚠") || strings.HasSuffix(strings.TrimSpace(tag), "---") {
			continue
		}
		outbounds = append(outbounds, outbound)
		tags = append(tags, tag)
	}
	if len(tags) == 0 {
		return fmt.Errorf("empty subscription")
	}
	config, err := json.Marshal(map[string]any{
		"log":       map[string]any{"disabled": true},
		"dns":       map[string]any{"servers": []any{map[string]any{"type": "local", "tag": "dns-local"}}},
		"route":     map[string]any{"final": "audit-direct", "default_domain_resolver": "dns-local"},
		"outbounds": outbounds,
	})
	if err != nil {
		return err
	}
	instance, cancel, err := boxmain.Create(config)
	if err != nil {
		return err
	}
	defer cancel()
	defer instance.Close()
	results := test_utils.BatchURLTest(context.Background(), instance, tags, *probeURL, 8, false, 8*time.Second)
	type measurement struct {
		Name         string `json:"name"`
		Reachable    bool   `json:"http_reachable"`
		Milliseconds int64  `json:"http_ms"`
	}
	report := make([]measurement, 0, len(results))
	for _, result := range results {
		report = append(report, measurement{result.Tag, result.Error == nil, result.Duration.Milliseconds()})
	}
	return json.NewEncoder(os.Stdout).Encode(report)
}
