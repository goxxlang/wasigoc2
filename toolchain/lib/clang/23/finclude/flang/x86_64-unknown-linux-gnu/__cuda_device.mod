!mod$ v1 sum:20c95fa4471a62d3
!need$ 1796497ca3749c4e i __cuda_builtins
module __cuda_device
use,intrinsic::__cuda_builtins,only:threadidx
use,intrinsic::__cuda_builtins,only:blockdim
use,intrinsic::__cuda_builtins,only:blockidx
use,intrinsic::__cuda_builtins,only:griddim
use,intrinsic::__cuda_builtins,only:warpsize
end
