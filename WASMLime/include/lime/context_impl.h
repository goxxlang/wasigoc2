#ifndef LIME_CONTEXT_IMPL_H_
#define LIME_CONTEXT_IMPL_H_

#include "context_gen.h"

#include "content/renderer_impl.h"

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#include "wpr/store.h"

#include <memory>
#include <string>
#include <vector>

namespace lime {

class FrameImpl;

// One Context = one wpr::Process entry (see ContextProviderImpl) plus one
// real content::Renderer -- WASMRenderer's Mojo-shaped bootstrap for a
// renderer-side frame -- bound in-process to a content::RendererImpl. Each
// Frame this Context creates goes through a real Renderer.CreateFrame()
// call (not a bare C++ constructor), the same way a real browser process
// asks its renderer process for a new frame, and is attached as a task
// (tab) on the Context's process entry via task_manager_->AttachTask.
class ContextImpl : public Context {
 public:
  ContextImpl(wpr::TaskManager* task_manager, int process_id,
             std::string renderer_host, int renderer_port);
  ~ContextImpl() override;

  void CreateFrame(mojo::PendingAssociatedReceiver<Frame> frame) override;
  void CreateFrameWithParams(const CreateFrameParams& params,
                             mojo::PendingAssociatedReceiver<Frame> frame) override;

  int process_id() const { return process_id_; }

 private:
  void CreateFrameInternal(const std::string& debug_name,
                           mojo::PendingAssociatedReceiver<Frame> frame);

  wpr::TaskManager* task_manager_;
  int process_id_;
  int next_tab_id_ = 1;

  content::RendererImpl renderer_impl_;
  std::unique_ptr<mojo::Receiver<content::Renderer>> renderer_receiver_;
  mojo::Remote<content::Renderer> renderer_remote_;

  std::vector<std::unique_ptr<FrameImpl>> frames_;
  std::vector<std::unique_ptr<mojo::AssociatedReceiver<Frame>>> frame_receivers_;
};

}  // namespace lime

#endif  // LIME_CONTEXT_IMPL_H_
