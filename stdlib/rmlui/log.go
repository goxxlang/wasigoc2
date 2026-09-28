// Port of RmlUi Source/Core/Log.cpp, LogDefault.cpp, SystemInterface.cpp,
// Clock.cpp, and Include/RmlUi/Core/SystemInterface.h.
package rmlui

import (
	"fmt"
	"os"
)

// LogType is Rml::Log::Type.
type LogType = int

const (
	LogAlways  LogType = 0
	LogError   LogType = 1
	LogAssert  LogType = 2
	LogWarning LogType = 3
	LogInfo    LogType = 4
	LogDebug   LogType = 5
	LogMax     LogType = 6
)

// SystemInterface is Rml::SystemInterface. Embed-by-composition: implement
// the methods you need and forward the rest to a *DefaultSystemInterface.
type SystemInterface interface {
	GetElapsedTime() float64
	TranslateString(input string) (string, int)
	JoinPath(documentPath string, path string) string
	LogMessage(logType LogType, message string) bool
	SetMouseCursor(cursorName string)
	SetClipboardText(text string)
	GetClipboardText() string
	ActivateKeyboard(caretPosition Vector2f, lineHeight float32)
	DeactivateKeyboard()
}

// DefaultSystemInterface is the behavior of the RmlUi SystemInterface base
// class. Time is driven by the host through AdvanceTime, because wasm32 has
// no monotonic clock the library can assume.
type DefaultSystemInterface struct {
	elapsed   float64
	clipboard string
	// Quiet suppresses LogDefault output (stderr), e.g. in goldens.
	Quiet bool
}

func NewDefaultSystemInterface() *DefaultSystemInterface { return &DefaultSystemInterface{} }

func (s *DefaultSystemInterface) AdvanceTime(seconds float64) { s.elapsed += seconds }
func (s *DefaultSystemInterface) SetElapsedTime(t float64)    { s.elapsed = t }
func (s *DefaultSystemInterface) GetElapsedTime() float64     { return s.elapsed }

func (s *DefaultSystemInterface) TranslateString(input string) (string, int) { return input, 0 }

func (s *DefaultSystemInterface) JoinPath(documentPath string, path string) string {
	return DefaultJoinPath(documentPath, path)
}

func (s *DefaultSystemInterface) LogMessage(logType LogType, message string) bool {
	if s.Quiet {
		return true
	}
	return LogDefaultMessage(logType, message)
}

func (s *DefaultSystemInterface) SetMouseCursor(cursorName string)        {}
func (s *DefaultSystemInterface) SetClipboardText(text string)            { s.clipboard = text }
func (s *DefaultSystemInterface) GetClipboardText() string                { return s.clipboard }
func (s *DefaultSystemInterface) ActivateKeyboard(p Vector2f, lh float32) {}
func (s *DefaultSystemInterface) DeactivateKeyboard()                     {}

// DefaultJoinPath is SystemInterface::JoinPath.
func DefaultJoinPath(documentPath string, path string) string {
	if len(path) > 0 && path[0] == '/' {
		return path[1:]
	}
	drivePos := indexByte(path, ':')
	slashPos := indexByte(path, '/')
	back := indexByte(path, '\\')
	if slashPos < 0 || (back >= 0 && back < slashPos) {
		slashPos = back
	}
	if drivePos >= 0 && (slashPos < 0 || drivePos < slashPos) {
		return path
	}
	translated := StringReplaceChar(documentPath, '\\', '/')
	fileStart := lastIndexByte(translated, '/')
	if fileStart >= 0 {
		translated = translated[:fileStart+1]
	} else {
		translated = ""
	}
	url := NewURL(StringReplaceChar(translated, ':', '|') + StringReplaceChar(path, '\\', '/'))
	return StringReplaceChar(url.GetPathedFileName(), '|', ':')
}

func indexByte(s string, c byte) int {
	for i := 0; i < len(s); i++ {
		if s[i] == c {
			return i
		}
	}
	return -1
}

func lastIndexByte(s string, c byte) int {
	for i := len(s) - 1; i >= 0; i-- {
		if s[i] == c {
			return i
		}
	}
	return -1
}

// LogDefaultMessage is LogDefault::LogMessage (the non-Windows branch).
func LogDefaultMessage(logType LogType, message string) bool {
	fmt.Fprintln(os.Stderr, message)
	return true
}

// LogMessage is Log::Message; callers pre-format with fmt.Sprintf.
func LogMessage(logType LogType, message string) {
	if si := GetSystemInterface(); si != nil {
		si.LogMessage(logType, message)
		return
	}
	LogDefaultMessage(logType, message)
}

// LogParseError is Log::ParseError.
func LogParseError(filename string, lineNumber int, message string) {
	if lineNumber >= 0 {
		LogMessage(LogError, filename+":"+FormatInt(lineNumber)+": "+message)
	} else {
		LogMessage(LogError, filename+": "+message)
	}
}

// ClockGetElapsedTime is Clock::GetElapsedTime.
func ClockGetElapsedTime() float64 {
	if si := GetSystemInterface(); si != nil {
		return si.GetElapsedTime()
	}
	return 0
}
