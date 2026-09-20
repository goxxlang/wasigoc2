#include "lime/context_provider_main.h"

#include <utility>

namespace lime {

ContextProviderHandle ContextProviderMain(std::string renderer_host, int renderer_port) {
  ContextProviderHandle handle;
  handle.impl = std::make_unique<ContextProviderImpl>(std::move(renderer_host), renderer_port);
  handle.receiver = std::make_unique<mojo::Receiver<ContextProvider>>(handle.impl.get());
  handle.receiver->Bind(handle.remote.BindNewPipeAndPassReceiver());
  return handle;
}

}  // namespace lime
