// Port of RmlUi Source/Core/URL.cpp and Include/RmlUi/Core/URL.h.
package rmlui

import "strings"

// URL is Rml::URL.
type URL struct {
	url        string
	protocol   string
	login      string
	password   string
	host       string
	port       int
	path       string
	fileName   string
	extension  string
	parameters map[string]string
	urlDirty   bool
}

func NewURL(u string) *URL {
	r := &URL{parameters: map[string]string{}}
	r.SetURL(u)
	return r
}

func (u *URL) SetURL(url string) bool {
	u.urlDirty = false
	u.url = url
	if url == "" {
		u.protocol = ""
		u.login = ""
		u.password = ""
		u.host = ""
		u.port = 0
		u.path = ""
		u.fileName = ""
		u.extension = ""
		return true
	}
	hostBegin := 0
	colon := indexByte(url, ':')
	if colon >= 0 {
		u.protocol = url[:colon]
		if !strings.HasPrefix(url[colon:], "://") {
			LogMessage(LogError, "Malformed protocol identifier found in URL "+url+".")
			return false
		}
		hostBegin = colon + 3
	} else {
		u.protocol = "file"
		hostBegin = 0
	}
	pathBegin := 0
	if hostBegin != 0 {
		rest := url[hostBegin:]
		at := indexByte(rest, '@')
		if at >= 0 {
			loginPassword := rest[:at]
			hostBegin = hostBegin + at + 1
			pw := indexByte(loginPassword, ':')
			if pw >= 0 {
				u.login = loginPassword[:pw]
				u.password = loginPassword[pw+1:]
			} else {
				u.login = loginPassword
			}
			rest = url[hostBegin:]
		}
		slash := indexByte(rest, '/')
		portColon := indexByte(rest, ':')
		if portColon >= 0 && (slash < 0 || portColon < slash) {
			p, ok := ScanInt(rest[portColon+1:])
			if !ok {
				LogMessage(LogError, "Malformed port number found in URL "+url+".")
				return false
			}
			u.port = p
			u.host = rest[:portColon]
			if slash < 0 {
				return true
			}
			pathBegin = hostBegin + slash + 1
		} else {
			u.port = -1
			if slash < 0 {
				u.host = rest
				return true
			}
			u.host = rest[:slash]
			pathBegin = hostBegin + slash + 1
		}
	}
	pathPart := url[pathBegin:]
	q := indexByte(pathPart, '?')
	if q >= 0 {
		params := pathPart[q+1:]
		pathPart = pathPart[:q]
		list := StringExpand(params, '&', false)
		for _, item := range list {
			kv := StringExpand(item, '=', false)
			if len(kv) == 0 {
				continue
			}
			key := URLDecode(kv[0])
			if len(kv) == 2 {
				u.parameters[key] = URLDecode(kv[1])
			} else {
				u.parameters[key] = ""
			}
		}
	}
	fileNameBegin := lastIndexByte(pathPart, '/')
	fileNamePart := pathPart
	if fileNameBegin < 0 {
		u.path = ""
	} else {
		u.path = pathPart[:fileNameBegin+1]
		fileNamePart = pathPart[fileNameBegin+1:]
		for {
			parentDirPos := strings.Index(u.path, "/../")
			if parentDirPos <= 0 {
				break
			}
			startPos := lastIndexByte(u.path[:parentDirPos], '/')
			if startPos < 0 {
				startPos = 0
			} else {
				startPos = startPos + 1
			}
			u.path = u.path[:startPos] + u.path[parentDirPos+4:]
			u.urlDirty = true
		}
	}
	ext := lastIndexByte(fileNamePart, '.')
	if ext < 0 {
		u.fileName = fileNamePart
		u.extension = ""
	} else {
		u.fileName = fileNamePart[:ext]
		u.extension = fileNamePart[ext+1:]
	}
	return true
}

func (u *URL) GetURL() string {
	if u.urlDirty {
		u.constructURL()
	}
	return u.url
}

func (u *URL) SetProtocol(p string) bool { u.protocol = p; u.urlDirty = true; return true }
func (u *URL) GetProtocol() string       { return u.protocol }
func (u *URL) SetLogin(l string) bool    { u.login = l; u.urlDirty = true; return true }
func (u *URL) GetLogin() string          { return u.login }
func (u *URL) SetPassword(p string) bool { u.password = p; u.urlDirty = true; return true }
func (u *URL) GetPassword() string       { return u.password }
func (u *URL) SetHost(h string) bool     { u.host = h; u.urlDirty = true; return true }
func (u *URL) GetHost() string           { return u.host }
func (u *URL) SetPort(p int) bool        { u.port = p; u.urlDirty = true; return true }
func (u *URL) GetPort() int              { return u.port }
func (u *URL) SetPath(p string) bool     { u.path = p; u.urlDirty = true; return true }
func (u *URL) GetPath() string           { return u.path }
func (u *URL) SetFileName(f string) bool { u.fileName = f; u.urlDirty = true; return true }
func (u *URL) GetFileName() string       { return u.fileName }
func (u *URL) SetExtension(e string) bool {
	u.extension = e
	u.urlDirty = true
	return true
}
func (u *URL) GetExtension() string { return u.extension }

func (u *URL) PrefixPath(prefix string) bool {
	if prefix != "" && prefix[len(prefix)-1] != '/' {
		u.path = prefix + "/" + u.path
	} else {
		u.path = prefix + u.path
	}
	u.urlDirty = true
	return true
}

func (u *URL) GetParameters() map[string]string { return u.parameters }
func (u *URL) SetParameter(key string, value string) {
	u.parameters[key] = value
	u.urlDirty = true
}
func (u *URL) ClearParameters() { u.parameters = map[string]string{} }

func (u *URL) GetPathedFileName() string {
	s := u.path + u.fileName
	if u.extension != "" {
		s = s + "." + u.extension
	}
	return s
}

// sortedKeys orders parameters like std::map iteration does.
func sortedKeys(m map[string]string) []string {
	keys := []string{}
	for k := range m {
		keys = append(keys, k)
	}
	for i := 1; i < len(keys); i++ {
		j := i
		for j > 0 && keys[j] < keys[j-1] {
			keys[j], keys[j-1] = keys[j-1], keys[j]
			j--
		}
	}
	return keys
}

func (u *URL) GetQueryString() string {
	var b strings.Builder
	count := 0
	rng1 := sortedKeys(u.parameters)
	for _, k := range rng1 {
		if count > 0 {
			b.WriteString("&")
		}
		b.WriteString(URLEncode(k))
		b.WriteString("=")
		b.WriteString(URLEncode(u.parameters[k]))
		count++
	}
	return b.String()
}

func (u *URL) constructURL() {
	s := ""
	if u.protocol != "" && u.host != "" {
		s = u.protocol + "://"
	}
	if u.login != "" {
		s = s + u.login
		if u.password != "" {
			s = s + ":" + u.password
		}
		s = s + "@"
	}
	s = s + u.host
	if s != "" {
		if u.port > 0 {
			s = s + ":" + FormatInt(u.port) + "/"
		} else {
			s = s + "/"
		}
	}
	s = s + u.path + u.fileName
	if u.extension != "" {
		s = s + "." + u.extension
	}
	if len(u.parameters) > 0 {
		s = s + "?" + u.GetQueryString()
	}
	u.url = s
	u.urlDirty = false
}

// URLEncode is URL::UrlEncode. Bytes >= 0x80 encode as their own %XX; the
// C++ passes a signed char to "%02X" and truncates every one to "%FF".
func URLEncode(value string) string {
	var b strings.Builder
	for i := 0; i < len(value); i++ {
		c := value[i]
		if urlIsUnreservedChar(c) {
			builderWriteByte(&b, c)
		} else {
			builderWriteByte(&b, '%')
			builderWriteByte(&b, "0123456789ABCDEF"[c>>4])
			builderWriteByte(&b, "0123456789ABCDEF"[c&15])
		}
	}
	return b.String()
}

func urlIsUnreservedChar(c byte) bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~'
}

// URLDecode is URL::UrlDecode: '+' is a space; '%' takes the next two
// characters as hex, or copies them through when they are not all hex.
func URLDecode(value string) string {
	var b strings.Builder
	i := 0
	for i < len(value) {
		c := value[i]
		if c == '+' {
			builderWriteByte(&b, ' ')
		} else if c == '%' {
			end := i + 3
			if end > len(value) {
				end = len(value)
			}
			t := value[i+1 : end]
			allHex := true
			n := 0
			for k := 0; k < len(t); k++ {
				if !isHexDigit(t[k]) {
					allHex = false
					break
				}
				n = n*16 + MathHexToDecimal(t[k])
			}
			if allHex {
				builderWriteByte(&b, byte(n))
			} else {
				b.WriteString(t)
			}
			i += 2
		} else {
			builderWriteByte(&b, c)
		}
		i++
	}
	return b.String()
}
