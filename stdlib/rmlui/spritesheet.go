// Port of RmlUi Source/Core/Spritesheet.cpp and
// Include/RmlUi/Core/Spritesheet.h.
package rmlui

// Sprite is Rml::Sprite.
type Sprite struct {
	Rectangle   Rectanglef // in 'px' units
	SpriteSheet *Spritesheet
}

// Spritesheet is Rml::Spritesheet.
type Spritesheet struct {
	Name                 string
	DefinitionLineNumber int
	DisplayScale         float32 // the inverse of the 'resolution' property
	TextureSource        *TextureSource
}

func NewSpritesheet(name string, source string, documentPath string, definitionLineNumber int, displayScale float32) *Spritesheet {
	return &Spritesheet{Name: name, DefinitionLineNumber: definitionLineNumber, DisplayScale: displayScale, TextureSource: NewTextureSource(source, documentPath)}
}

// SpriteDefinition is one entry of Rml::SpriteDefinitionList.
type SpriteDefinition struct {
	Name      string
	Rectangle Rectanglef
}

// SpritesheetList is Rml::SpritesheetList.
type SpritesheetList struct {
	spritesheets []*Spritesheet
	spriteMap    map[string]*Sprite
}

func NewSpritesheetList() *SpritesheetList {
	return &SpritesheetList{spriteMap: map[string]*Sprite{}}
}

func (l *SpritesheetList) AddSpriteSheet(name string, imageSource string, definitionSource string, definitionLineNumber int, displayScale float32, definitions []SpriteDefinition) bool {
	sheet := NewSpritesheet(name, imageSource, definitionSource, definitionLineNumber, displayScale)
	l.spritesheets = append(l.spritesheets, sheet)
	for _, def := range definitions {
		if existing, ok := l.spriteMap[def.Name]; ok && existing.SpriteSheet != nil {
			LogMessage(LogWarning, "Sprite '"+def.Name+"' was overwritten due to duplicate names at the same block scope. Declared at "+
				existing.SpriteSheet.TextureSource.GetDefinitionSource()+":"+FormatInt(existing.SpriteSheet.DefinitionLineNumber)+" and "+
				definitionSource+":"+FormatInt(definitionLineNumber))
		}
		l.spriteMap[def.Name] = &Sprite{Rectangle: def.Rectangle, SpriteSheet: sheet}
	}
	return true
}

func (l *SpritesheetList) GetSprite(name string) *Sprite {
	if s, ok := l.spriteMap[name]; ok {
		return s
	}
	return nil
}

func (l *SpritesheetList) Merge(other *SpritesheetList) {
	sheets := other.spritesheets
	for _, s := range sheets {
		l.spritesheets = append(l.spritesheets, s)
	}
	sprites := other.spriteMap
	for name, sprite := range sprites {
		l.spriteMap[name] = sprite
	}
}

func (l *SpritesheetList) NumSpriteSheets() int { return len(l.spritesheets) }
func (l *SpritesheetList) NumSprites() int      { return len(l.spriteMap) }

// Clone copies the list (the C++ copy constructor, used by CombineStyleSheet).
func (l *SpritesheetList) Clone() *SpritesheetList {
	c := NewSpritesheetList()
	c.Merge(l)
	return c
}
