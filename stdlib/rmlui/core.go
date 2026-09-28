// Port of RmlUi Source/Core/Core.cpp, PluginRegistry.cpp, and the
// system/file singletons from SystemInterface.cpp and FileInterface.cpp.
package rmlui

// Plugin is the subset of Rml::Plugin the core notifies. Hosts that need
// the rest of the C++ hook list can wrap this and watch the context.
type Plugin interface {
	OnElementDestroy(element *Element)
}

var (
	coreAlive      bool
	systemIface    SystemInterface
	fileIface      FileInterface
	plugins        []Plugin
	defaultRCSS    = ""
	cachedDefaults *StyleSheet
)

// nullRenderInterface is the stand-in RenderInterface used until the host
// installs its own. Every call is a no-op, matching an unimplemented
// optional method on the C++ base.
type nullRenderInterface struct{}

func (n nullRenderInterface) CompileGeometry(vertices []Vertex, indices []int) CompiledGeometryHandle {
	return 0
}
func (n nullRenderInterface) RenderGeometry(geometry CompiledGeometryHandle, translation Vector2f, texture TextureHandle) {
}
func (n nullRenderInterface) ReleaseGeometry(geometry CompiledGeometryHandle) {}
func (n nullRenderInterface) LoadTexture(source string) (TextureHandle, int, int) {
	return 0, 0, 0
}
func (n nullRenderInterface) GenerateTexture(source []byte, dimensions Vector2i) TextureHandle {
	return 0
}
func (n nullRenderInterface) ReleaseTexture(texture TextureHandle)                 {}
func (n nullRenderInterface) EnableScissorRegion(enable bool)                      {}
func (n nullRenderInterface) SetScissorRegion(region Rectanglei)                   {}
func (n nullRenderInterface) EnableClipMask(enable bool)                           {}
func (n nullRenderInterface) RenderToClipMask(operation ClipMaskOperation, geometry CompiledGeometryHandle, translation Vector2f) {
}
func (n nullRenderInterface) SetTransform(transform *Matrix4f) {}
func (n nullRenderInterface) PushLayer() LayerHandle           { return 0 }
func (n nullRenderInterface) CompositeLayers(source LayerHandle, destination LayerHandle, blendMode BlendMode, filters []CompiledFilterHandle) {
}
func (n nullRenderInterface) PopLayer()                        {}
func (n nullRenderInterface) SaveLayerAsTexture() TextureHandle { return 0 }
func (n nullRenderInterface) SaveLayerAsMaskImage() CompiledFilterHandle {
	return 0
}
func (n nullRenderInterface) CompileFilter(name string, parameters map[string]Variant) CompiledFilterHandle {
	return 0
}
func (n nullRenderInterface) ReleaseFilter(filter CompiledFilterHandle) {}
func (n nullRenderInterface) CompileShader(name string, parameters map[string]Variant) CompiledShaderHandle {
	return 0
}
func (n nullRenderInterface) RenderShader(shader CompiledShaderHandle, geometry CompiledGeometryHandle, translation Vector2f, texture TextureHandle) {
}
func (n nullRenderInterface) ReleaseShader(shader CompiledShaderHandle) {}

func initDefaultRCSS() {
	defaultRCSS = "" +
		"body, div, p, h1, h2, h3, h4, h5, h6, ul, ol, li, section, header, footer, nav, main, article, form, table, tr, hr, br { display: block; }\n" +
		"button, input, textarea, select, img, span { display: inline-block; }\n" +
		"body { position: relative; width: 100%; height: 100%; overflow: auto; }\n"
}

// Initialise is Rml::Initialise. A second call is a no-op success.
func Initialise() bool {
	if coreAlive {
		return true
	}
	EventSpecificationInitialize()
	StyleSheetSpecificationInitialise()
	StyleSheetParserInitialise()
	StyleSheetFactoryInitialise()
	FactoryInitialise()
	BoxShadowCacheInitialize()
	if systemIface == nil {
		systemIface = NewDefaultSystemInterface()
	}
	if fileIface == nil {
		fileIface = &FileInterfaceDefault{}
	}
	if fontEngine == nil {
		fontEngine = NewDefaultFontEngine()
	}
	initDefaultRCSS()
	coreAlive = true
	return true
}

// Shutdown is Rml::Shutdown.
func Shutdown() {
	if !coreAlive {
		return
	}
	contexts = map[string]*Context{}
	cachedDefaults = nil
	FactoryShutdown()
	StyleSheetFactoryShutdown()
	StyleSheetParserShutdown()
	StyleSheetSpecificationShutdown()
	EventSpecificationShutdown()
	BoxShadowCacheShutdown()
	plugins = nil
	coreAlive = false
}

// GetVersion is Rml::GetVersion.
func GetVersion() string { return RmlUiVersion }

// GetSystemInterface is Rml::GetSystemInterface.
func GetSystemInterface() SystemInterface { return systemIface }

// SetSystemInterface is Rml::SetSystemInterface.
func SetSystemInterface(iface SystemInterface) { systemIface = iface }

// GetFileInterface is Rml::GetFileInterface.
func GetFileInterface() FileInterface { return fileIface }

// SetFileInterface is Rml::SetFileInterface.
func SetFileInterface(iface FileInterface) { fileIface = iface }

// RegisterPlugin is PluginRegistry::RegisterPlugin.
func RegisterPlugin(plugin Plugin) {
	if plugin == nil {
		return
	}
	plugins = append(plugins, plugin)
}

// PluginRegistryNotifyElementDestroy is the Element destructor hook.
func PluginRegistryNotifyElementDestroy(element *Element) {
	list := plugins
	for _, plugin := range list {
		plugin.OnElementDestroy(element)
	}
}

// DefaultStyleSheet is the built-in RCSS every document starts from
// (body is a block, headings are blocks, controls are inline-block).
func DefaultStyleSheet(context *Context) *StyleSheet {
	if cachedDefaults != nil {
		return cachedDefaults
	}
	Initialise()
	cachedDefaults = FactoryInstanceStyleSheetString(defaultRCSS, context)
	return cachedDefaults
}
