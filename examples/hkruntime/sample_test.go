package main

import (
	"math"
	"testing"
)

func putF32(b []byte, i int, v float32) {
	u := math.Float32bits(v)
	b[4*i] = byte(u)
	b[4*i+1] = byte(u >> 8)
	b[4*i+2] = byte(u >> 16)
	b[4*i+3] = byte(u >> 24)
}

func TestPenalize(t *testing.T) {
	logits := make([]byte, 4*4)
	putF32(logits, 0, 10)
	putF32(logits, 1, -2)
	putF32(logits, 2, 8)
	putF32(logits, 3, 9) // the end token
	end := func(id int) bool { return id == 3 }
	// Token 0 said three times pays once. The end token is never penalized.
	penalize(logits, 4, []int{0, 0, 1, 3, 0}, 1.25, end)
	if got := logitAt(logits, 0); got != 8 {
		t.Fatalf("positive logit %v, want 10/1.25", got)
	}
	if got := logitAt(logits, 1); got != -2.5 {
		t.Fatalf("negative logit %v, want -2*1.25", got)
	}
	if got := logitAt(logits, 2); got != 8 {
		t.Fatalf("unsaid logit %v", got)
	}
	if got := logitAt(logits, 3); got != 9 {
		t.Fatalf("end token %v: a penalty must not hold an answer open", got)
	}
	// 1 is off.
	penalize(logits, 4, []int{2}, 1, nil)
	if got := logitAt(logits, 2); got != 8 {
		t.Fatalf("pen 1 changed %v", got)
	}
}

func TestRemember(t *testing.T) {
	var r []int
	for i := 0; i < 5; i++ {
		r = remember(r, i, 3)
	}
	if len(r) != 3 || r[0] != 2 || r[2] != 4 {
		t.Fatalf("window %v", r)
	}
}

func TestMinP(t *testing.T) {
	cs := []cand{{id: 7, p: 0.9}, {id: 8, p: 0.06}, {id: 9, p: 0.04}}
	r := rng{s: 12345}
	// min-p 0.05 keeps 7 and 8 (0.06 >= 0.045) and drops 9.
	for i := 0; i < 2000; i++ {
		if got := sample(cs, 0.6, 1, 0.05, 0, nil, &r); got.id == 9 {
			t.Fatalf("min-p kept the tail")
		}
	}
	// Greedy is the first entry.
	if got := sample(cs, 0, 0.9, 0.05, 0, nil, &r); got.id != 7 {
		t.Fatalf("greedy %d", got.id)
	}
	// top-p 0.5 is the first entry alone.
	for i := 0; i < 200; i++ {
		if got := sample(cs, 0.6, 0.5, 0, 0, nil, &r); got.id != 7 {
			t.Fatalf("top-p %d", got.id)
		}
	}
}

func TestTopKKeepsEnd(t *testing.T) {
	// Top-1 would be 7 alone. 9 is an end token with the same mass, so it stays
	// in the draw and is selected.
	cs := []cand{{id: 7, p: 0.5}, {id: 9, p: 0.5}}
	end := func(id int) bool { return id == 9 }
	sawEnd := false
	r := rng{s: 99}
	for i := 0; i < 200; i++ {
		if sample(cs, 0.6, 1, 0, 1, end, &r).id == 9 {
			sawEnd = true
			break
		}
	}
	if !sawEnd {
		t.Fatal("top-k dropped the end token")
	}
	// Without the end exception, top-1 is only 7.
	r.s = 99
	for i := 0; i < 50; i++ {
		if got := sample(cs, 0.6, 1, 0, 1, nil, &r); got.id != 7 {
			t.Fatalf("top-1 %d", got.id)
		}
	}
}

func TestPresenceSkipsEnd(t *testing.T) {
	logits := make([]byte, 4*3)
	putF32(logits, 0, 1)
	putF32(logits, 1, 5) // content, said many times
	putF32(logits, 2, 4) // the end token, also "said"
	end := func(id int) bool { return id == 2 }
	counts := []uint16{0, 8, 8}
	if got := pickPenalized(logits, 3, counts, 1, end); got != 2 {
		t.Fatalf("end token %d, want 2: a penalty must not hold the answer open", got)
	}
	if got := pickPenalized(logits, 3, counts, 1, nil); got != 0 {
		t.Fatalf("without the exception %d, want the unsaid token", got)
	}
}

func TestMixTemperature(t *testing.T) {
	w := [4]float64{0.81, 0.09, 0.09, 0.01}
	if got := mixTemperature(w, 0); got != mixTempAt(0) {
		t.Fatalf("lite %v", got)
	}
	if got := mixTemperature(w, 0.85); got != mixTempAt(1) {
		t.Fatalf("balanced %v", got)
	}
	if got := mixTemperature(w, 0.95); got != mixTempAt(2) {
		t.Fatalf("thinking %v", got)
	}
	if got := mixTemperature(w, 1); got != mixTempAt(3) {
		t.Fatalf("pro %v", got)
	}
}
