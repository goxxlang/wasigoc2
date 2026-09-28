// Port of RmlUi Source/Core/Texture.cpp and the TextureSource half of
// Include/RmlUi/Core/Texture.h. The render-side methods live with the
// RenderManager port.
package rmlui

// TextureSource is Rml::TextureSource: a texture path together with the
// document it was declared in, resolved lazily against that document.
type TextureSource struct {
	source           string
	definitionSource string
	// textures caches one loaded texture per render manager.
	textures map[*RenderManager]Texture
}

func NewTextureSource(source string, documentPath string) *TextureSource {
	return &TextureSource{source: source, definitionSource: documentPath, textures: map[*RenderManager]Texture{}}
}

func (t *TextureSource) GetSource() string           { return t.source }
func (t *TextureSource) GetDefinitionSource() string { return t.definitionSource }

// GetTexture is TextureSource::GetTexture: loads (once per render manager)
// the image through the render manager's texture database.
func (t *TextureSource) GetTexture(renderManager *RenderManager) Texture {
	if tex, ok := t.textures[renderManager]; ok {
		return tex
	}
	tex := renderManager.LoadTexture(t.source, t.definitionSource)
	t.textures[renderManager] = tex
	return tex
}
