package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestChipviewLinkWhenTheRunHasHTML(t *testing.T) {
	base := t.TempDir()
	pipe := filepath.Join(base, "t1", "pipeline")
	if err := os.MkdirAll(pipe, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(pipe, "events.log"), []byte("10:00:00 pipeline PASS\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(base, "t1", "chipview.html"), []byte("<!doctype html><title>chip</title>\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	lines = []line{{name: "ieda", base: base}}
	out := data("ieda", "t1")
	if !strings.Contains(out, "chipview\t/chipview?line=ieda&tag=t1\n") {
		t.Fatalf("missing chipview link\n%s", out)
	}
	if chipviewPath("ieda", "t1") == "" {
		t.Fatal("path")
	}
}

func TestIedaJournalFeedsTheBoard(t *testing.T) {
	base := t.TempDir()
	pb := filepath.Join(base, "ieda.playbook.json")
	if err := os.WriteFile(pb, []byte(`{"stack":{"stages":[{"name":"run"},{"name":"convert"}]},"proven_flow":{"dir":"/mnt/c/Users/grego/WASMApollo/pnr/flow"}}`), 0o644); err != nil {
		t.Fatal(err)
	}
	pipe := filepath.Join(base, "t1", "pipeline")
	if err := os.MkdirAll(pipe, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(pipe, "playbook"), []byte(pb+"\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	log := "" +
		"10:00:00 pipeline ieda-wgsl: 2 jobs, run " + filepath.Join(base, "t1") + "\n" +
		"10:00:01 step run: start\n" +
		"10:00:04 step run: PASS in 3s {\"seconds\":3,\"container\":\"dawn\"}\n" +
		"10:00:04 step convert: start\n" +
		"10:00:05 step convert: FAIL in 1s {\"seconds\":1,\"container\":\"apollo-eda\"} a kernel with no fabric image stops\n" +
		"10:00:05 pipeline FAIL: gate convert\n"
	if err := os.WriteFile(filepath.Join(pipe, "events.log"), []byte(log), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(pipe, "convert.verdict.json"), []byte(`{"pass":false,"findings":["a kernel with no fabric image stops"],"metrics":{"container":"apollo-eda","seconds":1}}`), 0o644); err != nil {
		t.Fatal(err)
	}
	lines = []line{{name: "ieda", base: base}}
	out := data("ieda", "t1")
	for _, want := range []string{
		"sel_line\tieda\n",
		"sel\tt1\tfail\t",
		"playbook\t" + pb,
		"proven\t/mnt/c/Users/grego/WASMApollo/pnr/flow",
		"stage\trun\tpass\t3\t10:00:01\t10:00:04\n",
		"stage\tconvert\tfail\t1\t10:00:04\t10:00:05\n",
		"pipeline\tpipeline ieda-wgsl: 2 jobs, run ",
		"end\tpipeline FAIL: gate convert",
		"vmetric\tconvert\tcontainer\tapollo-eda",
		"finding\tconvert\ta kernel with no fabric image stops",
	} {
		if !strings.Contains(out, want) {
			t.Fatalf("missing %q\n%s", want, out)
		}
	}
}
