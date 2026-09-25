!mod$ v1 sum:1796497ca3749c4e
!need$ d676699be0556005 i __fortran_builtins
module __cuda_builtins
use,intrinsic::__fortran_builtins,only:threadidx=>__builtin_threadidx
use,intrinsic::__fortran_builtins,only:blockdim=>__builtin_blockdim
use,intrinsic::__fortran_builtins,only:blockidx=>__builtin_blockidx
use,intrinsic::__fortran_builtins,only:griddim=>__builtin_griddim
use,intrinsic::__fortran_builtins,only:warpsize=>__builtin_warpsize
end
