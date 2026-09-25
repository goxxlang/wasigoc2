!mod$ v1 sum:bed7361c8ab90dcc
!need$ 1796497ca3749c4e i __cuda_builtins
module cuda_runtime_api
use,intrinsic::__cuda_builtins,only:threadidx
use,intrinsic::__cuda_builtins,only:blockdim
use,intrinsic::__cuda_builtins,only:blockidx
use,intrinsic::__cuda_builtins,only:griddim
use,intrinsic::__cuda_builtins,only:warpsize
integer(4),parameter::cuda_stream_kind=8_4
intrinsic::int_ptr_kind
interface
function cudagetstreamdefaultarg(devptr)
integer(4),device::devptr(1_8:*)
!dir$ ignore_tkr(tkr) devptr
integer(8)::cudagetstreamdefaultarg
end
end interface
interface
function cudagetstreamdefaultnull()
integer(8)::cudagetstreamdefaultnull
end
end interface
interface
function cudasetstreamdefault(stream)
integer(8),value::stream
!dir$ ignore_tkr(k) stream
integer(4)::cudasetstreamdefault
end
end interface
interface
function cudasetstreamarray(devptr,stream)
integer(4),device::devptr(1_8:*)
!dir$ ignore_tkr(tkr) devptr
integer(8),value::stream
!dir$ ignore_tkr(k) stream
integer(4)::cudasetstreamarray
end
end interface
interface
function cudastreamdestroy(stream)
integer(8),value::stream
!dir$ ignore_tkr(k) stream
integer(4)::cudastreamdestroy
end
end interface
interface cudaforgetdefaultstream
procedure::cudagetstreamdefaultarg
procedure::cudagetstreamdefaultnull
end interface
interface cudaforsetdefaultstream
procedure::cudasetstreamdefault
procedure::cudasetstreamarray
end interface
interface cudastreamdestroy
procedure::cudastreamdestroy
end interface
end
