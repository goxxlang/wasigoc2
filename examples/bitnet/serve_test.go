package main

import "testing"

func TestExtraUser(t *testing.T) {
	old := []histMsg{{"system", "be brief"}, {"user", "hi"}, {"assistant", "yo"}}
	neu := append(append([]histMsg{}, old...), histMsg{"user", "again"})
	text, ok := extraUser(old, neu)
	if !ok || text != "again" {
		t.Fatalf("continuation %q %v", text, ok)
	}
	if _, ok := extraUser(old, old); ok {
		t.Fatal("same list is not a new turn")
	}
	changed := append([]histMsg{}, old...)
	changed[2].content = "nope"
	changed = append(changed, histMsg{"user", "again"})
	if _, ok := extraUser(old, changed); ok {
		t.Fatal("edited history must not continue")
	}
}

func TestModelName(t *testing.T) {
	if modelName(`C:\Users\grego\models\hodge-1`) != "hodge-1" {
		t.Fatal(modelName(`C:\Users\grego\models\hodge-1`))
	}
	if modelName("") != "bitnet" {
		t.Fatal("empty")
	}
}
