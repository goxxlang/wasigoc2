// The `webgpu` gocvm catalog: compute on the host's WebGPU, named by ids.
//
// A Go++ guest reaches it as gocvm.Call("webgpu", api + "\x1f" + rest)
// (stdlib/webgpu). The device, its buffers, shader modules, pipelines and
// bind groups live in this module and are named by small integer ids; the
// GPU work itself runs on the host's navigator.gpu through remote.cc.
//
//   Open                       adapter + device; `key=value` lines
//   Info                       the same lines for the open device
//   Buffer   <size>            a storage|uniform|copy buffer; its id
//   Free     <id>              drop a buffer, shader, pipeline or group
//   Write    <id> <off>\n<bytes>
//   Read     <id> <off> <len>  the bytes
//   Shader   <wgsl>            a shader module; its id (compile errors fail)
//   Pipeline <shader> <entry> [name=value ...]   a compute pipeline; its id
//   Group    <pipeline> <index> <binding>=<buf>[:<off>[:<size>]] ...
//   Run      <program>         binary; see catalog.cc RunProgram
//
// A reply that begins "error:" is an error (gocvm.Call returns it as one).
#pragma once

#include <string>

namespace wasmwebgpu {

std::string webgpu_call(const std::string& api, const std::string& rest);

}  // namespace wasmwebgpu
