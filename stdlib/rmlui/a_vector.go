package rmlui

// Vector2i is Rml::Vector2i. It lives in this file so the type is complete
// before any interface result struct (RenderInterface.LoadTexture) is emitted.
type Vector2i struct {
	X int
	Y int
}
