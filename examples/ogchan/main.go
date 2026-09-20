package main

import (
	"fmt"
	"ogchan"
	"strings"
)

func main() {
	src := `<html><head>
<title>Fallback</title>
<meta property="og:title" content="Hello">
<meta property='og:description' content='Seam &amp; channel'>
<meta property="og:url" content="https://example.com/doc">
<meta property="og:image" content="/i.png">
<meta name="description" content="nope">
</head></html>`

	card, ok := ogchan.Parse(src, "https://example.com/doc")
	fmt.Println(ok)
	fmt.Println(card.Title == "Hello")
	fmt.Println(card.Description == "Seam & channel")
	fmt.Println(card.URL == "https://example.com/doc")
	fmt.Println(card.ImageURL == "https://example.com/i.png")

	htmlDoc := ogchan.Encode(card)
	back, ok2 := ogchan.Parse(htmlDoc, "")
	fmt.Println(ok2)
	fmt.Println(back.Title == card.Title)
	fmt.Println(back.Description == card.Description)
	fmt.Println(back.URL == card.URL)

	wa := ogchan.WhatsAppText(card, "15551234567", "")
	fmt.Println(strings.Contains(wa, `"preview_url":true`))
	fmt.Println(strings.Contains(wa, "https://example.com/doc"))

	sig := ogchan.SignalPreview(card)
	fmt.Println(strings.Contains(sig, `"url":"https://example.com/doc"`))
	fmt.Println(strings.Contains(sig, `"title":"Hello"`))

	from, ok3 := ogchan.FromSignal("https://example.com/doc", "Hello", "Seam & channel", 0)
	fmt.Println(ok3)
	fmt.Println(from.Title == "Hello")

	fmt.Println(ogchan.FirstHTTPS("see https://example.com/doc.") == "https://example.com/doc")
}
