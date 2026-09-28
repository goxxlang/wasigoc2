// Port of RmlUi Include/RmlUi/Core/FontEffect.h and FontEffectInstancer.h.
package rmlui

// Font effect layers (FontEffect::Layer).
const (
	FontEffectLayerBack = iota
	FontEffectLayerFront
)

// FontEffect is Rml::FontEffect.
type FontEffect interface {
	GetLayer() int
	SetFingerprint(fingerprint string)
	GetFingerprint() string
}

// FontEffectInstancer is Rml::FontEffectInstancer.
type FontEffectInstancer interface {
	GetPropertySpecification() *PropertySpecification
	InstanceFontEffect(name string, properties *PropertyDictionary) FontEffect
}
