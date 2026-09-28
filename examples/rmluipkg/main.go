package main

import (
	"fmt"
	"guikit"
	"rmlui"
)

func check(name string, ok bool) {
	if ok {
		fmt.Println("ok", name)
	} else {
		fmt.Println("FAIL", name)
	}
}

func near(a float32, b float32) bool {
	d := a - b
	if d < 0 {
		d = -d
	}
	return d < 0.001
}

func main() {
	check("round", rmlui.MathRound(2.5) == 3 && rmlui.MathRound(-2.5) == -2)
	check("sin", near(rmlui.MathSin(rmlui.Pi/6), 0.5))
	check("cos", near(rmlui.MathCos(rmlui.Pi/3), 0.5))
	check("atan2", near(rmlui.MathATan2(1, 1), rmlui.Pi/4))
	check("sqrt", near(rmlui.MathSquareRoot(1e6), 1000))
	check("floor-fltmax", rmlui.MathRoundDown(rmlui.FltMax) == rmlui.FltMax)
	o, w := rmlui.MathSnapToPixelGrid(0.4, 10.3)
	check("snap", o == 0 && w == 11)
	r := rmlui.RectanglefFromPositionSize(rmlui.Vector2f{1, 2}, rmlui.Vector2f{3, 4})
	check("rect", r.Right() == 4 && r.Bottom() == 6 && r.Contains(rmlui.Vector2f{2, 3}))
	c := rmlui.Colourb{255, 0, 0, 128}.ToPremultiplied()
	check("premul", c.Red == 128 && c.Alpha == 128)

	quiet := rmlui.NewDefaultSystemInterface()
	quiet.Quiet = true
	rmlui.SetSystemInterface(quiet)
	rmlui.Initialise()
	check("version", rmlui.GetVersion() == "6.3")

	ctx := rmlui.CreateContext("main", rmlui.Vector2i{400, 300})
	rml := "<rml><head><style>body { width: 200px; height: 100px; } div { display: block; }</style></head><body><div id=\"box\">Hi</div></body></rml>"
	doc := ctx.LoadDocumentFromMemory(rml)
	check("rml-doc", doc != nil)
	ctx.Update()
	box := doc.GetElementById("box")
	width := float32(0)
	if box != nil {
		width = box.GetBox().GetSize(rmlui.BoxAreaContent).X
	}
	check("rml-width", width == 200)

	src := "state { Count = 1 }\n" +
		"style { body { width: 180px; height: 40px; } span { display: inline-block; } }\n" +
		"view { span#n(\"{{ Count }}\") button#plus :gk-click.\"inc\" (\"+\") }\n" +
		"handle inc { Count = Count + 1 }\n"
	gdoc, err := guikit.Load(ctx, src)
	check("gml", err == nil && gdoc != nil)
	n := gdoc.GetElementById("n")
	text := ""
	if n != nil {
		text = n.GetInnerRML()
	}
	check("count1", text == "1")
	check("invoke", guikit.Invoke(gdoc, "inc", nil) == nil)
	text = ""
	if n != nil {
		text = n.GetInnerRML()
	}
	check("count2", text == "2")
	btn := gdoc.GetElementById("plus")
	if btn != nil {
		btn.Click()
	}
	text = ""
	if n != nil {
		text = n.GetInnerRML()
	}
	check("count3", text == "3")
}
