package main

import (
	"fmt"
	"liveview"
	"net"
	"net/http"
	"strings"
)

const counterSrc = `
live Counter {
  state {
    Count: 3
    Title: "Hi <x>"
    Items: ["a", "b"]
  }

  style {
    .n { font-size: 2rem; }
  }

  view {
    <div id="counter">
      <h1>{{ .Title }}</h1>
      <p class="n">{{ .Count }}</p>
      {{ if .Count }}<b>hot</b>{{ else }}<i>cold</i>{{ end }}
      <ul>
      {% for item in Items %}
        <li>{{ item }}</li>
      {% endfor %}
      </ul>
      <button gk-click="inc">+</button>
    </div>
  }

  event inc {
    Count += 1
  }

  event dec {
    Count -= 1
  }

  event mark {
    dml.setValue("out", "ok")
  }
}
`

const boxSrc = `
live Box {
  state { N: 1 }
  view {
    div#box.card {
      span "{{ .N }}"
      button :gk-click."inc" "+"
    }
  }
  event inc { N += 1 }
}
`

func main() {
	eng := liveview.NewEngine()
	err := eng.Add(counterSrc)
	fmt.Println(err == nil)

	c := eng.Get("counter")
	htmlOut := c.Render()
	fmt.Println(strings.Contains(htmlOut, `id="counter"`))
	fmt.Println(strings.Contains(htmlOut, "Hi &lt;x&gt;"))
	fmt.Println(strings.Contains(htmlOut, ">3<"))
	fmt.Println(strings.Contains(htmlOut, "<b>hot</b>"))
	fmt.Println(strings.Contains(htmlOut, "<li>a</li>"))
	fmt.Println(strings.Contains(htmlOut, "<li>b</li>"))
	fmt.Println(strings.Contains(htmlOut, `gk-click="inc"`))

	page := eng.PageHTML("counter")
	fmt.Println(strings.Contains(page, "<!DOCTYPE html>"))
	fmt.Println(strings.Contains(page, "font-size: 2rem"))
	fmt.Println(strings.Contains(page, `src="/live.js"`))

	p := c.Patch("inc", nil)
	fmt.Println(strings.Contains(p.HTML, ">4<"))

	p2 := c.Patch("mark", nil)
	fmt.Println(len(p2.DML) == 1)
	fmt.Println(p2.DML[0].Action == "setValue")
	fmt.Println(p2.DML[0].Value == "ok")

	engBox := liveview.NewEngine()
	err2 := engBox.Add(boxSrc)
	fmt.Println(err2 == nil)
	box := engBox.Get("box")
	boxHTML := box.Render()
	fmt.Println(strings.Contains(boxHTML, `id="box"`))
	fmt.Println(strings.Contains(boxHTML, `class="card"`))
	fmt.Println(strings.Contains(boxHTML, ">1<"))
	fmt.Println(strings.Contains(boxHTML, `gk-click="inc"`))
	box.Patch("inc", nil)
	fmt.Println(strings.Contains(box.Render(), ">2<"))

	fmt.Println(strings.Contains(liveview.ClientJS(), "/live/event"))
	fmt.Println(strings.Contains(liveview.ClientJS(), "gk-click"))

	httpEng := liveview.NewEngine()
	_ = httpEng.Add(counterSrc)
	ln, lerr := net.Listen("tcp", "127.0.0.1:34581")
	if lerr != nil {
		fmt.Println("http-skip")
	} else {
		mux := http.NewServeMux()
		httpEng.Mount(mux)
		go http.ServeHandler(ln, mux)
		st, body, _ := http.Get("127.0.0.1:34581", "/")
		fmt.Println("http-ok")
		fmt.Println(st)
		fmt.Println(strings.Contains(body, `id="counter"`))
		st2, body2, _ := http.Post("127.0.0.1:34581", "/live/event", `{"id":"counter","event":"inc","data":{}}`)
		fmt.Println(st2)
		fmt.Println(strings.Contains(body2, ">4<"))
		ln.Close()
	}
}
