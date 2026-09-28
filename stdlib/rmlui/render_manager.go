// Port of RmlUi Source/Core/RenderManager.cpp, RenderInterface.cpp,
// Geometry.cpp, Texture.cpp, CallbackTexture.cpp, CompiledFilterShader.cpp,
// TextureDatabase.cpp, and Include/RmlUi/Core/RenderInterface.h,
// RenderManager.h, Mesh.h, Vertex.h, Geometry.h, Texture.h,
// CallbackTexture.h, CompiledFilterShader.h, StableVector.h.
package rmlui

// Render resource handles (0 means none), as in Types.h.
type CompiledGeometryHandle = int
type TextureHandle = int
type LayerHandle = int
type CompiledFilterHandle = int
type CompiledShaderHandle = int

// ClipMaskOperation is Rml::ClipMaskOperation.
type ClipMaskOperation = int

const (
	ClipMaskOperationSet ClipMaskOperation = iota
	ClipMaskOperationSetInverse
	ClipMaskOperationIntersect
)

// BlendMode is Rml::BlendMode.
type BlendMode = int

const (
	BlendModeBlend BlendMode = iota
	BlendModeReplace
)

// Vertex is Rml::Vertex.
type Vertex struct {
	Position Vector2f
	Colour   ColourbPremultiplied
	TexCoord Vector2f
}

// Mesh is Rml::Mesh.
type Mesh struct {
	Vertices []Vertex
	Indices  []int
}

func (m *Mesh) Empty() bool { return len(m.Indices) == 0 }

// TexturedMesh is Rml::TexturedMesh.
type TexturedMesh struct {
	Mesh    Mesh
	Texture Texture
}

// RenderInterface is Rml::RenderInterface, implemented by the host. The
// "optional" C++ methods (clip mask, transform, layers, filters, shaders)
// are part of the interface; hosts without those features return zero
// handles and ignore the calls, which is the C++ base behavior.
type RenderInterface interface {
	CompileGeometry(vertices []Vertex, indices []int) CompiledGeometryHandle
	RenderGeometry(geometry CompiledGeometryHandle, translation Vector2f, texture TextureHandle)
	ReleaseGeometry(geometry CompiledGeometryHandle)
	// Width and height are separate ints so the generated result struct does
	// not need Vector2i to be complete where the interface is emitted.
	LoadTexture(source string) (handle TextureHandle, width int, height int)
	GenerateTexture(source []byte, dimensions Vector2i) TextureHandle
	ReleaseTexture(texture TextureHandle)
	EnableScissorRegion(enable bool)
	SetScissorRegion(region Rectanglei)

	EnableClipMask(enable bool)
	RenderToClipMask(operation ClipMaskOperation, geometry CompiledGeometryHandle, translation Vector2f)
	SetTransform(transform *Matrix4f)
	PushLayer() LayerHandle
	CompositeLayers(source LayerHandle, destination LayerHandle, blendMode BlendMode, filters []CompiledFilterHandle)
	PopLayer()
	SaveLayerAsTexture() TextureHandle
	SaveLayerAsMaskImage() CompiledFilterHandle
	CompileFilter(name string, parameters map[string]Variant) CompiledFilterHandle
	ReleaseFilter(filter CompiledFilterHandle)
	CompileShader(name string, parameters map[string]Variant) CompiledShaderHandle
	RenderShader(shader CompiledShaderHandle, geometry CompiledGeometryHandle, translation Vector2f, texture TextureHandle)
	ReleaseShader(shader CompiledShaderHandle)
}

// Geometry is Rml::Geometry: a mesh owned by a render manager.
type Geometry struct {
	renderManager *RenderManager
	handle        int // index into the manager's geometry list; -1 when empty
}

// NewGeometry is the default (empty) Geometry.
func NewGeometry() Geometry { return Geometry{handle: -1} }

func (g *Geometry) Valid() bool { return g.renderManager != nil && g.handle >= 0 }

// Render draws the geometry at a rounded translation.
func (g *Geometry) Render(translation Vector2f, texture Texture, shader CompiledShader) {
	if !g.Valid() {
		return
	}
	g.renderManager.render(g, translation.Round(), texture, shader)
}

// RenderPlain is Render(translation) with no texture or shader.
func (g *Geometry) RenderPlain(translation Vector2f) {
	g.Render(translation, NewTexture(), NewCompiledShader())
}

const (
	GeometryReleaseReturnMesh = 0
	GeometryReleaseClearMesh  = 1
)

// Release frees the render resource, returning the mesh.
func (g *Geometry) Release(mode int) Mesh {
	if !g.Valid() {
		return Mesh{}
	}
	mesh := g.renderManager.releaseGeometry(g)
	g.renderManager = nil
	g.handle = -1
	if mode == GeometryReleaseClearMesh {
		return Mesh{}
	}
	return mesh
}

func (g *Geometry) GetMesh() Mesh { return g.renderManager.geometryList[g.handle].mesh }

// Texture is Rml::Texture: a view of a file or callback texture. The
// indices are stored one-based so the zero Texture{} is the invalid
// texture, like the C++ default constructor.
type Texture struct {
	renderManager *RenderManager
	fileSlot      int // file index + 1; 0 when not a file texture
	callbackSlot  int // callback index + 1; 0 when not a callback texture
}

func NewTexture() Texture { return Texture{} }

func (t Texture) Valid() bool { return t.renderManager != nil && (t.callbackSlot > 0 || t.fileSlot > 0) }

func (t Texture) Equals(o Texture) bool {
	return t.renderManager == o.renderManager && t.fileSlot == o.fileSlot && t.callbackSlot == o.callbackSlot
}

func (t Texture) GetDimensions() Vector2i {
	if t.renderManager == nil {
		return Vector2i{}
	}
	if t.fileSlot > 0 {
		return t.renderManager.textureDatabase.fileGetDimensions(t.renderManager.renderInterface, t.fileSlot-1)
	}
	if t.callbackSlot > 0 {
		return t.renderManager.textureDatabase.callbackEnsureLoaded(t.renderManager, t.callbackSlot-1).dimensions
	}
	return Vector2i{}
}

// CallbackTextureFunc is CallbackTextureFunction: generates a texture on
// demand through the given interface.
type CallbackTextureFunc interface {
	GenerateTexture(textureInterface *CallbackTextureInterface) bool
}

// CallbackTexture is Rml::CallbackTexture.
type CallbackTexture struct {
	renderManager *RenderManager
	handle        int
}

func NewCallbackTexture() CallbackTexture { return CallbackTexture{handle: -1} }

func (c *CallbackTexture) Valid() bool { return c.renderManager != nil && c.handle >= 0 }

// Texture is operator Texture().
func (c *CallbackTexture) GetTexture() Texture {
	if !c.Valid() {
		return Texture{}
	}
	return Texture{renderManager: c.renderManager, callbackSlot: c.handle + 1}
}

func (c *CallbackTexture) Release() {
	if c.Valid() {
		c.renderManager.textureDatabase.callbackRelease(c.renderManager.renderInterface, c.handle)
		c.renderManager = nil
		c.handle = -1
	}
}

// CallbackTextureInterface is Rml::CallbackTextureInterface.
type CallbackTextureInterface struct {
	renderManager   *RenderManager
	renderInterface RenderInterface
	entry           *callbackTextureEntry
}

func (c *CallbackTextureInterface) GenerateTexture(source []byte, dimensions Vector2i) bool {
	if c.entry.textureHandle != 0 {
		LogMessage(LogError, "Texture already set")
		return false
	}
	c.entry.textureHandle = c.renderInterface.GenerateTexture(source, dimensions)
	if c.entry.textureHandle != 0 {
		c.entry.dimensions = dimensions
	}
	return c.entry.textureHandle != 0
}

func (c *CallbackTextureInterface) SaveLayerAsTexture() {
	if c.entry.textureHandle != 0 {
		LogMessage(LogError, "Texture already set")
		return
	}
	region := c.renderManager.GetScissorRegion()
	if !region.Valid() {
		LogMessage(LogError, "Save layer as texture requires a scissor region to be set first")
		return
	}
	c.entry.textureHandle = c.renderInterface.SaveLayerAsTexture()
	if c.entry.textureHandle != 0 {
		c.entry.dimensions = region.Size()
	}
}

func (c *CallbackTextureInterface) SetTextureHandle(handle TextureHandle, dimensions Vector2i) {
	if c.entry.textureHandle != 0 {
		LogMessage(LogError, "Texture already set")
		return
	}
	c.entry.textureHandle = handle
	c.entry.dimensions = dimensions
}

func (c *CallbackTextureInterface) GetRenderManager() *RenderManager { return c.renderManager }

// CallbackTextureSource is Rml::CallbackTextureSource: a callback cached
// per render manager.
type CallbackTextureSource struct {
	callback CallbackTextureFunc
	textures map[*RenderManager]*CallbackTexture
}

func NewCallbackTextureSource(callback CallbackTextureFunc) *CallbackTextureSource {
	return &CallbackTextureSource{callback: callback, textures: map[*RenderManager]*CallbackTexture{}}
}

func (s *CallbackTextureSource) GetTexture(renderManager *RenderManager) Texture {
	t, ok := s.textures[renderManager]
	if !ok || !t.Valid() {
		made := renderManager.MakeCallbackTexture(s.callback)
		t = &made
		s.textures[renderManager] = t
	}
	return t.GetTexture()
}

// ReleaseAll releases every cached callback texture.
func (s *CallbackTextureSource) ReleaseAll() {
	textures := s.textures
	for _, t := range textures {
		t.Release()
	}
	s.textures = map[*RenderManager]*CallbackTexture{}
}

// CompiledFilter is Rml::CompiledFilter.
type CompiledFilter struct {
	renderManager *RenderManager
	handle        CompiledFilterHandle
}

func (f *CompiledFilter) Valid() bool { return f.handle != 0 }

// AddHandleTo appends the handle to list when valid.
func (f *CompiledFilter) AddHandleTo(list []CompiledFilterHandle) []CompiledFilterHandle {
	if f.handle != 0 {
		return append(list, f.handle)
	}
	return list
}

func (f *CompiledFilter) Release() {
	if f.handle != 0 {
		f.renderManager.renderInterface.ReleaseFilter(f.handle)
		f.renderManager.compiledFilterCount--
		f.handle = 0
		f.renderManager = nil
	}
}

// CompiledShader is Rml::CompiledShader.
type CompiledShader struct {
	renderManager *RenderManager
	handle        CompiledShaderHandle
}

func NewCompiledShader() CompiledShader { return CompiledShader{} }

func (s *CompiledShader) Valid() bool { return s.handle != 0 }

func (s *CompiledShader) Release() {
	if s.handle != 0 {
		s.renderManager.renderInterface.ReleaseShader(s.handle)
		s.renderManager.compiledShaderCount--
		s.handle = 0
		s.renderManager = nil
	}
}

// ClipMaskGeometry is Rml::ClipMaskGeometry.
type ClipMaskGeometry struct {
	Operation      ClipMaskOperation
	Geometry       *Geometry
	AbsoluteOffset Vector2f
	Transform      *Matrix4f
}

func (a ClipMaskGeometry) Equals(b ClipMaskGeometry) bool {
	return a.Operation == b.Operation && a.Geometry == b.Geometry && a.AbsoluteOffset.Equals(b.AbsoluteOffset) && a.Transform == b.Transform
}

func clipMaskListsEqual(a []ClipMaskGeometry, b []ClipMaskGeometry) bool {
	if len(a) != len(b) {
		return false
	}
	for i := 0; i < len(a); i++ {
		if !a[i].Equals(b[i]) {
			return false
		}
	}
	return true
}

// RenderState is Rml::RenderState.
type RenderState struct {
	ScissorRegion Rectanglei
	ClipMaskList  []ClipMaskGeometry
	Transform     Matrix4f
}

func NewRenderState() RenderState {
	return RenderState{ScissorRegion: RectangleiMakeInvalid(), Transform: Matrix4Identity()}
}

type geometryData struct {
	mesh   Mesh
	handle CompiledGeometryHandle
	live   bool
}

type callbackTextureEntry struct {
	callback      CallbackTextureFunc
	textureHandle TextureHandle
	dimensions    Vector2i
	loadFailed    bool
	live          bool
}

type fileTextureEntry struct {
	textureHandle     TextureHandle
	dimensions        Vector2i
	loadTextureFailed bool
}

// textureDatabase is TextureDatabase: file textures by source path, and
// callback textures in a stable vector.
type textureDatabase struct {
	fileList     []*fileTextureEntry
	fileSources  []string
	fileMap      map[string]int
	callbackList []*callbackTextureEntry
	callbackFree []int
}

func newTextureDatabase() *textureDatabase {
	return &textureDatabase{fileMap: map[string]int{}}
}

func (db *textureDatabase) insertFile(source string) int {
	if index, ok := db.fileMap[source]; ok {
		return index
	}
	index := len(db.fileList)
	db.fileMap[source] = index
	db.fileList = append(db.fileList, &fileTextureEntry{})
	db.fileSources = append(db.fileSources, source)
	return index
}

func (db *textureDatabase) fileEnsureLoaded(ri RenderInterface, index int) *fileTextureEntry {
	entry := db.fileList[index]
	if entry.textureHandle == 0 && !entry.loadTextureFailed {
		source := db.fileSources[index]
		handle, width, height := ri.LoadTexture(source)
		entry.textureHandle = handle
		entry.dimensions = Vector2i{width, height}
		if handle == 0 {
			entry.loadTextureFailed = true
			entry.dimensions = Vector2i{}
			LogMessage(LogWarning, "Could not load texture: "+source)
		}
	}
	return entry
}

func (db *textureDatabase) fileGetHandle(ri RenderInterface, index int) TextureHandle {
	return db.fileEnsureLoaded(ri, index).textureHandle
}

func (db *textureDatabase) fileGetDimensions(ri RenderInterface, index int) Vector2i {
	return db.fileEnsureLoaded(ri, index).dimensions
}

func (db *textureDatabase) fileRelease(ri RenderInterface, source string) bool {
	index, ok := db.fileMap[source]
	if !ok {
		return false
	}
	entry := db.fileList[index]
	if entry.textureHandle != 0 {
		ri.ReleaseTexture(entry.textureHandle)
		db.fileList[index] = &fileTextureEntry{}
		return true
	}
	return false
}

func (db *textureDatabase) fileReleaseAll(ri RenderInterface) {
	for i := 0; i < len(db.fileList); i++ {
		if db.fileList[i].textureHandle != 0 {
			ri.ReleaseTexture(db.fileList[i].textureHandle)
			db.fileList[i] = &fileTextureEntry{}
		}
	}
}

func (db *textureDatabase) callbackCreate(callback CallbackTextureFunc) int {
	entry := &callbackTextureEntry{callback: callback, live: true}
	if len(db.callbackFree) > 0 {
		index := db.callbackFree[len(db.callbackFree)-1]
		db.callbackFree = db.callbackFree[:len(db.callbackFree)-1]
		db.callbackList[index] = entry
		return index
	}
	db.callbackList = append(db.callbackList, entry)
	return len(db.callbackList) - 1
}

func (db *textureDatabase) callbackRelease(ri RenderInterface, index int) {
	entry := db.callbackList[index]
	if entry.textureHandle != 0 {
		ri.ReleaseTexture(entry.textureHandle)
	}
	db.callbackList[index] = &callbackTextureEntry{}
	db.callbackFree = append(db.callbackFree, index)
}

func (db *textureDatabase) callbackEnsureLoaded(rm *RenderManager, index int) *callbackTextureEntry {
	entry := db.callbackList[index]
	if entry.textureHandle == 0 && !entry.loadFailed && entry.callback != nil {
		iface := &CallbackTextureInterface{renderManager: rm, renderInterface: rm.renderInterface, entry: entry}
		if !entry.callback.GenerateTexture(iface) {
			entry.loadFailed = true
			entry.textureHandle = 0
			entry.dimensions = Vector2i{}
		}
	}
	return entry
}

func (db *textureDatabase) callbackCount() int {
	count := 0
	list := db.callbackList
	for _, e := range list {
		if e.live {
			count++
		}
	}
	return count
}

func (db *textureDatabase) callbackReleaseAll(ri RenderInterface) {
	list := db.callbackList
	for _, e := range list {
		if e.textureHandle != 0 {
			ri.ReleaseTexture(e.textureHandle)
			e.textureHandle = 0
			e.dimensions = Vector2i{}
		}
	}
}

// RenderManager is Rml::RenderManager: tracks render state and resources
// over a RenderInterface.
type RenderManager struct {
	renderInterface     RenderInterface
	geometryList        []*geometryData
	geometryFree        []int
	textureDatabase     *textureDatabase
	compiledFilterCount int
	compiledShaderCount int
	state               RenderState
	viewportDimensions  Vector2i
	renderStack         []LayerHandle
}

func NewRenderManager(renderInterface RenderInterface) *RenderManager {
	return &RenderManager{renderInterface: renderInterface, textureDatabase: newTextureDatabase(), state: NewRenderState()}
}

func (rm *RenderManager) GetRenderInterface() RenderInterface { return rm.renderInterface }

// Destroy reports leaked resources and releases every texture.
func (rm *RenderManager) Destroy() {
	liveGeometry := 0
	list := rm.geometryList
	for _, g := range list {
		if g.live {
			liveGeometry++
		}
	}
	if liveGeometry != 0 {
		LogMessage(LogError, "Leaking Geometry detected ("+FormatInt(liveGeometry)+"). Ensure that all RmlUi resources have been released by the end of Rml::Shutdown.")
	}
	if rm.compiledFilterCount != 0 {
		LogMessage(LogError, "Leaking CompiledFilter detected ("+FormatInt(rm.compiledFilterCount)+"). Ensure that all RmlUi resources have been released by the end of Rml::Shutdown.")
	}
	if rm.compiledShaderCount != 0 {
		LogMessage(LogError, "Leaking CompiledShader detected ("+FormatInt(rm.compiledShaderCount)+"). Ensure that all RmlUi resources have been released by the end of Rml::Shutdown.")
	}
	if n := rm.textureDatabase.callbackCount(); n != 0 {
		LogMessage(LogError, "Leaking CallbackTexture detected ("+FormatInt(n)+"). Ensure that all RmlUi resources have been released by the end of Rml::Shutdown.")
	}
	rm.ReleaseAllTextures()
}

func (rm *RenderManager) PrepareRender(dimensions Vector2i) { rm.SetViewport(dimensions) }
func (rm *RenderManager) SetViewport(dimensions Vector2i)   { rm.viewportDimensions = dimensions }
func (rm *RenderManager) GetViewport() Vector2i             { return rm.viewportDimensions }

// MakeGeometry takes ownership of mesh; an empty mesh gives empty geometry.
func (rm *RenderManager) MakeGeometry(mesh Mesh) Geometry {
	if mesh.Empty() {
		return NewGeometry()
	}
	data := &geometryData{mesh: mesh, live: true}
	index := 0
	if len(rm.geometryFree) > 0 {
		index = rm.geometryFree[len(rm.geometryFree)-1]
		rm.geometryFree = rm.geometryFree[:len(rm.geometryFree)-1]
		rm.geometryList[index] = data
	} else {
		rm.geometryList = append(rm.geometryList, data)
		index = len(rm.geometryList) - 1
	}
	return Geometry{renderManager: rm, handle: index}
}

// LoadTexture registers (lazily loads) the texture at source relative to
// documentPath; "?"-prefixed sources are passed through unchanged.
func (rm *RenderManager) LoadTexture(source string, documentPath string) Texture {
	path := ""
	if len(source) > 0 && source[0] == '?' {
		path = source
	} else {
		path = GetSystemInterface().JoinPath(StringReplaceChar(documentPath, '|', ':'), source)
	}
	return Texture{renderManager: rm, fileSlot: rm.textureDatabase.insertFile(path) + 1}
}

func (rm *RenderManager) MakeCallbackTexture(callback CallbackTextureFunc) CallbackTexture {
	return CallbackTexture{renderManager: rm, handle: rm.textureDatabase.callbackCreate(callback)}
}

func (rm *RenderManager) DisableScissorRegion() { rm.SetScissorRegion(RectangleiMakeInvalid()) }

func (rm *RenderManager) SetScissorRegion(region Rectanglei) {
	oldEnable := rm.state.ScissorRegion.Valid()
	newEnable := region.Valid()
	if newEnable != oldEnable {
		rm.renderInterface.EnableScissorRegion(newEnable)
	}
	if newEnable {
		region = region.Intersect(RectangleiFromPositionSize(Vector2i{}, rm.viewportDimensions))
		if !region.Equals(rm.state.ScissorRegion) {
			rm.renderInterface.SetScissorRegion(region)
		}
	}
	rm.state.ScissorRegion = region
}

func (rm *RenderManager) GetScissorRegion() Rectanglei { return rm.state.ScissorRegion }

func (rm *RenderManager) DisableClipMask() {
	if len(rm.state.ClipMaskList) > 0 {
		rm.state.ClipMaskList = nil
		rm.applyClipMask(rm.state.ClipMaskList)
	}
}

// SetClipMaskGeometry is SetClipMask(operation, geometry, translation).
func (rm *RenderManager) SetClipMaskGeometry(operation ClipMaskOperation, geometry *Geometry, translation Vector2f) {
	rm.state.ClipMaskList = []ClipMaskGeometry{ClipMaskGeometry{operation, geometry, translation, nil}}
	rm.applyClipMask(rm.state.ClipMaskList)
}

func (rm *RenderManager) SetClipMask(clipElements []ClipMaskGeometry) {
	if !clipMaskListsEqual(rm.state.ClipMaskList, clipElements) {
		rm.state.ClipMaskList = clipElements
		rm.applyClipMask(rm.state.ClipMaskList)
	}
}

// SetTransform sets the transform; nil means identity.
func (rm *RenderManager) SetTransform(newTransform *Matrix4f) {
	target := Matrix4Identity()
	if newTransform != nil {
		target = *newTransform
	}
	if !rm.state.Transform.Equals(target) {
		rm.renderInterface.SetTransform(newTransform)
		rm.state.Transform = target
	}
}

func (rm *RenderManager) applyClipMask(clipElements []ClipMaskGeometry) {
	enabled := len(clipElements) > 0
	rm.renderInterface.EnableClipMask(enabled)
	if enabled {
		initial := rm.state.Transform
		for _, clip := range clipElements {
			rm.SetTransform(clip.Transform)
			if handle := rm.getCompiledGeometryHandle(clip.Geometry.handle); handle != 0 {
				rm.renderInterface.RenderToClipMask(clip.Operation, handle, clip.AbsoluteOffset)
			}
		}
		rm.SetTransform(&initial)
	}
}

func (rm *RenderManager) GetState() RenderState { return rm.state }

func (rm *RenderManager) SetState(next RenderState) {
	rm.SetScissorRegion(next.ScissorRegion)
	rm.SetClipMask(next.ClipMaskList)
	transform := next.Transform
	rm.SetTransform(&transform)
}

func (rm *RenderManager) ResetState() { rm.SetState(NewRenderState()) }

func (rm *RenderManager) getCompiledGeometryHandle(index int) CompiledGeometryHandle {
	if index < 0 || index >= len(rm.geometryList) {
		return 0
	}
	g := rm.geometryList[index]
	if g.handle == 0 && !g.mesh.Empty() {
		g.handle = rm.renderInterface.CompileGeometry(g.mesh.Vertices, g.mesh.Indices)
		if g.handle == 0 {
			LogMessage(LogError, "Got empty compiled geometry.")
		}
	}
	return g.handle
}

func (rm *RenderManager) render(geometry *Geometry, translation Vector2f, texture Texture, shader CompiledShader) {
	if geometry.renderManager != rm || (shader.Valid() && shader.renderManager != rm) || (texture.Valid() && texture.renderManager != rm) {
		LogMessage(LogError, "Trying to render geometry with resources constructed in different render managers.")
		return
	}
	handle := rm.getCompiledGeometryHandle(geometry.handle)
	if handle == 0 {
		return
	}
	textureHandle := 0
	if texture.fileSlot > 0 {
		textureHandle = rm.textureDatabase.fileGetHandle(rm.renderInterface, texture.fileSlot-1)
	} else if texture.callbackSlot > 0 {
		textureHandle = rm.textureDatabase.callbackEnsureLoaded(rm, texture.callbackSlot-1).textureHandle
	}
	if shader.Valid() {
		rm.renderInterface.RenderShader(shader.handle, handle, translation, textureHandle)
	} else {
		rm.renderInterface.RenderGeometry(handle, translation, textureHandle)
	}
}

// GetTextureSourceList lists every registered file texture source.
func (rm *RenderManager) GetTextureSourceList() []string {
	return append([]string{}, rm.textureDatabase.fileSources...)
}

func (rm *RenderManager) ReleaseTexture(source string) bool {
	return rm.textureDatabase.fileRelease(rm.renderInterface, source)
}

func (rm *RenderManager) ReleaseAllTextures() {
	rm.textureDatabase.callbackReleaseAll(rm.renderInterface)
	rm.textureDatabase.fileReleaseAll(rm.renderInterface)
}

func (rm *RenderManager) ReleaseAllCompiledGeometry() {
	list := rm.geometryList
	for _, g := range list {
		if g.handle != 0 {
			rm.renderInterface.ReleaseGeometry(g.handle)
			g.handle = 0
		}
	}
}

func (rm *RenderManager) releaseGeometry(geometry *Geometry) Mesh {
	data := rm.geometryList[geometry.handle]
	if data.handle != 0 {
		rm.renderInterface.ReleaseGeometry(data.handle)
	}
	mesh := data.mesh
	rm.geometryList[geometry.handle] = &geometryData{}
	rm.geometryFree = append(rm.geometryFree, geometry.handle)
	return mesh
}

func (rm *RenderManager) CompileFilter(name string, parameters map[string]Variant) CompiledFilter {
	if handle := rm.renderInterface.CompileFilter(name, parameters); handle != 0 {
		rm.compiledFilterCount++
		return CompiledFilter{renderManager: rm, handle: handle}
	}
	return CompiledFilter{}
}

func (rm *RenderManager) CompileShader(name string, parameters map[string]Variant) CompiledShader {
	if handle := rm.renderInterface.CompileShader(name, parameters); handle != 0 {
		rm.compiledShaderCount++
		return CompiledShader{renderManager: rm, handle: handle}
	}
	return CompiledShader{}
}

func (rm *RenderManager) PushLayer() LayerHandle {
	layer := rm.renderInterface.PushLayer()
	rm.renderStack = append(rm.renderStack, layer)
	return layer
}

func (rm *RenderManager) CompositeLayers(source LayerHandle, destination LayerHandle, blendMode BlendMode, filters []CompiledFilterHandle) {
	rm.renderInterface.CompositeLayers(source, destination, blendMode, filters)
}

func (rm *RenderManager) PopLayer() {
	rm.renderInterface.PopLayer()
	if len(rm.renderStack) > 0 {
		rm.renderStack = rm.renderStack[:len(rm.renderStack)-1]
	}
}

func (rm *RenderManager) GetTopLayer() LayerHandle {
	if len(rm.renderStack) == 0 {
		return 0
	}
	return rm.renderStack[len(rm.renderStack)-1]
}

func (rm *RenderManager) GetNextLayer() LayerHandle {
	if len(rm.renderStack) < 2 {
		return 0
	}
	return rm.renderStack[len(rm.renderStack)-2]
}

func (rm *RenderManager) SaveLayerAsMaskImage() CompiledFilter {
	if handle := rm.renderInterface.SaveLayerAsMaskImage(); handle != 0 {
		rm.compiledFilterCount++
		return CompiledFilter{renderManager: rm, handle: handle}
	}
	return CompiledFilter{}
}
