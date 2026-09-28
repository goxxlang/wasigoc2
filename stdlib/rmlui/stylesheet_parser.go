// Port of RmlUi Source/Core/StyleSheetParser.cpp and StyleSheetParser.h.
package rmlui

// abstractPropertyParser is AbstractPropertyParser: receives each
// "name: value" declaration of a block.
type abstractPropertyParser interface {
	Parse(name string, value string) bool
}

// propertySpecificationParser passes declarations to a specification.
type propertySpecificationParser struct {
	properties    *PropertyDictionary
	specification *PropertySpecification
}

func (p *propertySpecificationParser) Parse(name string, value string) bool {
	return p.specification.ParsePropertyDeclaration(p.properties, name, value)
}

// spritesheetPropertyParser reads @spritesheet blocks: arbitrary sprite
// names whose values are rectangles, plus src and resolution.
type spritesheetPropertyParser struct {
	imageSource           string
	imageResolutionFactor float32
	spriteDefinitions     []SpriteDefinition
	properties            *PropertyDictionary
	specification         *PropertySpecification
	idSrc                 PropertyId
	idRx                  PropertyId
	idRy                  PropertyId
	idRw                  PropertyId
	idRh                  PropertyId
	idResolution          PropertyId
	idRectangle           ShorthandId
}

func newSpritesheetPropertyParser() *spritesheetPropertyParser {
	p := &spritesheetPropertyParser{imageResolutionFactor: 1, properties: NewPropertyDictionary()}
	p.specification = NewPropertySpecification(6, 1)
	p.idSrc = p.specification.RegisterProperty("src", "", false, false, PropertyIdInvalid).AddParser("string", "").GetId()
	p.idRx = p.specification.RegisterProperty("rectangle-x", "", false, false, PropertyIdInvalid).AddParser("length", "").GetId()
	p.idRy = p.specification.RegisterProperty("rectangle-y", "", false, false, PropertyIdInvalid).AddParser("length", "").GetId()
	p.idRw = p.specification.RegisterProperty("rectangle-w", "", false, false, PropertyIdInvalid).AddParser("length", "").GetId()
	p.idRh = p.specification.RegisterProperty("rectangle-h", "", false, false, PropertyIdInvalid).AddParser("length", "").GetId()
	p.idRectangle = p.specification.RegisterShorthand("rectangle", "rectangle-x, rectangle-y, rectangle-w, rectangle-h", ShorthandTypeFallThrough, ShorthandIdInvalid)
	p.idResolution = p.specification.RegisterProperty("resolution", "", false, false, PropertyIdInvalid).AddParser("resolution", "").GetId()
	return p
}

func (p *spritesheetPropertyParser) Clear() {
	p.imageResolutionFactor = 1
	p.imageSource = ""
	p.spriteDefinitions = nil
}

func (p *spritesheetPropertyParser) Parse(name string, value string) bool {
	if name == "src" {
		if !p.specification.ParsePropertyDeclarationId(p.properties, p.idSrc, value) {
			return false
		}
		if prop := p.properties.GetProperty(p.idSrc); prop != nil && prop.Unit == UnitSTRING {
			p.imageSource = prop.Value.GetString()
		}
	} else if name == "resolution" {
		if !p.specification.ParsePropertyDeclarationId(p.properties, p.idResolution, value) {
			return false
		}
		if prop := p.properties.GetProperty(p.idResolution); prop != nil && prop.Unit == UnitX {
			p.imageResolutionFactor = prop.Value.GetFloat()
		}
	} else {
		if !p.specification.ParseShorthandDeclaration(p.properties, p.idRectangle, value) {
			return false
		}
		position := Vector2f{}
		size := Vector2f{}
		if prop := p.properties.GetProperty(p.idRx); prop != nil {
			position.X = prop.Value.GetFloat()
		}
		if prop := p.properties.GetProperty(p.idRy); prop != nil {
			position.Y = prop.Value.GetFloat()
		}
		if prop := p.properties.GetProperty(p.idRw); prop != nil {
			size.X = prop.Value.GetFloat()
		}
		if prop := p.properties.GetProperty(p.idRh); prop != nil {
			size.Y = prop.Value.GetFloat()
		}
		p.spriteDefinitions = append(p.spriteDefinitions, SpriteDefinition{Name: name, Rectangle: RectanglefFromPositionSize(position, size)})
	}
	return true
}

// mediaQueryPropertyParser reads the feature list of an @media rule.
type mediaQueryPropertyParser struct {
	properties    *PropertyDictionary
	specification *PropertySpecification
}

func newMediaQueryPropertyParser() *mediaQueryPropertyParser {
	p := &mediaQueryPropertyParser{specification: NewPropertySpecification(14, 0)}
	s := p.specification
	s.RegisterProperty("width", "", false, false, MediaQueryIdWidth).AddParser("length", "")
	s.RegisterProperty("min-width", "", false, false, MediaQueryIdMinWidth).AddParser("length", "")
	s.RegisterProperty("max-width", "", false, false, MediaQueryIdMaxWidth).AddParser("length", "")
	s.RegisterProperty("height", "", false, false, MediaQueryIdHeight).AddParser("length", "")
	s.RegisterProperty("min-height", "", false, false, MediaQueryIdMinHeight).AddParser("length", "")
	s.RegisterProperty("max-height", "", false, false, MediaQueryIdMaxHeight).AddParser("length", "")
	s.RegisterProperty("aspect-ratio", "", false, false, MediaQueryIdAspectRatio).AddParser("ratio", "")
	s.RegisterProperty("min-aspect-ratio", "", false, false, MediaQueryIdMinAspectRatio).AddParser("ratio", "")
	s.RegisterProperty("max-aspect-ratio", "", false, false, MediaQueryIdMaxAspectRatio).AddParser("ratio", "")
	s.RegisterProperty("resolution", "", false, false, MediaQueryIdResolution).AddParser("resolution", "")
	s.RegisterProperty("min-resolution", "", false, false, MediaQueryIdMinResolution).AddParser("resolution", "")
	s.RegisterProperty("max-resolution", "", false, false, MediaQueryIdMaxResolution).AddParser("resolution", "")
	s.RegisterProperty("orientation", "", false, false, MediaQueryIdOrientation).AddParser("keyword", "landscape, portrait")
	s.RegisterProperty("theme", "", false, false, MediaQueryIdTheme).AddParser("string", "")
	return p
}

func (p *mediaQueryPropertyParser) Parse(name string, value string) bool {
	return p.specification.ParsePropertyDeclaration(p.properties, name, value)
}

// fontFacePropertyParser reads @font-face blocks.
type fontFacePropertyParser struct {
	properties    *PropertyDictionary
	specification *PropertySpecification
	sources       []string
}

func newFontFacePropertyParser() *fontFacePropertyParser {
	p := &fontFacePropertyParser{specification: NewPropertySpecification(5, 0)}
	s := p.specification
	s.RegisterProperty("font-family", "", false, false, FontFaceIdFontFamily).AddParser("string", "")
	s.RegisterProperty("font-weight", "all", false, false, FontFaceIdFontWeight).AddParser("keyword", "all=0, normal=400, bold=700").AddParser("number", "")
	s.RegisterProperty("font-style", "normal", false, false, FontFaceIdFontStyle).AddParser("keyword", "normal, italic")
	s.RegisterProperty("-rmlui-fallback-face", "false", false, false, FontFaceIdFallbackFace).AddParser("keyword", "false, true")
	s.RegisterProperty("-rmlui-face-index", "0", false, false, FontFaceIdFaceIndex).AddParser("number", "")
	return p
}

func (p *fontFacePropertyParser) Clear() {
	p.properties = nil
	p.sources = nil
}

func (p *fontFacePropertyParser) Parse(name string, value string) bool {
	if name == "src" {
		p.sources = append(p.sources, StringExpandList(value)...)
		return true
	}
	return p.specification.ParsePropertyDeclaration(p.properties, name, value)
}

type styleSheetParserData struct {
	spritesheet *spritesheetPropertyParser
	mediaQuery  *mediaQueryPropertyParser
	fontFace    *fontFacePropertyParser
}

var styleSheetPropertyParsers *styleSheetParserData

// StyleSheetParserInitialise is StyleSheetParser::Initialise.
func StyleSheetParserInitialise() {
	styleSheetPropertyParsers = &styleSheetParserData{
		spritesheet: newSpritesheetPropertyParser(),
		mediaQuery:  newMediaQueryPropertyParser(),
		fontFace:    newFontFacePropertyParser(),
	}
}

func StyleSheetParserShutdown() { styleSheetPropertyParsers = nil }

// StyleSheetParser is Rml::StyleSheetParser.
type StyleSheetParser struct {
	stream         *Stream
	streamFileName string
	lineNumber     int
	parseBuffer    string
	parseBufferPos int
}

func NewStyleSheetParser() *StyleSheetParser { return &StyleSheetParser{} }

func isValidIdentifier(str string) bool {
	if str == "" {
		return false
	}
	for i := 0; i < len(str); i++ {
		if !isIdentifierChar(str[i]) {
			return false
		}
	}
	return true
}

func unescapeSelectorToken(token string) string {
	out := []byte{}
	for i := 0; i < len(token); i++ {
		c := token[i]
		if c == '\\' && i+1 < len(token) {
			out = append(out, token[i+1])
			i++
		} else {
			out = append(out, c)
		}
	}
	return string(out)
}

func findAttributeSelectorEnd(rule string, startIndex int) int {
	var quote byte = 0
	for index := startIndex + 1; index < len(rule); index++ {
		c := rule[index]
		if (c == '\'' || c == '"') && !isEscapedCharacter(rule, index) {
			if quote == 0 {
				quote = c
			} else if quote == c {
				quote = 0
			}
		} else if c == ']' && quote == 0 && !isEscapedCharacter(rule, index) {
			return index
		}
	}
	return -1
}

func findFirstUnescaped(s string, begin int, end int, tokens string) int {
	for index := begin; index < end; index++ {
		if indexByte(tokens, s[index]) >= 0 && !isEscapedCharacter(s, index) {
			return index
		}
	}
	return -1
}

func parseAttributeSelector(rule string, begin int, end int) AttributeSelector {
	attribute := AttributeSelector{}
	iOperator := findFirstUnescaped(rule, begin, end, "=~|^$*")
	iNameEnd := end
	if iOperator >= 0 && iOperator < end {
		iNameEnd = iOperator
	}
	attribute.Name = unescapeSelectorToken(StringStripWhitespace(rule[begin:iNameEnd]))
	if iOperator < 0 {
		return attribute
	}
	c := rule[iOperator]
	attribute.Type = int(c)
	valueBegin := iOperator + 2
	if c == '=' {
		valueBegin = iOperator + 1
	}
	if valueBegin > end {
		valueBegin = end
	}
	value := StringStripWhitespace(rule[valueBegin:end])
	if len(value) >= 2 && ((value[0] == '"' && value[len(value)-1] == '"') || (value[0] == '\'' && value[len(value)-1] == '\'')) {
		value = value[1 : len(value)-1]
	}
	attribute.Value = unescapeSelectorToken(value)
	return attribute
}

func postprocessKeyframes(keyframesMap map[string]*Keyframes) {
	for _, keyframes := range keyframesMap {
		blocks := keyframes.Blocks
		for i := 1; i < len(blocks); i++ {
			j := i
			for j > 0 && blocks[j].NormalizedTime < blocks[j-1].NormalizedTime {
				blocks[j], blocks[j-1] = blocks[j-1], blocks[j]
				j--
			}
		}
		seen := PropertyIdSet{}
		for _, block := range blocks {
			props := block.Properties.GetProperties()
			for id := range props {
				seen.Insert(id)
			}
		}
		keyframes.PropertyIds = seen.Ids()
	}
}

// scanPercent is sscanf("%f%%%n"): a number immediately followed by '%'.
func scanPercent(rule string) (float32, bool, bool) {
	f, rest, ok := Strtof(rule)
	if !ok {
		return 0, false, false
	}
	return f, true, len(rest) > 0 && rest[0] == '%'
}

func (p *StyleSheetParser) location() string {
	return p.streamFileName + ":" + FormatInt(p.lineNumber)
}

func (p *StyleSheetParser) parseKeyframeBlock(keyframesMap map[string]*Keyframes, identifier string, rules string, properties *PropertyDictionary) bool {
	if !isValidIdentifier(identifier) {
		LogMessage(LogWarning, "Invalid keyframes identifier '"+identifier+"' at "+p.location())
		return false
	}
	if properties.Empty() {
		return true
	}
	ruleList := StringExpandList(rules)
	ruleValues := []float32{}
	for _, rule := range ruleList {
		rule = StringToLower(rule)
		if rule == "from" {
			ruleValues = append(ruleValues, 0)
		} else if rule == "to" {
			ruleValues = append(ruleValues, 1)
		} else {
			value, matched, percent := scanPercent(rule)
			if matched && percent && value >= 0 && value <= 100 {
				ruleValues = append(ruleValues, 0.01*value)
			}
		}
	}
	if len(ruleValues) == 0 {
		LogMessage(LogWarning, "Invalid keyframes rule(s) '"+rules+"' at "+p.location())
		return false
	}
	keyframes, ok := keyframesMap[identifier]
	if !ok {
		keyframes = &Keyframes{}
		keyframesMap[identifier] = keyframes
	}
	for _, selector := range ruleValues {
		var found *KeyframeBlock
		blocks := keyframes.Blocks
		for _, block := range blocks {
			if MathAbsolute(block.NormalizedTime-selector) < 0.0001 {
				found = block
				break
			}
		}
		if found == nil {
			found = &KeyframeBlock{NormalizedTime: selector, Properties: NewPropertyDictionary()}
			keyframes.Blocks = append(keyframes.Blocks, found)
		} else {
			// Duplicate keyframes: the latest definition wins, as in CSS.
			found.Properties = NewPropertyDictionary()
		}
		found.Properties.Import(properties, 0)
	}
	return true
}

func (p *StyleSheetParser) parseDecoratorBlock(atName string, namedDecoratorMap map[string]*NamedDecorator, source *PropertySource) bool {
	nameType := StringExpand(atName, ':', false)
	if len(nameType) != 2 || nameType[0] == "" || nameType[1] == "" {
		LogMessage(LogWarning, "Decorator syntax error at "+p.location()+". Use syntax: '@decorator name : type { ... }'.")
		return false
	}
	name := nameType[0]
	decoratorType := nameType[1]
	if _, exists := namedDecoratorMap[name]; exists {
		LogMessage(LogWarning, "Decorator with name '"+name+"' already declared, ignoring decorator at "+p.location()+".")
		return false
	}
	instancer := FactoryGetDecoratorInstancer(decoratorType)
	properties := NewPropertyDictionary()
	if instancer == nil {
		if parent, ok := namedDecoratorMap[decoratorType]; ok {
			instancer = FactoryGetDecoratorInstancer(parent.Type)
			properties = parent.Properties.Clone()
			decoratorType = parent.Type
		}
		if instancer == nil {
			LogMessage(LogWarning, "Invalid decorator type '"+decoratorType+"' declared at "+p.location()+".")
			return false
		}
	}
	specification := instancer.GetPropertySpecification()
	parser := &propertySpecificationParser{properties: properties, specification: specification}
	p.readProperties(parser)
	specification.SetPropertyDefaults(properties)
	properties.SetSourceOfAllProperties(source)
	namedDecoratorMap[name] = &NamedDecorator{Type: decoratorType, Instancer: instancer, Properties: properties}
	return true
}

func (p *StyleSheetParser) parseFontFaceBlock(source *PropertySource) bool {
	ff := styleSheetPropertyParsers.fontFace
	properties := NewPropertyDictionary()
	ff.properties = properties
	p.readProperties(ff)
	ff.specification.SetPropertyDefaults(properties)
	properties.SetSourceOfAllProperties(source)
	if properties.GetProperty(FontFaceIdFontFamily) == nil {
		LogMessage(LogWarning, "@font-face block missing font-family at "+p.location()+".")
		return false
	}
	if len(ff.sources) == 0 {
		LogMessage(LogWarning, "@font-face block missing src at "+p.location()+".")
		return false
	}
	family := properties.GetProperty(FontFaceIdFontFamily).Value.GetString()
	isFallback := properties.GetProperty(FontFaceIdFallbackFace).Value.GetBool()
	faceIndex := properties.GetProperty(FontFaceIdFaceIndex).Value.GetInt()
	weight := properties.GetProperty(FontFaceIdFontWeight).Value.GetInt()
	style := properties.GetProperty(FontFaceIdFontStyle).Value.GetInt()
	sources := ff.sources
	for _, src := range sources {
		LoadFontFaceFromFile(src, family, style, weight, isFallback, faceIndex)
	}
	return true
}

const (
	mqGlobal = iota
	mqName
	mqValue
)

func (p *StyleSheetParser) parseMediaFeatureMap(rules string, properties *PropertyDictionary) (MediaQueryModifier, bool) {
	mq := styleSheetPropertyParsers.mediaQuery
	mq.properties = properties
	state := mqGlobal
	name := ""
	current := ""
	modifier := MediaQueryModifierNone
	for cursor := 0; cursor < len(rules); cursor++ {
		c := rules[cursor]
		switch c {
		case ' ':
			if state == mqGlobal {
				current = StringStripWhitespace(StringToLower(current))
				if current == "not" {
					if modifier != MediaQueryModifierNone {
						LogMessage(LogWarning, "Unexpected '"+current+"' in @media query list at "+p.location()+".")
						return modifier, false
					}
					modifier = MediaQueryModifierNot
					current = ""
				}
			}
		case '(':
			if state != mqGlobal {
				LogMessage(LogWarning, "Unexpected '(' in @media query list at "+p.location()+".")
				return modifier, false
			}
			current = StringStripWhitespace(StringToLower(current))
			if current != "and" && (!properties.Empty() || current != "") {
				LogMessage(LogWarning, "Unexpected '"+current+"' in @media query list at "+p.location()+". Expected 'and'.")
				return modifier, false
			}
			current = ""
			state = mqName
		case ')':
			if state != mqValue {
				LogMessage(LogWarning, "Unexpected ')' in @media query list at "+p.location()+".")
				return modifier, false
			}
			current = StringStripWhitespace(current)
			if !mq.Parse(name, current) {
				LogMessage(LogWarning, "Syntax error parsing media-query property declaration '"+name+": "+current+";' in "+p.location()+".")
			}
			current = ""
			state = mqGlobal
		case ':':
			if state != mqName {
				LogMessage(LogWarning, "Unexpected ':' in @media query list at "+p.location()+".")
				return modifier, false
			}
			current = StringStripWhitespace(StringToLower(current))
			if !isValidIdentifier(current) {
				LogMessage(LogWarning, "Malformed property name '"+current+"' in @media query list at "+p.location()+".")
				return modifier, false
			}
			name = current
			current = ""
			state = mqValue
		default:
			current = current + string([]byte{c})
		}
	}
	if properties.Empty() {
		LogMessage(LogWarning, "Media query list parsing yielded no properties at "+p.location()+".")
	}
	return modifier, true
}

const (
	ssGlobal = iota
	ssAtRuleIdentifier
	ssKeyframeBlock
	ssInvalid
)

// Parse reads the whole stream into media blocks.
func (p *StyleSheetParser) Parse(stream *Stream, beginLineNumber int) ([]*MediaBlock, bool) {
	styleSheets := []*MediaBlock{}
	ruleCount := 0
	p.lineNumber = beginLineNumber
	p.stream = stream
	p.parseBuffer = ""
	p.parseBufferPos = 0
	p.streamFileName = StringReplaceChar(stream.GetSourceURL().GetURL(), '|', ':')

	state := ssGlobal
	var current *MediaBlock
	insideMediaBlock := false
	atRuleName := ""

	for p.fillBuffer() {
		for {
			token, preToken := p.findAnyToken("{@}")
			if token == 0 {
				break
			}
			switch state {
			case ssGlobal:
				if token == '{' {
					if current == nil {
						current = &MediaBlock{Properties: NewPropertyDictionary(), Stylesheet: NewStyleSheet(), Modifier: MediaQueryModifierNone}
					}
					ruleLineNumber := p.lineNumber
					properties := NewPropertyDictionary()
					parser := &propertySpecificationParser{properties: properties, specification: GetPropertySpecification()}
					p.readProperties(parser)
					ruleNames := StringExpandNested(preToken, ',', '(', ')', false)
					for i := 0; i < len(ruleNames); i++ {
						source := &PropertySource{Path: p.streamFileName, LineNumber: ruleLineNumber, RuleName: unescapeSelectorToken(ruleNames[i])}
						properties.SetSourceOfAllProperties(source)
						if importSelectorProperties(current.Stylesheet.root, ruleNames[i], properties, ruleCount) == nil {
							LogMessage(LogWarning, "Invalid selector '"+ruleNames[i]+"' encountered while parsing stylesheet at "+p.location()+".")
						}
					}
					ruleCount++
				} else if token == '@' {
					state = ssAtRuleIdentifier
				} else if insideMediaBlock && token == '}' {
					postprocessKeyframes(current.Stylesheet.keyframes)
					current.Stylesheet.specificityOffset = ruleCount
					styleSheets = append(styleSheets, current)
					current = nil
					insideMediaBlock = false
				} else {
					LogMessage(LogWarning, "Invalid character '"+string([]byte{token})+"' found while parsing stylesheet at "+p.location()+". Trying to proceed.")
				}
			case ssAtRuleIdentifier:
				if token == '{' {
					if current == nil {
						current = &MediaBlock{Properties: NewPropertyDictionary(), Stylesheet: NewStyleSheet(), Modifier: MediaQueryModifierNone}
					}
					space := indexByte(preToken, ' ')
					identPart := preToken
					if space >= 0 {
						identPart = preToken[:space]
					}
					atRuleIdentifier := StringStripWhitespace(identPart)
					rest := ""
					if len(atRuleIdentifier) <= len(preToken) {
						rest = preToken[len(atRuleIdentifier):]
					}
					atRuleName = StringStripWhitespace(rest)
					if atRuleIdentifier == "keyframes" {
						state = ssKeyframeBlock
					} else if atRuleIdentifier == "decorator" {
						source := &PropertySource{Path: p.streamFileName, LineNumber: p.lineNumber, RuleName: preToken}
						p.parseDecoratorBlock(atRuleName, current.Stylesheet.namedDecoratorMap, source)
						atRuleName = ""
						state = ssGlobal
					} else if atRuleIdentifier == "spritesheet" {
						sp := styleSheetPropertyParsers.spritesheet
						p.readProperties(sp)
						if len(sp.spriteDefinitions) == 0 {
							LogMessage(LogWarning, "Spritesheet '"+atRuleName+"' has no sprites defined, ignored. At "+p.location())
						} else if sp.imageSource == "" {
							LogMessage(LogWarning, "No image source (property 'src') specified for spritesheet '"+atRuleName+"'. At "+p.location())
						} else if sp.imageResolutionFactor <= 0 || sp.imageResolutionFactor >= 100 {
							LogMessage(LogWarning, "Spritesheet resolution (property 'resolution') value must be larger than 0.0 and smaller than 100.0, given "+FormatFloat(sp.imageResolutionFactor)+". In spritesheet '"+atRuleName+"'. At "+p.location())
						} else {
							displayScale := 1.0 / sp.imageResolutionFactor
							current.Stylesheet.spritesheetList.AddSpriteSheet(atRuleName, sp.imageSource, p.streamFileName, p.lineNumber, displayScale, sp.spriteDefinitions)
						}
						sp.Clear()
						atRuleName = ""
						state = ssGlobal
					} else if atRuleIdentifier == "media" {
						if current != nil {
							postprocessKeyframes(current.Stylesheet.keyframes)
							current.Stylesheet.specificityOffset = ruleCount
							styleSheets = append(styleSheets, current)
							current = nil
						}
						featureMap := NewPropertyDictionary()
						modifier, _ := p.parseMediaFeatureMap(atRuleName, featureMap)
						current = &MediaBlock{Properties: featureMap, Stylesheet: NewStyleSheet(), Modifier: modifier}
						insideMediaBlock = true
						state = ssGlobal
					} else if atRuleIdentifier == "font-face" {
						source := &PropertySource{Path: p.streamFileName, LineNumber: p.lineNumber, RuleName: preToken}
						p.parseFontFaceBlock(source)
						styleSheetPropertyParsers.fontFace.Clear()
						atRuleName = ""
						state = ssGlobal
					} else {
						atRuleName = ""
						state = ssGlobal
						LogMessage(LogWarning, "Invalid at-rule identifier '"+atRuleIdentifier+"' found in stylesheet at "+p.location())
					}
				} else {
					LogMessage(LogWarning, "Invalid character '"+string([]byte{token})+"' found while parsing at-rule identifier in stylesheet at "+p.location())
					state = ssInvalid
				}
			case ssKeyframeBlock:
				if token == '{' {
					if current == nil {
						current = &MediaBlock{Properties: NewPropertyDictionary(), Stylesheet: NewStyleSheet(), Modifier: MediaQueryModifierNone}
					}
					properties := NewPropertyDictionary()
					parser := &propertySpecificationParser{properties: properties, specification: GetPropertySpecification()}
					p.readProperties(parser)
					p.parseKeyframeBlock(current.Stylesheet.keyframes, atRuleName, preToken, properties)
				} else if token == '}' {
					atRuleName = ""
					state = ssGlobal
				} else {
					LogMessage(LogWarning, "Invalid character '"+string([]byte{token})+"' found while parsing keyframe block in stylesheet at "+p.location())
					state = ssInvalid
				}
			default:
				state = ssInvalid
			}
			if state == ssInvalid {
				break
			}
		}
		if state == ssInvalid {
			break
		}
	}
	if current != nil {
		postprocessKeyframes(current.Stylesheet.keyframes)
		current.Stylesheet.specificityOffset = ruleCount
		styleSheets = append(styleSheets, current)
	}
	return styleSheets, len(styleSheets) > 0
}

// ParseProperties parses an inline declaration list (e.g. a style="..."
// attribute) into parsedProperties.
func (p *StyleSheetParser) ParseProperties(parsedProperties *PropertyDictionary, properties string) {
	p.stream = NewStreamMemory(properties)
	p.parseBuffer = ""
	p.parseBufferPos = 0
	parser := &propertySpecificationParser{properties: parsedProperties, specification: GetPropertySpecification()}
	p.readProperties(parser)
	p.stream = nil
}

// ConstructSelectorNodes is StyleSheetParser::ConstructNodes: builds the
// nodes for a selector list under rootNode and returns the leaves.
func ConstructSelectorNodes(rootNode *StyleSheetNode, selectors string) []*StyleSheetNode {
	empty := NewPropertyDictionary()
	selectorList := StringExpandNested(selectors, ',', '(', ')', false)
	leafs := []*StyleSheetNode{}
	for _, selector := range selectorList {
		leaf := importSelectorProperties(rootNode, selector, empty, 0)
		if leaf == nil {
			LogMessage(LogWarning, "Invalid selector '"+selector+"' encountered.")
		} else if leaf != rootNode {
			leafs = append(leafs, leaf)
		}
	}
	return leafs
}

const (
	rpName = iota
	rpValue
	rpQuote
)

func (p *StyleSheetParser) readProperties(propertyParser abstractPropertyParser) {
	name := ""
	value := ""
	state := rpName
	var previous byte = 0
	for {
		c, ok := p.readCharacter()
		if !ok {
			break
		}
		p.parseBufferPos++
		switch state {
		case rpName:
			if c == ';' {
				name = StringStripWhitespace(name)
				if name != "" {
					LogMessage(LogWarning, "Found name with no value while parsing property declaration '"+name+"' at "+p.location())
					name = ""
				}
			} else if c == '}' {
				name = StringStripWhitespace(name)
				if name != "" {
					LogMessage(LogWarning, "End of rule encountered while parsing property declaration '"+name+"' at "+p.location())
				}
				return
			} else if c == ':' {
				name = StringStripWhitespace(name)
				state = rpValue
			} else {
				name = name + string([]byte{c})
			}
		case rpValue:
			if c == ';' {
				value = StringStripWhitespace(value)
				if !propertyParser.Parse(name, value) {
					LogMessage(LogWarning, "Syntax error parsing property declaration '"+name+": "+value+";' in "+p.location()+".")
				}
				name = ""
				value = ""
				state = rpName
			} else if c == '}' {
				// handled below
			} else {
				value = value + string([]byte{c})
				if c == '"' {
					state = rpQuote
				}
			}
		case rpQuote:
			value = value + string([]byte{c})
			if c == '"' && previous != '\\' {
				state = rpValue
			}
		}
		if c == '}' {
			break
		}
		previous = c
	}
	if state == rpValue && name != "" && value != "" {
		value = StringStripWhitespace(value)
		if !propertyParser.Parse(name, value) {
			LogMessage(LogWarning, "Syntax error parsing property declaration '"+name+": "+value+";' in "+p.location()+".")
		}
	} else if StringStripWhitespace(name) != "" || value != "" {
		LogMessage(LogWarning, "Invalid property declaration '"+name+"':'"+value+"' at "+p.location())
	}
}

// importSelectorProperties is StyleSheetParser::ImportProperties: builds
// the node chain for one selector under node and imports properties onto
// the leaf. Returns nil for an invalid selector.
func importSelectorProperties(node *StyleSheetNode, rule string, properties *PropertyDictionary, ruleSpecificity int) *StyleSheetNode {
	leaf := node
	index := 0
	for index < len(rule) {
		selector := CompoundSelector{Combinator: CombinatorDescendant}
		for index > 0 && index < len(rule) {
			end := false
			switch rule[index] {
			case ' ':
			case '>':
				selector.Combinator = CombinatorChild
			case '+':
				selector.Combinator = CombinatorNextSibling
			case '~':
				selector.Combinator = CombinatorSubsequentSibling
			default:
				end = true
			}
			if end {
				break
			}
			index++
		}
		for index < len(rule) {
			startIndex := index
			endIndex := index + 1
			if rule[startIndex] == '*' {
				startIndex++
			}
			if startIndex < len(rule) && rule[startIndex] == '[' {
				endIndex = findAttributeSelectorEnd(rule, startIndex)
				if endIndex < 0 {
					return nil
				}
				endIndex++
			} else {
				parenthesisCount := 0
				if startIndex < len(rule) && rule[startIndex] == ':' {
					if endIndex < len(rule) && rule[endIndex] == ':' {
						endIndex++
					}
				}
				for endIndex < len(rule) {
					ch := rule[endIndex]
					if parenthesisCount == 0 && indexByte("#.:[ >+~", ch) >= 0 && !isEscapedCharacter(rule, endIndex) {
						break
					}
					if ch == '(' && !isEscapedCharacter(rule, endIndex) {
						parenthesisCount++
					} else if ch == ')' && !isEscapedCharacter(rule, endIndex) {
						parenthesisCount--
					}
					endIndex++
				}
			}
			if endIndex > startIndex && startIndex < len(rule) {
				switch rule[startIndex] {
				case '#':
					selector.Id = unescapeSelectorToken(rule[startIndex+1 : endIndex])
				case '.':
					selector.ClassNames = append(selector.ClassNames, unescapeSelectorToken(rule[startIndex+1:endIndex]))
				case ':':
					pseudoClassName := unescapeSelectorToken(rule[startIndex+1 : endIndex])
					nodeSelector := GetStructuralSelector(pseudoClassName)
					if nodeSelector.Type != StructuralInvalid {
						selector.StructuralSelectors = append(selector.StructuralSelectors, nodeSelector)
					} else {
						selector.PseudoClassNames = append(selector.PseudoClassNames, pseudoClassName)
					}
				case '[':
					attrBegin := startIndex + 1
					attrEnd := endIndex - 1
					if attrEnd <= attrBegin {
						return nil
					}
					selector.Attributes = append(selector.Attributes, parseAttributeSelector(rule, attrBegin, attrEnd))
				default:
					selector.Tag = unescapeSelectorToken(rule[startIndex:endIndex])
				}
			}
			index = endIndex
			if index < len(rule) && indexByte(" >+~", rule[index]) >= 0 {
				break
			}
		}
		sortStrings(selector.ClassNames)
		sortAttributeSelectors(selector.Attributes)
		sortStrings(selector.PseudoClassNames)
		sortStructuralSelectors(selector.StructuralSelectors)
		leaf = leaf.GetOrCreateChildNode(selector)
	}
	leaf.ImportProperties(properties, ruleSpecificity)
	return leaf
}

// findAnyToken reads until one of tokens, returning it and the text before.
func (p *StyleSheetParser) findAnyToken(tokens string) (byte, string) {
	buffer := []byte{}
	for {
		c, ok := p.readCharacter()
		if !ok {
			break
		}
		p.parseBufferPos++
		if indexByte(tokens, c) >= 0 {
			return c, string(buffer)
		}
		buffer = append(buffer, c)
	}
	return 0, string(buffer)
}

// readCharacter returns the next character outside comments without
// consuming it; newlines are counted and skipped, never returned.
func (p *StyleSheetParser) readCharacter() (byte, bool) {
	comment := false
	for {
		for p.parseBufferPos < len(p.parseBuffer) {
			c := p.parseBuffer[p.parseBufferPos]
			if c == '\n' {
				p.lineNumber++
			} else if comment {
				if c == '*' {
					p.parseBufferPos++
					if p.parseBufferPos >= len(p.parseBuffer) {
						if !p.fillBuffer() {
							return 0, false
						}
					}
					if p.parseBuffer[p.parseBufferPos] == '/' {
						comment = false
					} else {
						p.parseBufferPos--
					}
				}
			} else {
				if c == '/' {
					p.parseBufferPos++
					if p.parseBufferPos >= len(p.parseBuffer) {
						if !p.fillBuffer() {
							p.parseBuffer = "/"
							p.parseBufferPos = 0
							return '/', true
						}
					}
					if p.parseBuffer[p.parseBufferPos] == '*' {
						comment = true
					} else {
						if p.parseBufferPos == 0 {
							p.parseBuffer = "/" + p.parseBuffer
						} else {
							p.parseBufferPos--
						}
						return '/', true
					}
				}
				if !comment {
					return p.parseBuffer[p.parseBufferPos], true
				}
			}
			p.parseBufferPos++
		}
		if !p.fillBuffer() {
			break
		}
	}
	return 0, false
}

// fillBuffer loads the rest of the stream; the whole stream arrives at once.
func (p *StyleSheetParser) fillBuffer() bool {
	if p.stream == nil || p.stream.IsEOS() {
		return false
	}
	firstRead := p.parseBuffer == ""
	p.parseBuffer = p.stream.ReadAll()
	p.parseBufferPos = 0
	if firstRead && StringStartsWith(p.parseBuffer, "\xEF\xBB\xBF") {
		LogMessage(LogWarning, "UTF-8 BOM encountered in stylesheet "+p.streamFileName+". This is not supported and can lead to subtle issues, please remove the BOM from the file's text encoding.")
	}
	return p.parseBuffer != ""
}
