// Port of RmlUi Source/Core/Math.cpp and Include/RmlUi/Core/Math.h.
//
// The stdlib math here truncates through int64 (undefined for FLT_MAX,
// which RmlUi uses as "none" for max-width/max-height) and has no
// trigonometry, so this file carries its own guarded float math.
package rmlui

const Pi float32 = 3.14159265358979323846

const FltMax float32 = 3.40282347e+38

const fZero float32 = 0.0001

const pi64 = 3.14159265358979323846264338327950288

// Largest float64 with a fractional part; anything at or above is integral.
const twoPow52 = 4503599627370496.0

func floor64(x float64) float64 {
	if x != x || x >= twoPow52 || x <= -twoPow52 {
		return x
	}
	i := int64(x)
	f := float64(i)
	if f > x {
		f = f - 1
	}
	return f
}

func ceil64(x float64) float64 {
	if x != x || x >= twoPow52 || x <= -twoPow52 {
		return x
	}
	i := int64(x)
	f := float64(i)
	if f < x {
		f = f + 1
	}
	return f
}

func trunc64(x float64) float64 {
	if x != x || x >= twoPow52 || x <= -twoPow52 {
		return x
	}
	return float64(int64(x))
}

func fmod64(x float64, y float64) float64 {
	if y == 0 {
		return 0
	}
	q := x / y
	return x - trunc64(q)*y
}

func abs64(x float64) float64 {
	if x < 0 {
		return -x
	}
	return x
}

func sqrt64(x float64) float64 {
	if x <= 0 {
		return 0
	}
	if x != x || x > 1.7976931348623157e+308 {
		return x
	}
	// Scale into [0.25, 4) so Newton starts close and converges in a few steps.
	scale := 1.0
	for x >= 4 {
		x = x / 4
		scale = scale * 2
	}
	for x < 0.25 {
		x = x * 4
		scale = scale / 2
	}
	z := (x + 1) / 2
	for i := 0; i < 8; i++ {
		z = (z + x/z) / 2
	}
	return z * scale
}

// sinCosReduced evaluates sin and cos for |x| <= pi/4.
func sinReduced(x float64) float64 {
	x2 := x * x
	return x * (1 - x2/6*(1-x2/20*(1-x2/42*(1-x2/72*(1-x2/110*(1-x2/156))))))
}

func cosReduced(x float64) float64 {
	x2 := x * x
	return 1 - x2/2*(1-x2/12*(1-x2/30*(1-x2/56*(1-x2/90*(1-x2/132)))))
}

func sin64(x float64) float64 {
	if x != x {
		return x
	}
	// Reduce to [0, 2pi), then to an octant.
	x = fmod64(x, 2*pi64)
	if x < 0 {
		x = x + 2*pi64
	}
	sign := 1.0
	if x >= pi64 {
		x = x - pi64
		sign = -1.0
	}
	if x > pi64/2 {
		x = pi64 - x
	}
	if x <= pi64/4 {
		return sign * sinReduced(x)
	}
	return sign * cosReduced(pi64/2-x)
}

func cos64(x float64) float64 {
	return sin64(x + pi64/2)
}

func atanReduced(x float64) float64 {
	// |x| <= tan(pi/12) after the reductions in atan64: series converges fast.
	x2 := x * x
	sum := 0.0
	term := x
	for n := 0; n < 12; n++ {
		sum = sum + term/float64(2*n+1)
		term = -term * x2
	}
	return sum
}

func atan64(x float64) float64 {
	if x != x {
		return x
	}
	sign := 1.0
	if x < 0 {
		x = -x
		sign = -1.0
	}
	invert := false
	if x > 1 {
		x = 1 / x
		invert = true
	}
	// atan(x) = 2*atan(x / (1 + sqrt(1 + x^2))), applied twice.
	x = x / (1 + sqrt64(1+x*x))
	x = x / (1 + sqrt64(1+x*x))
	r := 4.0 * atanReduced(x)
	if invert {
		r = pi64/2 - r
	}
	return sign * r
}

func atan264(y float64, x float64) float64 {
	if x > 0 {
		return atan64(y / x)
	}
	if x < 0 {
		if y >= 0 {
			return atan64(y/x) + pi64
		}
		return atan64(y/x) - pi64
	}
	if y > 0 {
		return pi64 / 2
	}
	if y < 0 {
		return -pi64 / 2
	}
	return 0
}

func exp64(x float64) float64 {
	if x == 0 {
		return 1
	}
	if x > 709 {
		return 1.7976931348623157e+308
	}
	if x < -745 {
		return 0
	}
	neg := x < 0
	if neg {
		x = -x
	}
	halvings := 0
	for x > 0.5 {
		x = x / 2
		halvings++
	}
	sum := 1.0
	term := 1.0
	for n := 1; n < 20; n++ {
		term = term * x / float64(n)
		sum = sum + term
	}
	for i := 0; i < halvings; i++ {
		sum = sum * sum
	}
	if neg {
		return 1 / sum
	}
	return sum
}

func log64(x float64) float64 {
	if x <= 0 {
		return -1.7976931348623157e+308
	}
	// Scale into [1, 2) keeping count of powers of two.
	k := 0
	for x >= 2 {
		x = x / 2
		k++
	}
	for x < 1 {
		x = x * 2
		k--
	}
	// ln(x) = 2*atanh((x-1)/(x+1))
	t := (x - 1) / (x + 1)
	t2 := t * t
	sum := 0.0
	term := t
	for n := 0; n < 30; n++ {
		sum = sum + term/float64(2*n+1)
		term = term * t2
	}
	return 2*sum + float64(k)*0.69314718055994530942
}

func pow64(x float64, y float64) float64 {
	if y == 0 {
		return 1
	}
	if x == 0 {
		return 0
	}
	if y == trunc64(y) && abs64(y) < 64 {
		n := int(abs64(y))
		r := 1.0
		for i := 0; i < n; i++ {
			r = r * x
		}
		if y < 0 {
			return 1 / r
		}
		return r
	}
	if x < 0 {
		return 0
	}
	return exp64(y * log64(x))
}

// Math namespace (Rml::Math).

func MathIsCloseToZero(value float32) bool { return MathAbsolute(value) < fZero }

func MathAbsolute(value float32) float32 {
	if value < 0 {
		return -value
	}
	return value
}

func MathAbsoluteInt(value int) int {
	if value < 0 {
		return -value
	}
	return value
}

func MathAbsoluteVector(value Vector2f) Vector2f {
	return Vector2f{MathAbsolute(value.X), MathAbsolute(value.Y)}
}

func MathCos(angle float32) float32       { return float32(cos64(float64(angle))) }
func MathSin(angle float32) float32       { return float32(sin64(float64(angle))) }
func MathTan(angle float32) float32       { return float32(sin64(float64(angle)) / cos64(float64(angle))) }
func MathATan2(y float32, x float32) float32 { return float32(atan264(float64(y), float64(x))) }
func MathExp(value float32) float32       { return float32(exp64(float64(value))) }

func MathACos(value float32) float32 {
	v := float64(value)
	return float32(atan264(sqrt64(1-v*v), v))
}

func MathASin(value float32) float32 {
	v := float64(value)
	return float32(atan264(v, sqrt64(1-v*v)))
}

func MathLog2(value int) int {
	result := 0
	for value > 1 {
		value = value >> 1
		result++
	}
	return result
}

func MathRadiansToDegrees(angle float32) float32 { return angle * (180.0 / Pi) }
func MathDegreesToRadians(angle float32) float32 { return angle * (Pi / 180.0) }

func MathNormaliseAngle(angle float32) float32 {
	result := float32(fmod64(float64(angle), float64(Pi*2.0)))
	if result < 0 {
		result += Pi * 2.0
	}
	return result
}

func MathSquareRoot(value float32) float32 { return float32(sqrt64(float64(value))) }

func MathRound(value float32) float32 { return float32(floor64(float64(value) + 0.5)) }

func MathRoundDouble(value float64) float64 { return floor64(value + 0.5) }

func MathRoundUp(value float32) float32 { return float32(ceil64(float64(value))) }

func MathRoundDown(value float32) float32 { return float32(floor64(float64(value))) }

func MathRoundToInteger(value float32) int {
	if value > 0 {
		return int(value + 0.5)
	}
	return int(value - 0.5)
}

func MathRoundUpToInteger(value float32) int   { return int(ceil64(float64(value))) }
func MathRoundDownToInteger(value float32) int { return int(floor64(float64(value))) }

// MathDecomposeFractionalIntegral returns (fractional, integral), like modf.
func MathDecomposeFractionalIntegral(value float32) (float32, float32) {
	integral := float32(trunc64(float64(value)))
	return value - integral, integral
}

// MathSnapToPixelGrid rounds an offset/width pair so the right edge lands on
// a pixel boundary as well as the left.
func MathSnapToPixelGrid(offset float32, width float32) (float32, float32) {
	rightEdge := offset + width
	offset = MathRound(offset)
	width = MathRound(rightEdge) - offset
	return offset, width
}

func MathSnapToPixelGridVector(position Vector2f, size Vector2f) (Vector2f, Vector2f) {
	bottomRight := position.Add(size)
	position = position.Round()
	size = bottomRight.Round().Sub(position)
	return position, size
}

func MathSnapToPixelGridRect(rectangle Rectanglef) Rectanglef {
	return RectanglefFromCorners(rectangle.TopLeft().Round(), rectangle.BottomRight().Round())
}

func MathExpandToPixelGrid(position Vector2f, size Vector2f) (Vector2f, Vector2f) {
	bottomRight := position.Add(size)
	position = Vector2f{MathRoundDown(position.X), MathRoundDown(position.Y)}
	size = Vector2f{MathRoundUp(bottomRight.X), MathRoundUp(bottomRight.Y)}.Sub(position)
	return position, size
}

func MathExpandToPixelGridRect(rectangle Rectanglef) Rectanglef {
	topLeft := Vector2f{MathRoundDown(rectangle.Left()), MathRoundDown(rectangle.Top())}
	bottomRight := Vector2f{MathRoundUp(rectangle.Right()), MathRoundUp(rectangle.Bottom())}
	return RectanglefFromCorners(topLeft, bottomRight)
}

func MathToPowerOfTwo(number int) int {
	if (number & (number - 1)) == 0 {
		return number
	}
	for i := 31; i >= 0; i-- {
		if (number & (1 << i)) != 0 {
			if i == 31 {
				return 1 << 31
			}
			return 1 << (i + 1)
		}
	}
	return 0
}

func MathHexToDecimal(hexDigit byte) int {
	if hexDigit >= '0' && hexDigit <= '9' {
		return int(hexDigit - '0')
	}
	if hexDigit >= 'a' && hexDigit <= 'f' {
		return 10 + int(hexDigit-'a')
	}
	if hexDigit >= 'A' && hexDigit <= 'F' {
		return 10 + int(hexDigit-'A')
	}
	return -1
}

// RmlUi seeds std::rand from nothing; a fixed LCG keeps output deterministic.
var mathRandState uint32 = 1

func mathRand() int {
	mathRandState = mathRandState*1103515245 + 12345
	return int((mathRandState >> 16) & 0x7fff)
}

func MathRandomReal(maxValue float32) float32 { return float32(mathRand()) / 32767.0 * maxValue }
func MathRandomInteger(maxValue int) int      { return mathRand() % maxValue }
func MathRandomBool() bool                    { return MathRandomInteger(2) == 1 }

func MathMax(a float32, b float32) float32 {
	if a > b {
		return a
	}
	return b
}

func MathMin(a float32, b float32) float32 {
	if a < b {
		return a
	}
	return b
}

func MathMaxInt(a int, b int) int {
	if a > b {
		return a
	}
	return b
}

func MathMinInt(a int, b int) int {
	if a < b {
		return a
	}
	return b
}

func MathClamp(value float32, min float32, max float32) float32 {
	if value < min {
		return min
	}
	if value > max {
		return max
	}
	return value
}

func MathClampInt(value int, min int, max int) int {
	if value < min {
		return min
	}
	if value > max {
		return max
	}
	return value
}

func MathLerp(t float32, v0 float32, v1 float32) float32 { return v0*(1.0-t) + v1*t }

func MathMaxVector(a Vector2f, b Vector2f) Vector2f {
	return Vector2f{MathMax(a.X, b.X), MathMax(a.Y, b.Y)}
}

func MathMinVector(a Vector2f, b Vector2f) Vector2f {
	return Vector2f{MathMin(a.X, b.X), MathMin(a.Y, b.Y)}
}

func MathClampVector(value Vector2f, min Vector2f, max Vector2f) Vector2f {
	return Vector2f{MathClamp(value.X, min.X, max.X), MathClamp(value.Y, min.Y, max.Y)}
}

func MathRoundedLerp(t float32, v0 ColourbPremultiplied, v1 ColourbPremultiplied) ColourbPremultiplied {
	return ColourbPremultiplied{
		byte(MathRoundToInteger(MathLerp(t, float32(v0.Red), float32(v1.Red)))),
		byte(MathRoundToInteger(MathLerp(t, float32(v0.Green), float32(v1.Green)))),
		byte(MathRoundToInteger(MathLerp(t, float32(v0.Blue), float32(v1.Blue)))),
		byte(MathRoundToInteger(MathLerp(t, float32(v0.Alpha), float32(v1.Alpha)))),
	}
}
