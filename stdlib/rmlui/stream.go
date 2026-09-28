// Port of RmlUi Source/Core/Stream.cpp, StreamMemory.cpp, StreamFile.cpp,
// FileInterface.cpp, FileInterfaceDefault.cpp and their headers.
//
// Every RmlUi stream in the core is read front to back, so a single
// in-memory Stream serves as StreamMemory and StreamFile (which loads the
// whole file through the FileInterface when opened).
package rmlui

import "os"

const (
	StreamModeWrite  = 1 << 0
	StreamModeAppend = 1 << 1
	StreamModeRead   = 1 << 2
	StreamModeAsync  = 1 << 3
	StreamModeMask   = StreamModeWrite | StreamModeAppend | StreamModeRead
)

const (
	SeekSet = 0
	SeekCur = 1
	SeekEnd = 2
)

// Stream is Rml::Stream, implemented as Rml::StreamMemory.
type Stream struct {
	data       []byte
	pos        int
	url        *URL
	streamMode int
}

// NewStreamMemory is StreamMemory(buffer, size): a read-only view of data.
func NewStreamMemory(data string) *Stream {
	s := &Stream{data: []byte(data), url: NewURL("")}
	s.streamMode = StreamModeRead
	return s
}

// NewStreamMemoryWritable is StreamMemory(initial_size).
func NewStreamMemoryWritable() *Stream {
	return &Stream{url: NewURL(""), streamMode: StreamModeRead | StreamModeWrite}
}

// OpenStreamFile is StreamFile::Open: loads path through the FileInterface.
func OpenStreamFile(path string) (*Stream, bool) {
	fi := GetFileInterface()
	contents, ok := fi.LoadFile(path)
	if !ok {
		LogMessage(LogWarning, "Unable to open file "+path+".")
		return nil, false
	}
	s := &Stream{data: []byte(contents), url: NewURL(path), streamMode: StreamModeRead}
	return s, true
}

func (s *Stream) Close()             { s.streamMode = 0 }
func (s *Stream) GetStreamMode() int { return s.streamMode }
func (s *Stream) GetSourceURL() *URL { return s.url }

// SetSourceURL is StreamMemory::SetSourceURL.
func (s *Stream) SetSourceURL(url string) { s.url = NewURL(url) }

func (s *Stream) IsEOS() bool   { return s.pos >= len(s.data) }
func (s *Stream) Length() int   { return len(s.data) }
func (s *Stream) Tell() int     { return s.pos }
func (s *Stream) IsReadReady() bool  { return true }
func (s *Stream) IsWriteReady() bool { return s.streamMode&StreamModeWrite != 0 }

func (s *Stream) Seek(offset int, origin int) bool {
	np := 0
	switch origin {
	case SeekSet:
		np = offset
	case SeekCur:
		np = s.pos + offset
	case SeekEnd:
		np = len(s.data) + offset
	default:
		return false
	}
	if np < 0 || np > len(s.data) {
		return false
	}
	s.pos = np
	return true
}

// ReadString is Stream::Read(String&, bytes): appends up to n bytes.
func (s *Stream) ReadString(n int) string {
	end := s.pos + n
	if end > len(s.data) {
		end = len(s.data)
	}
	out := string(s.data[s.pos:end])
	s.pos = end
	return out
}

// ReadAll returns the unread remainder.
func (s *Stream) ReadAll() string { return s.ReadString(len(s.data) - s.pos) }

// Peek is Stream::Peek.
func (s *Stream) Peek(n int) string {
	end := s.pos + n
	if end > len(s.data) {
		end = len(s.data)
	}
	return string(s.data[s.pos:end])
}

// ReadByte reads one byte; ok is false at end of stream.
func (s *Stream) ReadByte() (byte, bool) {
	if s.pos >= len(s.data) {
		return 0, false
	}
	b := s.data[s.pos]
	s.pos++
	return b, true
}

// Write appends at the write position (StreamMemory::Write).
func (s *Stream) Write(str string) int {
	b := []byte(str)
	for i := 0; i < len(b); i++ {
		if s.pos < len(s.data) {
			s.data[s.pos] = b[i]
		} else {
			s.data = append(s.data, b[i])
		}
		s.pos++
	}
	return len(b)
}

// Truncate keeps the first n bytes.
func (s *Stream) Truncate(n int) int {
	if n < len(s.data) {
		s.data = s.data[:n]
	}
	if s.pos > len(s.data) {
		s.pos = len(s.data)
	}
	return len(s.data)
}

func (s *Stream) PushBack(str string) int {
	s.data = append(s.data, []byte(str)...)
	return len(str)
}

func (s *Stream) PopBack(n int) int { return s.Truncate(len(s.data) - n) }

// PushFront is StreamMemory::PushFront.
func (s *Stream) PushFront(str string) int {
	s.data = append([]byte(str), s.data...)
	return len(str)
}

// PopFront is StreamMemory::PopFront.
func (s *Stream) PopFront(n int) int {
	if n > len(s.data) {
		n = len(s.data)
	}
	s.data = s.data[n:]
	s.pos = s.pos - n
	if s.pos < 0 {
		s.pos = 0
	}
	return n
}

// Contents is StreamMemory::RawStream as a string.
func (s *Stream) Contents() string { return string(s.data) }

// ---- FileInterface ----

// FileInterface is Rml::FileInterface. The handle-based C++ API collapses to
// LoadFile, which is how every core reader uses it.
type FileInterface interface {
	LoadFile(path string) (string, bool)
}

// FileInterfaceDefault is Rml::FileInterfaceDefault: the host filesystem.
type FileInterfaceDefault struct{}

func (f *FileInterfaceDefault) LoadFile(path string) (string, bool) {
	data, err := os.ReadFile(path)
	if err != nil {
		return "", false
	}
	return string(data), true
}

// MemoryFileInterface serves files from an in-memory table, the usual
// setup for a wasm guest whose documents are bundled or pushed by the host.
type MemoryFileInterface struct {
	files map[string]string
	// Fallback, when set, is consulted for paths not in the table.
	Fallback FileInterface
}

func NewMemoryFileInterface() *MemoryFileInterface {
	return &MemoryFileInterface{files: map[string]string{}}
}

func (f *MemoryFileInterface) AddFile(path string, contents string) { f.files[path] = contents }

func (f *MemoryFileInterface) RemoveFile(path string) { delete(f.files, path) }

func (f *MemoryFileInterface) LoadFile(path string) (string, bool) {
	if c, ok := f.files[path]; ok {
		return c, true
	}
	if f.Fallback != nil {
		return f.Fallback.LoadFile(path)
	}
	return "", false
}
