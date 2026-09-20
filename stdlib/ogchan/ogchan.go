// Package ogchan is a clean-room Open Graph document channel.
//
// Specs used (public documents, not messenger source):
//   - Open Graph Protocol: https://ogp.me/  (og:title, og:type, og:image, og:url, og:description)
//   - WhatsApp link previews: OG tags in the first 300 KiB of HTML;
//     Cloud API text messages with preview_url on the first https URL
//   - Signal DataMessage.preview field names: url, title, description, date
//
// This is not a derivative of Signal-Desktop or the WhatsApp Node SDK.
package ogchan

import (
	"html"
	"strconv"
	"strings"
)

const headScanBytes = 300 * 1024

type Card struct {
	URL         string
	Title       string
	Description string
	ImageURL    string
	ImageType   string
	SiteName    string
	Type        string
	PublishedAt string
}

func isSpace(c byte) bool {
	return c == ' ' || c == '\n' || c == '\r' || c == '\t'
}

func indexFold(s string, sub string) int {
	m := len(sub)
	if m == 0 {
		return 0
	}
	if m > len(s) {
		return -1
	}
	lim := len(s) - m
	for i := 0; i <= lim; i++ {
		if strings.EqualFold(s[i:i+m], sub) {
			return i
		}
	}
	return -1
}

func stripHash(u string) string {
	h := strings.Index(u, "#")
	if h >= 0 {
		return u[0:h]
	}
	return u
}

func originOf(u string) string {
	if !strings.HasPrefix(strings.ToLower(u), "https://") {
		return ""
	}
	rest := u[8:]
	slash := strings.Index(rest, "/")
	if slash < 0 {
		return stripHash(u)
	}
	return u[0 : 8+slash]
}

func dirOf(u string) string {
	u = stripHash(u)
	slash := strings.LastIndex(u, "/")
	if slash < 8 {
		return u + "/"
	}
	return u[0 : slash+1]
}

func ResolveHTTPS(ref string, base string) string {
	ref = strings.TrimSpace(ref)
	if ref == "" {
		return ""
	}
	low := strings.ToLower(ref)
	if strings.HasPrefix(low, "https://") {
		return stripHash(ref)
	}
	if strings.HasPrefix(low, "http://") {
		return ""
	}
	if base == "" {
		return ""
	}
	if strings.HasPrefix(ref, "/") {
		o := originOf(base)
		if o == "" {
			return ""
		}
		return o + ref
	}
	return dirOf(base) + ref
}

func parseAttrs(raw string) map[string]string {
	out := make(map[string]string)
	i := 0
	n := len(raw)
	for i < n {
		for i < n && isSpace(raw[i]) {
			i++
		}
		if i >= n {
			break
		}
		if raw[i] == '/' {
			i++
			continue
		}
		start := i
		for i < n && raw[i] != '=' && !isSpace(raw[i]) && raw[i] != '/' {
			i++
		}
		name := strings.ToLower(raw[start:i])
		for i < n && isSpace(raw[i]) {
			i++
		}
		val := ""
		if i < n && raw[i] == '=' {
			i++
			for i < n && isSpace(raw[i]) {
				i++
			}
			if i < n && (raw[i] == '"' || raw[i] == '\'') {
				q := raw[i]
				i++
				vs := i
				for i < n && raw[i] != q {
					i++
				}
				val = raw[vs:i]
				if i < n {
					i++
				}
			} else {
				vs := i
				for i < n && !isSpace(raw[i]) && raw[i] != '/' {
					i++
				}
				val = raw[vs:i]
			}
		}
		if name != "" {
			out[name] = html.UnescapeString(strings.TrimSpace(val))
		}
	}
	return out
}

func extractTitle(doc string) string {
	i := indexFold(doc, "<title")
	if i < 0 {
		return ""
	}
	gt := strings.Index(doc[i:], ">")
	if gt < 0 {
		return ""
	}
	start := i + gt + 1
	j := indexFold(doc[start:], "</title>")
	if j < 0 {
		return ""
	}
	return strings.TrimSpace(html.UnescapeString(doc[start : start+j]))
}

func firstMeta(props map[string]string, names []string) string {
	for i := 0; i < len(names); i++ {
		v := props[names[i]]
		if v != "" {
			return v
		}
	}
	return ""
}

func Parse(htmlDoc string, baseURL string) (Card, bool) {
	var card Card
	doc := htmlDoc
	if len(doc) > headScanBytes {
		doc = doc[0:headScanBytes]
	}
	props := make(map[string]string)
	names := make(map[string]string)
	off := 0
	for {
		i := indexFold(doc[off:], "<meta")
		if i < 0 {
			break
		}
		i = off + i
		j := strings.Index(doc[i:], ">")
		if j < 0 {
			break
		}
		j = i + j
		attrs := parseAttrs(doc[i+5 : j])
		if attrs["property"] != "" && attrs["content"] != "" {
			key := strings.ToLower(attrs["property"])
			if props[key] == "" {
				props[key] = attrs["content"]
			}
		}
		if attrs["name"] != "" && attrs["content"] != "" {
			key := strings.ToLower(attrs["name"])
			if names[key] == "" {
				names[key] = attrs["content"]
			}
		}
		off = j + 1
	}
	title := firstMeta(props, []string{"og:title"})
	if title == "" {
		title = extractTitle(doc)
	}
	if title == "" {
		return card, false
	}
	canonical := ResolveHTTPS(firstMeta(props, []string{"og:url"}), baseURL)
	if canonical == "" {
		canonical = ResolveHTTPS(baseURL, "")
	}
	if canonical == "" {
		return card, false
	}
	card.URL = canonical
	card.Title = title
	card.Description = firstMeta(props, []string{"og:description"})
	if card.Description == "" {
		card.Description = names["description"]
	}
	card.ImageURL = ResolveHTTPS(firstMeta(props, []string{"og:image:secure_url", "og:image", "og:image:url"}), canonical)
	card.ImageType = firstMeta(props, []string{"og:image:type"})
	card.SiteName = firstMeta(props, []string{"og:site_name"})
	card.Type = firstMeta(props, []string{"og:type"})
	if card.Type == "" {
		card.Type = "website"
	}
	card.PublishedAt = firstMeta(props, []string{"article:published_time", "og:published_time", "article:modified_time"})
	return card, true
}

func metaProp(property string, content string) string {
	if content == "" {
		return ""
	}
	return `<meta property="` + property + `" content="` + html.EscapeString(content) + `">` + "\n"
}

func Encode(card Card) string {
	typ := card.Type
	if typ == "" {
		typ = "article"
	}
	body := card.Description
	return `<!doctype html>
<html prefix="og: https://ogp.me/ns#">
<head>
<meta charset="utf-8">
<title>` + html.EscapeString(card.Title) + `</title>
` + metaProp("og:title", card.Title) + metaProp("og:type", typ) + metaProp("og:url", card.URL) + metaProp("og:description", card.Description) + metaProp("og:image", card.ImageURL) + metaProp("og:image:secure_url", card.ImageURL) + metaProp("og:image:type", card.ImageType) + metaProp("og:site_name", card.SiteName) + metaProp("article:published_time", card.PublishedAt) + `</head>
<body>
<article>
<h1>` + html.EscapeString(card.Title) + `</h1>
<p>` + html.EscapeString(body) + `</p>
</article>
</body>
</html>
`
}

func jsonString(s string) string {
	var out []byte
	out = append(out, '"')
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == '\\' || c == '"' {
			out = append(out, '\\')
			out = append(out, c)
		} else if c == '\n' {
			out = append(out, '\\')
			out = append(out, 'n')
		} else if c == '\r' {
			out = append(out, '\\')
			out = append(out, 'r')
		} else if c == '\t' {
			out = append(out, '\\')
			out = append(out, 't')
		} else {
			out = append(out, c)
		}
	}
	out = append(out, '"')
	return string(out)
}

func WhatsAppText(card Card, to string, body string) string {
	if body == "" {
		body = card.URL
	}
	return `{"messaging_product":"whatsapp","recipient_type":"individual","to":` + jsonString(to) + `,"type":"text","text":{"preview_url":true,"body":` + jsonString(body) + `}}`
}

func SignalPreview(card Card) string {
	date := "0"
	if card.PublishedAt != "" {
		n, err := strconv.ParseInt(card.PublishedAt, 10, 64)
		if err == nil {
			date = strconv.FormatInt(n, 10)
		}
	}
	return `{"url":` + jsonString(card.URL) + `,"title":` + jsonString(card.Title) + `,"description":` + jsonString(card.Description) + `,"date":` + date + `}`
}

func FromSignal(url string, title string, description string, dateMs int64) (Card, bool) {
	var card Card
	u := ResolveHTTPS(url, "")
	if u == "" || strings.TrimSpace(title) == "" {
		return card, false
	}
	card.URL = u
	card.Title = title
	card.Description = description
	card.Type = "article"
	if dateMs > 0 {
		card.PublishedAt = strconv.FormatInt(dateMs, 10)
	}
	return card, true
}

func FirstHTTPS(text string) string {
	low := strings.ToLower(text)
	i := strings.Index(low, "https://")
	if i < 0 {
		return ""
	}
	rest := text[i:]
	end := len(rest)
	for k := 0; k < len(rest); k++ {
		c := rest[k]
		if c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '"' || c == '<' || c == '>' {
			end = k
			break
		}
	}
	u := rest[0:end]
	for len(u) > 0 {
		last := u[len(u)-1]
		if last == '.' || last == ',' || last == ')' || last == ';' || last == '!' {
			u = u[0 : len(u)-1]
		} else {
			break
		}
	}
	return ResolveHTTPS(u, "")
}
