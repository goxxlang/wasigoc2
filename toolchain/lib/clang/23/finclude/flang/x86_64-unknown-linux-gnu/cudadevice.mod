!mod$ v1 sum:26460f9fbadc8c7a
!need$ 20c95fa4471a62d3 i __cuda_device
!need$ d676699be0556005 i __fortran_builtins
!need$ 1796497ca3749c4e i __cuda_builtins
module cudadevice
use,intrinsic::__fortran_builtins,only:dim3=>__builtin_dim3
use,intrinsic::__fortran_builtins,only:c_devptr=>__builtin_c_devptr
use,intrinsic::__fortran_builtins,only:c_devloc=>__builtin_c_devloc
use,intrinsic::__cuda_device,only:threadidx
use,intrinsic::__cuda_device,only:blockdim
use,intrinsic::__cuda_device,only:blockidx
use,intrinsic::__cuda_device,only:griddim
use,intrinsic::__cuda_device,only:warpsize
interface
attributes(device) function syncthreads_and_i4(value)
integer(4),value::value
integer(4)::syncthreads_and_i4
end
end interface
interface
attributes(device) function syncthreads_and_l4(value)
logical(4),value::value
integer(4)::syncthreads_and_l4
end
end interface
interface
attributes(device) function syncthreads_count_i4(value)
integer(4),value::value
integer(4)::syncthreads_count_i4
end
end interface
interface
attributes(device) function syncthreads_count_l4(value)
logical(4),value::value
integer(4)::syncthreads_count_l4
end
end interface
interface
attributes(device) function syncthreads_or_i4(value)
integer(4),value::value
integer(4)::syncthreads_or_i4
end
end interface
interface
attributes(device) function syncthreads_or_l4(value)
logical(4),value::value
integer(4)::syncthreads_or_l4
end
end interface
interface
attributes(device) subroutine syncwarp(mask)
integer(4),value::mask
end
end interface
interface
attributes(device) subroutine threadfence()
end
end interface
interface
attributes(device) subroutine threadfence_block()
end
end interface
interface
attributes(device) subroutine threadfence_system()
end
end interface
interface
attributes(device) function __fadd_rn(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fadd_rn
end
end interface
interface
attributes(device) function __fadd_rz(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fadd_rz
end
end interface
interface
attributes(device) function __fadd_rd(x,y) bind(c)
real(4),intent(in),value::x
real(4),intent(in),value::y
real(4)::__fadd_rd
end
end interface
interface
attributes(device) function __fadd_ru(x,y) bind(c)
real(4),intent(in),value::x
real(4),intent(in),value::y
real(4)::__fadd_ru
end
end interface
interface
attributes(device) function __fmul_rn(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fmul_rn
end
end interface
interface
attributes(device) function __fmul_rz(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fmul_rz
end
end interface
interface
attributes(device) function __fmul_ru(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fmul_ru
end
end interface
interface
attributes(device) function __fmul_rd(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fmul_rd
end
end interface
interface
attributes(device) function __fmaf_rn(a,b,c) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4),value::c
!dir$ ignore_tkr(d) c
real(4)::__fmaf_rn
end
end interface
interface
attributes(device) function __fmaf_rz(a,b,c) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4),value::c
!dir$ ignore_tkr(d) c
real(4)::__fmaf_rz
end
end interface
interface
attributes(device) function __fmaf_ru(a,b,c) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4),value::c
!dir$ ignore_tkr(d) c
real(4)::__fmaf_ru
end
end interface
interface
attributes(device) function __fmaf_rd(a,b,c) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4),value::c
!dir$ ignore_tkr(d) c
real(4)::__fmaf_rd
end
end interface
interface
attributes(device) function __frcp_rn(a) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4)::__frcp_rn
end
end interface
interface
attributes(device) function __frcp_rz(a) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4)::__frcp_rz
end
end interface
interface
attributes(device) function __frcp_ru(a) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4)::__frcp_ru
end
end interface
interface
attributes(device) function __frcp_rd(a) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4)::__frcp_rd
end
end interface
interface
attributes(device) function __fsqrt_rn(a) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4)::__fsqrt_rn
end
end interface
interface
attributes(device) function __fsqrt_rz(a) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4)::__fsqrt_rz
end
end interface
interface
attributes(device) function __fsqrt_ru(a) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4)::__fsqrt_ru
end
end interface
interface
attributes(device) function __fsqrt_rd(a) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4)::__fsqrt_rd
end
end interface
interface
attributes(device) function __fdiv_rn(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fdiv_rn
end
end interface
interface
attributes(device) function __fdiv_rz(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fdiv_rz
end
end interface
interface
attributes(device) function __fdiv_ru(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fdiv_ru
end
end interface
interface
attributes(device) function __fdiv_rd(a,b) bind(c)
real(4),value::a
!dir$ ignore_tkr(d) a
real(4),value::b
!dir$ ignore_tkr(d) b
real(4)::__fdiv_rd
end
end interface
interface
attributes(device) function __dadd_rn(a,b) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8)::__dadd_rn
end
end interface
interface
attributes(device) function __dadd_rz(a,b) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8)::__dadd_rz
end
end interface
interface
attributes(device) function __dadd_ru(a,b) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8)::__dadd_ru
end
end interface
interface
attributes(device) function __dadd_rd(a,b) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8)::__dadd_rd
end
end interface
interface
attributes(device) function __dmul_rn(a,b) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8)::__dmul_rn
end
end interface
interface
attributes(device) function __dmul_rz(a,b) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8)::__dmul_rz
end
end interface
interface
attributes(device) function __dmul_ru(a,b) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8)::__dmul_ru
end
end interface
interface
attributes(device) function __dmul_rd(a,b) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8)::__dmul_rd
end
end interface
interface
attributes(device) function __fma_rn(a,b,c) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8),value::c
real(8)::__fma_rn
end
end interface
interface
attributes(device) function __fma_rz(a,b,c) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8),value::c
real(8)::__fma_rz
end
end interface
interface
attributes(device) function __fma_ru(a,b,c) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8),value::c
real(8)::__fma_ru
end
end interface
interface
attributes(device) function __fma_rd(a,b,c) bind(c)
real(8),value::a
!dir$ ignore_tkr(d) a
real(8),value::b
!dir$ ignore_tkr(d) b
real(8),value::c
real(8)::__fma_rd
end
end interface
interface
attributes(device) function rsqrtf(x) bind(c,name="__nv_rsqrtf")
real(4),value::x
real(4)::rsqrtf
end
end interface
interface
attributes(device) function rsqrt(x) bind(c,name="__nv_rsqrt")
real(8),value::x
real(8)::rsqrt
end
end interface
interface
attributes(device) function __saturatef(r) bind(c,name="__nv_saturatef")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4)::__saturatef
end
end interface
interface
attributes(device) function __sad(i,j,k) bind(c,name="__nv_sad")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4),value::j
!dir$ ignore_tkr(d) j
integer(4),value::k
!dir$ ignore_tkr(d) k
integer(4)::__sad
end
end interface
interface
attributes(device) function __usad(i,j,k) bind(c,name="__nv_usad")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4),value::j
!dir$ ignore_tkr(d) j
integer(4),value::k
!dir$ ignore_tkr(d) k
integer(4)::__usad
end
end interface
interface
attributes(device) function signbitf(x) bind(c,name="__nv_signbitf")
real(4),value::x
integer(4)::signbitf
end
end interface
interface
attributes(device) function signbit(x) bind(c,name="__nv_signbitd")
real(8),value::x
integer(4)::signbit
end
end interface
interface
attributes(device) subroutine sincosf(x,y,z) bind(c,name="__nv_sincosf")
real(4),value::x
real(4),device::y
real(4),device::z
end
end interface
interface
attributes(device) subroutine sincos(x,y,z) bind(c,name="__nv_sincos")
real(8),value::x
real(8),device::y
real(8),device::z
end
end interface
interface
attributes(device) subroutine sincospif(x,y,z) bind(c,name="__nv_sincospif")
real(4),value::x
real(4),device::y
real(4),device::z
end
end interface
interface
attributes(device) subroutine sincospi(x,y,z) bind(c,name="__nv_sincospi")
real(8),value::x
real(8),device::y
real(8),device::z
end
end interface
interface
attributes(device) function __cosf(x) bind(c,name="__nv_fast_cosf")
real(4),value::x
real(4)::__cosf
end
end interface
interface
attributes(device) function __exp10f(r) bind(c,name="__nv_fast_exp10f")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4)::__exp10f
end
end interface
interface
attributes(device) function __expf(r) bind(c,name="__nv_fast_expf")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4)::__expf
end
end interface
interface
attributes(device) function __fdividef(r,d) bind(c,name="__nv_fast_fdividef")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4),value::d
!dir$ ignore_tkr(d) d
real(4)::__fdividef
end
end interface
interface
attributes(device) function __log10f(r) bind(c,name="__nv_fast_log10f")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4)::__log10f
end
end interface
interface
attributes(device) function __log2f(r) bind(c,name="__nv_fast_log2f")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4)::__log2f
end
end interface
interface
attributes(device) function __logf(r) bind(c,name="__nv_fast_logf")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4)::__logf
end
end interface
interface
attributes(device) function __powf(x,y) bind(c,name="__nv_fast_powf")
real(4),value::x
!dir$ ignore_tkr(d) x
real(4),value::y
!dir$ ignore_tkr(tkrdm) y
real(4)::__powf
end
end interface
interface
attributes(device) subroutine __sincosf(r,s,c) bind(c,name="__nv_fast_sincosf")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4)::s
!dir$ ignore_tkr(d) s
real(4)::c
!dir$ ignore_tkr(d) c
end
end interface
interface
attributes(device) function __sinf(r) bind(c,name="__nv_fast_sinf")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4)::__sinf
end
end interface
interface
attributes(device) function __tanf(r) bind(c,name="__nv_fast_tanf")
real(4),value::r
!dir$ ignore_tkr(d) r
real(4)::__tanf
end
end interface
interface
attributes(device) function cospif(x) bind(c,name="__nv_cospif")
real(4),value::x
real(4)::cospif
end
end interface
interface
attributes(device) function cospi(x) bind(c,name="__nv_cospi")
real(8),value::x
real(8)::cospi
end
end interface
interface
attributes(device) function sinpif(x) bind(c,name="__nv_sinpif")
real(4),value::x
real(4)::sinpif
end
end interface
interface
attributes(device) function sinpi(x) bind(c,name="__nv_sinpi")
real(8),value::x
real(8)::sinpi
end
end interface
interface
attributes(device) function __mulhi(i,j) bind(c,name="__nv_mulhi")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4),value::j
!dir$ ignore_tkr(d) j
integer(4)::__mulhi
end
end interface
interface
attributes(device) function __umulhi(i,j) bind(c,name="__nv_umulhi")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4),value::j
!dir$ ignore_tkr(d) j
integer(4)::__umulhi
end
end interface
interface
attributes(device) function __mul64hi(i,j) bind(c,name="__nv_mul64hi")
integer(8),value::i
!dir$ ignore_tkr(d) i
integer(8),value::j
!dir$ ignore_tkr(d) j
integer(8)::__mul64hi
end
end interface
interface
attributes(device) function __umul64hi(i,j) bind(c,name="__nv_umul64hi")
integer(8),value::i
!dir$ ignore_tkr(d) i
integer(8),value::j
!dir$ ignore_tkr(d) j
integer(8)::__umul64hi
end
end interface
interface
attributes(device) function __int_as_float(i) bind(c,name="__nv_int_as_float")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(4)::__int_as_float
end
end interface
interface
attributes(device) function __float_as_int(i) bind(c,name="__nv_float_as_int")
real(4),value::i
!dir$ ignore_tkr(d) i
integer(4)::__float_as_int
end
end interface
interface
attributes(device) function __float2half_rn(r) bind(c,name="__nv_float2half_rn")
real(4),value::r
!dir$ ignore_tkr(d) r
real(2)::__float2half_rn
end
end interface
interface
attributes(device) function __float2int_rd(r) bind(c,name="__nv_float2int_rd")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(4)::__float2int_rd
end
end interface
interface
attributes(device) function __float2int_rn(r) bind(c,name="__nv_float2int_rn")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(4)::__float2int_rn
end
end interface
interface
attributes(device) function __float2int_ru(r) bind(c,name="__nv_float2int_ru")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(4)::__float2int_ru
end
end interface
interface
attributes(device) function __float2int_rz(r) bind(c,name="__nv_float2int_rz")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(4)::__float2int_rz
end
end interface
interface
attributes(device) function __float2uint_rd(r) bind(c,name="__nv_float2uint_rd")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(4)::__float2uint_rd
end
end interface
interface
attributes(device) function __float2uint_rn(r) bind(c,name="__nv_float2uint_rn")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(4)::__float2uint_rn
end
end interface
interface
attributes(device) function __float2uint_ru(r) bind(c,name="__nv_float2uint_ru")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(4)::__float2uint_ru
end
end interface
interface
attributes(device) function __float2uint_rz(r) bind(c,name="__nv_float2uint_rz")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(4)::__float2uint_rz
end
end interface
interface
attributes(device) function __float2ll_rd(r) bind(c,name="__nv_float2ll_rd")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(8)::__float2ll_rd
end
end interface
interface
attributes(device) function __float2ll_rn(r) bind(c,name="__nv_float2ll_rn")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(8)::__float2ll_rn
end
end interface
interface
attributes(device) function __float2ll_ru(r) bind(c,name="__nv_float2ll_ru")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(8)::__float2ll_ru
end
end interface
interface
attributes(device) function __float2ll_rz(r) bind(c,name="__nv_float2ll_rz")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(8)::__float2ll_rz
end
end interface
interface
attributes(device) function __float2ull_rd(r) bind(c,name="__nv_float2ull_rd")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(8)::__float2ull_rd
end
end interface
interface
attributes(device) function __float2ull_rn(r) bind(c,name="__nv_float2ull_rn")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(8)::__float2ull_rn
end
end interface
interface
attributes(device) function __float2ull_ru(r) bind(c,name="__nv_float2ull_ru")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(8)::__float2ull_ru
end
end interface
interface
attributes(device) function __float2ull_rz(r) bind(c,name="__nv_float2ull_rz")
real(4),value::r
!dir$ ignore_tkr(d) r
integer(8)::__float2ull_rz
end
end interface
interface
attributes(device) function __half2float(i) bind(c,name="__nv_half2float")
real(2),value::i
!dir$ ignore_tkr(d) i
real(4)::__half2float
end
end interface
interface
attributes(device) function __double_as_longlong(i) bind(c,name="__nv_double_as_longlong")
real(8),value::i
!dir$ ignore_tkr(d) i
integer(8)::__double_as_longlong
end
end interface
interface
attributes(device) function __longlong_as_double(i) bind(c,name="__nv_longlong_as_double")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(8)::__longlong_as_double
end
end interface
interface
attributes(device) function __double2int_rd(r) bind(c,name="__nv_double2int_rd")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2int_rd
end
end interface
interface
attributes(device) function __double2int_rn(r) bind(c,name="__nv_double2int_rn")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2int_rn
end
end interface
interface
attributes(device) function __double2int_ru(r) bind(c,name="__nv_double2int_ru")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2int_ru
end
end interface
interface
attributes(device) function __double2int_rz(r) bind(c,name="__nv_double2int_rz")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2int_rz
end
end interface
interface
attributes(device) function __double2uint_rd(r) bind(c,name="__nv_double2uint_rd")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2uint_rd
end
end interface
interface
attributes(device) function __double2uint_rn(r) bind(c,name="__nv_double2uint_rn")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2uint_rn
end
end interface
interface
attributes(device) function __double2uint_ru(r) bind(c,name="__nv_double2uint_ru")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2uint_ru
end
end interface
interface
attributes(device) function __double2uint_rz(r) bind(c,name="__nv_double2uint_rz")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2uint_rz
end
end interface
interface
attributes(device) function __double2float_rn(r) bind(c,name="__nv_double2float_rn")
real(8),value::r
!dir$ ignore_tkr(d) r
real(4)::__double2float_rn
end
end interface
interface
attributes(device) function __double2float_rz(r) bind(c,name="__nv_double2float_rz")
real(8),value::r
!dir$ ignore_tkr(d) r
real(4)::__double2float_rz
end
end interface
interface
attributes(device) function __double2float_ru(r) bind(c,name="__nv_double2float_ru")
real(8),value::r
!dir$ ignore_tkr(d) r
real(4)::__double2float_ru
end
end interface
interface
attributes(device) function __double2float_rd(r) bind(c,name="__nv_double2float_rd")
real(8),value::r
!dir$ ignore_tkr(d) r
real(4)::__double2float_rd
end
end interface
interface
attributes(device) function __double2loint(r) bind(c,name="__nv_double2loint")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2loint
end
end interface
interface
attributes(device) function __double2hiint(r) bind(c,name="__nv_double2hiint")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(4)::__double2hiint
end
end interface
interface
attributes(device) function __hiloint2double(i,j) bind(c,name="__nv_hiloint2double")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4),value::j
!dir$ ignore_tkr(d) j
real(8)::__hiloint2double
end
end interface
interface
attributes(device) function __int2float_rd(i) bind(c,name="__nv_int2float_rd")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(4)::__int2float_rd
end
end interface
interface
attributes(device) function __int2float_rn(i) bind(c,name="__nv_int2float_rn")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(4)::__int2float_rn
end
end interface
interface
attributes(device) function __int2float_ru(i) bind(c,name="__nv_int2float_ru")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(4)::__int2float_ru
end
end interface
interface
attributes(device) function __int2float_rz(i) bind(c,name="__nv_int2float_rz")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(4)::__int2float_rz
end
end interface
interface
attributes(device) function __int2double_rn(i) bind(c,name="__nv_int2double_rn")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(8)::__int2double_rn
end
end interface
interface
attributes(device) function __uint2float_rd(i) bind(c,name="__nv_uint2float_rd")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(4)::__uint2float_rd
end
end interface
interface
attributes(device) function __uint2float_rn(i) bind(c,name="__nv_uint2float_rn")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(4)::__uint2float_rn
end
end interface
interface
attributes(device) function __uint2float_ru(i) bind(c,name="__nv_uint2float_ru")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(4)::__uint2float_ru
end
end interface
interface
attributes(device) function __uint2float_rz(i) bind(c,name="__nv_uint2float_rz")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(4)::__uint2float_rz
end
end interface
interface
attributes(device) function __uint2double_rn(i) bind(c,name="__nv_uint2double_rn")
integer(4),value::i
!dir$ ignore_tkr(d) i
real(8)::__uint2double_rn
end
end interface
interface
attributes(device) function __double2ll_rd(r) bind(c,name="__nv_double2ll_rd")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(8)::__double2ll_rd
end
end interface
interface
attributes(device) function __double2ll_rn(r) bind(c,name="__nv_double2ll_rn")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(8)::__double2ll_rn
end
end interface
interface
attributes(device) function __double2ll_ru(r) bind(c,name="__nv_double2ll_ru")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(8)::__double2ll_ru
end
end interface
interface
attributes(device) function __double2ll_rz(r) bind(c,name="__nv_double2ll_rz")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(8)::__double2ll_rz
end
end interface
interface
attributes(device) function __double2ull_rd(r) bind(c,name="__nv_double2ull_rd")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(8)::__double2ull_rd
end
end interface
interface
attributes(device) function __double2ull_rn(r) bind(c,name="__nv_double2ull_rn")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(8)::__double2ull_rn
end
end interface
interface
attributes(device) function __double2ull_ru(r) bind(c,name="__nv_double2ull_ru")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(8)::__double2ull_ru
end
end interface
interface
attributes(device) function __double2ull_rz(r) bind(c,name="__nv_double2ull_rz")
real(8),value::r
!dir$ ignore_tkr(d) r
integer(8)::__double2ull_rz
end
end interface
interface
attributes(device) function __ll2float_rd(i) bind(c,name="__nv_ll2float_rd")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(4)::__ll2float_rd
end
end interface
interface
attributes(device) function __ll2float_rn(i) bind(c,name="__nv_ll2float_rn")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(4)::__ll2float_rn
end
end interface
interface
attributes(device) function __ll2float_ru(i) bind(c,name="__nv_ll2float_ru")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(4)::__ll2float_ru
end
end interface
interface
attributes(device) function __ll2float_rz(i) bind(c,name="__nv_ll2float_rz")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(4)::__ll2float_rz
end
end interface
interface
attributes(device) function __ll2double_rd(i) bind(c,name="__nv_ll2double_rd")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(8)::__ll2double_rd
end
end interface
interface
attributes(device) function __ll2double_rn(i) bind(c,name="__nv_ll2double_rn")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(8)::__ll2double_rn
end
end interface
interface
attributes(device) function __ll2double_ru(i) bind(c,name="__nv_ll2double_ru")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(8)::__ll2double_ru
end
end interface
interface
attributes(device) function __ll2double_rz(i) bind(c,name="__nv_ll2double_rz")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(8)::__ll2double_rz
end
end interface
interface
attributes(device) function __ull2double_rd(i) bind(c,name="__nv_ull2double_rd")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(8)::__ull2double_rd
end
end interface
interface
attributes(device) function __ull2double_rn(i) bind(c,name="__nv_ull2double_rn")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(8)::__ull2double_rn
end
end interface
interface
attributes(device) function __ull2double_ru(i) bind(c,name="__nv_ull2double_ru")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(8)::__ull2double_ru
end
end interface
interface
attributes(device) function __ull2double_rz(i) bind(c,name="__nv_ull2double_rz")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(8)::__ull2double_rz
end
end interface
interface
attributes(device) function __ull2float_rd(i) bind(c,name="__nv_ull2float_rd")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(4)::__ull2float_rd
end
end interface
interface
attributes(device) function __ull2float_rn(i) bind(c,name="__nv_ull2float_rn")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(4)::__ull2float_rn
end
end interface
interface
attributes(device) function __ull2float_ru(i) bind(c,name="__nv_ull2float_ru")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(4)::__ull2float_ru
end
end interface
interface
attributes(device) function __ull2float_rz(i) bind(c,name="__nv_ull2float_rz")
integer(8),value::i
!dir$ ignore_tkr(d) i
real(4)::__ull2float_rz
end
end interface
interface
attributes(device) function __mul24(i,j) bind(c,name="__nv_mul24")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4),value::j
!dir$ ignore_tkr(d) j
integer(4)::__mul24
end
end interface
interface
attributes(device) function __umul24(i,j) bind(c,name="__nv_umul24")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4),value::j
!dir$ ignore_tkr(d) j
integer(4)::__umul24
end
end interface
interface
attributes(device) function __drcp_rd(x) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8)::__drcp_rd
end
end interface
interface
attributes(device) function __drcp_rn(x) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8)::__drcp_rn
end
end interface
interface
attributes(device) function __drcp_ru(x) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8)::__drcp_ru
end
end interface
interface
attributes(device) function __drcp_rz(x) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8)::__drcp_rz
end
end interface
interface
attributes(device) function __dsqrt_rd(x) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8)::__dsqrt_rd
end
end interface
interface
attributes(device) function __dsqrt_rn(x) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8)::__dsqrt_rn
end
end interface
interface
attributes(device) function __dsqrt_ru(x) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8)::__dsqrt_ru
end
end interface
interface
attributes(device) function __dsqrt_rz(x) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8)::__dsqrt_rz
end
end interface
interface
attributes(device) function __ddiv_rn(x,y) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8),value::y
!dir$ ignore_tkr(d) y
real(8)::__ddiv_rn
end
end interface
interface
attributes(device) function __ddiv_rz(x,y) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8),value::y
!dir$ ignore_tkr(d) y
real(8)::__ddiv_rz
end
end interface
interface
attributes(device) function __ddiv_ru(x,y) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8),value::y
!dir$ ignore_tkr(d) y
real(8)::__ddiv_ru
end
end interface
interface
attributes(device) function __ddiv_rd(x,y) bind(c)
real(8),value::x
!dir$ ignore_tkr(d) x
real(8),value::y
!dir$ ignore_tkr(d) y
real(8)::__ddiv_rd
end
end interface
interface
attributes(device) function __clz(i) bind(c,name="__nv_clz")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4)::__clz
end
end interface
interface
attributes(device) function __clzll(i) bind(c,name="__nv_clzll")
integer(8),value::i
!dir$ ignore_tkr(d) i
integer(4)::__clzll
end
end interface
interface
attributes(device) function __ffs(i) bind(c,name="__nv_ffs")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4)::__ffs
end
end interface
interface
attributes(device) function __ffsll(i) bind(c,name="__nv_ffsll")
integer(8),value::i
!dir$ ignore_tkr(d) i
integer(4)::__ffsll
end
end interface
interface
attributes(device) function __popc(i) bind(c,name="__nv_popc")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4)::__popc
end
end interface
interface
attributes(device) function __popcll(i) bind(c,name="__nv_popcll")
integer(8),value::i
!dir$ ignore_tkr(d) i
integer(4)::__popcll
end
end interface
interface
attributes(device) function __brev(i) bind(c,name="__nv_brev")
integer(4),value::i
!dir$ ignore_tkr(d) i
integer(4)::__brev
end
end interface
interface
attributes(device) function __brevll(i) bind(c,name="__nv_brevll")
integer(8),value::i
!dir$ ignore_tkr(d) i
integer(8)::__brevll
end
end interface
interface
pure attributes(device) function atomicaddi(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicaddi
end
end interface
interface
pure attributes(device) function atomicaddf(address,val)
real(4),intent(inout)::address
!dir$ ignore_tkr(d) address
real(4),value::val
!dir$ ignore_tkr(d) val
real(4)::atomicaddf
end
end interface
interface
pure attributes(device) function atomicaddd(address,val)
real(8),intent(inout)::address
!dir$ ignore_tkr(d) address
real(8),value::val
!dir$ ignore_tkr(d) val
real(8)::atomicaddd
end
end interface
interface
pure attributes(device) function atomicaddl(address,val)
integer(8),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(8),value::val
!dir$ ignore_tkr(d) val
integer(8)::atomicaddl
end
end interface
interface
pure attributes(device) function atomicaddr2(address,val)
real(2),intent(inout)::address(1_8:2_8)
!dir$ ignore_tkr(rd) address
real(2),intent(in)::val(1_8:2_8)
!dir$ ignore_tkr(d) val
integer(4)::atomicaddr2
end
end interface
interface
pure attributes(device) function atomicaddvector_r2x2(address,val) result(z)
real(2),intent(inout)::address(1_8:2_8)
!dir$ ignore_tkr(rd) address
real(2),intent(in)::val(1_8:2_8)
!dir$ ignore_tkr(d) val
real(2)::z(1_8:2_8)
end
end interface
interface
pure attributes(device) function atomicaddvector_r4x2(address,val) result(z)
real(4),intent(inout)::address(1_8:2_8)
!dir$ ignore_tkr(rd) address
real(4),intent(in)::val(1_8:2_8)
!dir$ ignore_tkr(d) val
real(4)::z(1_8:2_8)
end
end interface
interface
pure attributes(device) function atomicadd_r4x2(address,val) result(z)
real(4),intent(inout)::address(1_8:2_8)
!dir$ ignore_tkr(rd) address
real(4),intent(in)::val(1_8:2_8)
!dir$ ignore_tkr(d) val
real(4)::z(1_8:2_8)
end
end interface
interface
pure attributes(device) function atomicadd_r4x4(address,val) result(z)
real(4),intent(inout)::address(1_8:4_8)
!dir$ ignore_tkr(rd) address
real(4),intent(in)::val(1_8:4_8)
!dir$ ignore_tkr(d) val
real(4)::z(1_8:4_8)
end
end interface
interface
pure attributes(device) function atomicsubi(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicsubi
end
end interface
interface
pure attributes(device) function atomicsubf(address,val)
real(4),intent(inout)::address
!dir$ ignore_tkr(d) address
real(4),value::val
!dir$ ignore_tkr(d) val
real(4)::atomicsubf
end
end interface
interface
pure attributes(device) function atomicsubd(address,val)
real(8),intent(inout)::address
!dir$ ignore_tkr(d) address
real(8),value::val
!dir$ ignore_tkr(d) val
real(8)::atomicsubd
end
end interface
interface
pure attributes(device) function atomicsubl(address,val)
integer(8),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(8),value::val
!dir$ ignore_tkr(kd) val
integer(8)::atomicsubl
end
end interface
interface
pure attributes(device) function atomicmaxi(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicmaxi
end
end interface
interface
pure attributes(device) function atomicmaxf(address,val)
real(4),intent(inout)::address
!dir$ ignore_tkr(d) address
real(4),value::val
!dir$ ignore_tkr(d) val
real(4)::atomicmaxf
end
end interface
interface
pure attributes(device) function atomicmaxd(address,val)
real(8),intent(inout)::address
!dir$ ignore_tkr(d) address
real(8),value::val
!dir$ ignore_tkr(d) val
real(8)::atomicmaxd
end
end interface
interface
pure attributes(device) function atomicmaxl(address,val)
integer(8),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(8),value::val
!dir$ ignore_tkr(kd) val
integer(8)::atomicmaxl
end
end interface
interface
pure attributes(device) function atomicmini(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicmini
end
end interface
interface
pure attributes(device) function atomicminf(address,val)
real(4),intent(inout)::address
!dir$ ignore_tkr(d) address
real(4),value::val
!dir$ ignore_tkr(d) val
real(4)::atomicminf
end
end interface
interface
pure attributes(device) function atomicmind(address,val)
real(8),intent(inout)::address
!dir$ ignore_tkr(d) address
real(8),value::val
!dir$ ignore_tkr(d) val
real(8)::atomicmind
end
end interface
interface
pure attributes(device) function atomicminl(address,val)
integer(8),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(8),value::val
!dir$ ignore_tkr(kd) val
integer(8)::atomicminl
end
end interface
interface
pure attributes(device) function atomicandi(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicandi
end
end interface
interface
pure attributes(device) function atomicori(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicori
end
end interface
interface
pure attributes(device) function atomicinci(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicinci
end
end interface
interface
pure attributes(device) function atomicdeci(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(d) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicdeci
end
end interface
interface
pure attributes(device) function atomiccasi(address,val,val2)
integer(4),intent(inout)::address
!dir$ ignore_tkr(rd) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4),value::val2
!dir$ ignore_tkr(d) val2
integer(4)::atomiccasi
end
end interface
interface
pure attributes(device) function atomiccasul(address,val,val2)
integer(8),intent(inout)::address
!dir$ ignore_tkr(rd) address
integer(8),value::val
!dir$ ignore_tkr(kd) val
integer(8),value::val2
!dir$ ignore_tkr(kd) val2
integer(8)::atomiccasul
end
end interface
interface
pure attributes(device) function atomiccasf(address,val,val2)
real(4),intent(inout)::address
!dir$ ignore_tkr(rd) address
real(4),value::val
!dir$ ignore_tkr(d) val
real(4),value::val2
!dir$ ignore_tkr(d) val2
real(4)::atomiccasf
end
end interface
interface
pure attributes(device) function atomiccasd(address,val,val2)
real(8),intent(inout)::address
!dir$ ignore_tkr(rd) address
real(8),value::val
!dir$ ignore_tkr(d) val
real(8),value::val2
!dir$ ignore_tkr(d) val2
real(8)::atomiccasd
end
end interface
interface
pure attributes(device) function atomicexchi(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(rd) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicexchi
end
end interface
interface
pure attributes(device) function atomicexchul(address,val)
integer(8),intent(inout)::address
!dir$ ignore_tkr(rd) address
integer(8),value::val
!dir$ ignore_tkr(kd) val
integer(8)::atomicexchul
end
end interface
interface
pure attributes(device) function atomicexchf(address,val)
real(4),intent(inout)::address
!dir$ ignore_tkr(rd) address
real(4),value::val
!dir$ ignore_tkr(d) val
real(4)::atomicexchf
end
end interface
interface
pure attributes(device) function atomicexchd(address,val)
real(8),intent(inout)::address
!dir$ ignore_tkr(rd) address
real(8),value::val
!dir$ ignore_tkr(d) val
real(8)::atomicexchd
end
end interface
interface
pure attributes(device) function atomicxori(address,val)
integer(4),intent(inout)::address
!dir$ ignore_tkr(rd) address
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::atomicxori
end
end interface
interface
attributes(device) function clock()
integer(4)::clock
end
end interface
interface
attributes(device) function clock64()
integer(8)::clock64
end
end interface
interface
attributes(device) function globaltimer()
integer(8)::globaltimer
end
end interface
interface
attributes(device) function match_all_syncjj(mask,val,pred)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::pred
!dir$ ignore_tkr(d) pred
integer(4)::match_all_syncjj
end
end interface
interface
attributes(device) function match_all_syncjx(mask,val,pred)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
integer(8),value::val
!dir$ ignore_tkr(d) val
integer(4)::pred
!dir$ ignore_tkr(d) pred
integer(4)::match_all_syncjx
end
end interface
interface
attributes(device) function match_all_syncjf(mask,val,pred)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
real(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::pred
!dir$ ignore_tkr(d) pred
integer(4)::match_all_syncjf
end
end interface
interface
attributes(device) function match_all_syncjd(mask,val,pred)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
real(8),value::val
!dir$ ignore_tkr(d) val
integer(4)::pred
!dir$ ignore_tkr(d) pred
integer(4)::match_all_syncjd
end
end interface
interface
attributes(device) function match_any_syncjj(mask,val)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
integer(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::match_any_syncjj
end
end interface
interface
attributes(device) function match_any_syncjx(mask,val)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
integer(8),value::val
!dir$ ignore_tkr(d) val
integer(4)::match_any_syncjx
end
end interface
interface
attributes(device) function match_any_syncjf(mask,val)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
real(4),value::val
!dir$ ignore_tkr(d) val
integer(4)::match_any_syncjf
end
end interface
interface
attributes(device) function match_any_syncjd(mask,val)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
real(8),value::val
!dir$ ignore_tkr(d) val
integer(4)::match_any_syncjd
end
end interface
interface
attributes(device) function all_sync(mask,pred)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
integer(4),value::pred
!dir$ ignore_tkr(td) pred
integer(4)::all_sync
end
end interface
interface
attributes(device) function any_sync(mask,pred)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
integer(4),value::pred
!dir$ ignore_tkr(td) pred
integer(4)::any_sync
end
end interface
interface
attributes(device) function ballot_sync(mask,pred)
integer(4),value::mask
!dir$ ignore_tkr(d) mask
integer(4),value::pred
!dir$ ignore_tkr(td) pred
integer(4)::ballot_sync
end
end interface
interface
pure attributes(device) function __ldcg_i4(x) bind(c)
integer(4),intent(in)::x
!dir$ ignore_tkr(d) x
integer(4)::__ldcg_i4
end
end interface
interface
pure attributes(device) function __ldcg_i8(x) bind(c)
integer(8),intent(in)::x
!dir$ ignore_tkr(d) x
integer(8)::__ldcg_i8
end
end interface
interface
pure attributes(device) function __ldcg_cd(x) bind(c) result(y)
import::c_devptr
type(c_devptr),intent(in)::x
!dir$ ignore_tkr(d) x
type(c_devptr)::y
end
end interface
interface
pure attributes(device) function __ldcg_r2(x) bind(c)
real(2),intent(in)::x
!dir$ ignore_tkr(d) x
real(2)::__ldcg_r2
end
end interface
interface
pure attributes(device) function __ldcg_r4(x) bind(c)
real(4),intent(in)::x
!dir$ ignore_tkr(d) x
real(4)::__ldcg_r4
end
end interface
interface
pure attributes(device) function __ldcg_r8(x) bind(c)
real(8),intent(in)::x
!dir$ ignore_tkr(d) x
real(8)::__ldcg_r8
end
end interface
interface
pure attributes(device) function __ldcg_c4(x) bind(c,name="__ldcg_c4x")
complex(4),intent(in)::x
!dir$ ignore_tkr(d) x
complex(4)::__ldcg_c4
end
end interface
interface
pure attributes(device) function __ldcg_c8(x) bind(c,name="__ldcg_c8x")
complex(8),intent(in)::x
!dir$ ignore_tkr(d) x
complex(8)::__ldcg_c8
end
end interface
interface
pure attributes(device) function __ldcg_i4x4(x) result(y)
integer(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
integer(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldcg_i8x2(x) result(y)
integer(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
integer(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldcg_r2x2(x) result(y)
real(2),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(2)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldcg_r4x4(x) result(y)
real(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
real(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldcg_r8x2(x) result(y)
real(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldca_i4(x) bind(c)
integer(4),intent(in)::x
!dir$ ignore_tkr(d) x
integer(4)::__ldca_i4
end
end interface
interface
pure attributes(device) function __ldca_i8(x) bind(c)
integer(8),intent(in)::x
!dir$ ignore_tkr(d) x
integer(8)::__ldca_i8
end
end interface
interface
pure attributes(device) function __ldca_cd(x) bind(c) result(y)
import::c_devptr
type(c_devptr),intent(in)::x
!dir$ ignore_tkr(d) x
type(c_devptr)::y
end
end interface
interface
pure attributes(device) function __ldca_r2(x) bind(c)
real(2),intent(in)::x
!dir$ ignore_tkr(d) x
real(2)::__ldca_r2
end
end interface
interface
pure attributes(device) function __ldca_r4(x) bind(c)
real(4),intent(in)::x
!dir$ ignore_tkr(d) x
real(4)::__ldca_r4
end
end interface
interface
pure attributes(device) function __ldca_r8(x) bind(c)
real(8),intent(in)::x
!dir$ ignore_tkr(d) x
real(8)::__ldca_r8
end
end interface
interface
pure attributes(device) function __ldca_c4(x) bind(c,name="__ldca_c4x")
complex(4),intent(in)::x
!dir$ ignore_tkr(d) x
complex(4)::__ldca_c4
end
end interface
interface
pure attributes(device) function __ldca_c8(x) bind(c,name="__ldca_c8x")
complex(8),intent(in)::x
!dir$ ignore_tkr(d) x
complex(8)::__ldca_c8
end
end interface
interface
pure attributes(device) function __ldca_i4x4(x) result(y)
integer(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
integer(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldca_i8x2(x) result(y)
integer(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
integer(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldca_r2x2(x) result(y)
real(2),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(2)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldca_r4x4(x) result(y)
real(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
real(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldca_r8x2(x) result(y)
real(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldcs_i4(x) bind(c)
integer(4),intent(in)::x
!dir$ ignore_tkr(d) x
integer(4)::__ldcs_i4
end
end interface
interface
pure attributes(device) function __ldcs_i8(x) bind(c)
integer(8),intent(in)::x
!dir$ ignore_tkr(d) x
integer(8)::__ldcs_i8
end
end interface
interface
pure attributes(device) function __ldcs_cd(x) bind(c) result(y)
import::c_devptr
type(c_devptr),intent(in)::x
!dir$ ignore_tkr(d) x
type(c_devptr)::y
end
end interface
interface
pure attributes(device) function __ldcs_r2(x) bind(c)
real(2),intent(in)::x
!dir$ ignore_tkr(d) x
real(2)::__ldcs_r2
end
end interface
interface
pure attributes(device) function __ldcs_r4(x) bind(c)
real(4),intent(in)::x
!dir$ ignore_tkr(d) x
real(4)::__ldcs_r4
end
end interface
interface
pure attributes(device) function __ldcs_r8(x) bind(c)
real(8),intent(in)::x
!dir$ ignore_tkr(d) x
real(8)::__ldcs_r8
end
end interface
interface
pure attributes(device) function __ldcs_c4(x) bind(c,name="__ldcs_c4x")
complex(4),intent(in)::x
!dir$ ignore_tkr(d) x
complex(4)::__ldcs_c4
end
end interface
interface
pure attributes(device) function __ldcs_c8(x) bind(c,name="__ldcs_c8x")
complex(8),intent(in)::x
!dir$ ignore_tkr(d) x
complex(8)::__ldcs_c8
end
end interface
interface
pure attributes(device) function __ldcs_i4x4(x) result(y)
integer(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
integer(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldcs_i8x2(x) result(y)
integer(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
integer(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldcs_r2x2(x) result(y)
real(2),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(2)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldcs_r4x4(x) result(y)
real(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
real(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldcs_r8x2(x) result(y)
real(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldlu_i4(x) bind(c)
integer(4),intent(in)::x
!dir$ ignore_tkr(d) x
integer(4)::__ldlu_i4
end
end interface
interface
pure attributes(device) function __ldlu_i8(x) bind(c)
integer(8),intent(in)::x
!dir$ ignore_tkr(d) x
integer(8)::__ldlu_i8
end
end interface
interface
pure attributes(device) function __ldlu_cd(x) bind(c) result(y)
import::c_devptr
type(c_devptr),intent(in)::x
!dir$ ignore_tkr(d) x
type(c_devptr)::y
end
end interface
interface
pure attributes(device) function __ldlu_r2(x) bind(c)
real(2),intent(in)::x
!dir$ ignore_tkr(d) x
real(2)::__ldlu_r2
end
end interface
interface
pure attributes(device) function __ldlu_r4(x) bind(c)
real(4),intent(in)::x
!dir$ ignore_tkr(d) x
real(4)::__ldlu_r4
end
end interface
interface
pure attributes(device) function __ldlu_r8(x) bind(c)
real(8),intent(in)::x
!dir$ ignore_tkr(d) x
real(8)::__ldlu_r8
end
end interface
interface
pure attributes(device) function __ldlu_c4(x) bind(c,name="__ldlu_c4x")
complex(4),intent(in)::x
!dir$ ignore_tkr(d) x
complex(4)::__ldlu_c4
end
end interface
interface
pure attributes(device) function __ldlu_c8(x) bind(c,name="__ldlu_c8x")
complex(8),intent(in)::x
!dir$ ignore_tkr(d) x
complex(8)::__ldlu_c8
end
end interface
interface
pure attributes(device) function __ldlu_i4x4(x) result(y)
integer(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
integer(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldlu_i8x2(x) result(y)
integer(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
integer(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldlu_r2x2(x) result(y)
real(2),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(2)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldlu_r4x4(x) result(y)
real(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
real(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldlu_r8x2(x) result(y)
real(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldcv_i4(x) bind(c)
integer(4),intent(in)::x
!dir$ ignore_tkr(d) x
integer(4)::__ldcv_i4
end
end interface
interface
pure attributes(device) function __ldcv_i8(x) bind(c)
integer(8),intent(in)::x
!dir$ ignore_tkr(d) x
integer(8)::__ldcv_i8
end
end interface
interface
pure attributes(device) function __ldcv_cd(x) bind(c) result(y)
import::c_devptr
type(c_devptr),intent(in)::x
!dir$ ignore_tkr(d) x
type(c_devptr)::y
end
end interface
interface
pure attributes(device) function __ldcv_r2(x) bind(c)
real(2),intent(in)::x
!dir$ ignore_tkr(d) x
real(2)::__ldcv_r2
end
end interface
interface
pure attributes(device) function __ldcv_r4(x) bind(c)
real(4),intent(in)::x
!dir$ ignore_tkr(d) x
real(4)::__ldcv_r4
end
end interface
interface
pure attributes(device) function __ldcv_r8(x) bind(c)
real(8),intent(in)::x
!dir$ ignore_tkr(d) x
real(8)::__ldcv_r8
end
end interface
interface
pure attributes(device) function __ldcv_c4(x) bind(c,name="__ldcv_c4x")
complex(4),intent(in)::x
!dir$ ignore_tkr(d) x
complex(4)::__ldcv_c4
end
end interface
interface
pure attributes(device) function __ldcv_c8(x) bind(c,name="__ldcv_c8x")
complex(8),intent(in)::x
!dir$ ignore_tkr(d) x
complex(8)::__ldcv_c8
end
end interface
interface
pure attributes(device) function __ldcv_i4x4(x) result(y)
integer(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
integer(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldcv_i8x2(x) result(y)
integer(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
integer(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldcv_r2x2(x) result(y)
real(2),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(2)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) function __ldcv_r4x4(x) result(y)
real(4),intent(in)::x(1_8:4_8)
!dir$ ignore_tkr(d) x
real(4)::y(1_8:4_8)
end
end interface
interface
pure attributes(device) function __ldcv_r8x2(x) result(y)
real(8),intent(in)::x(1_8:2_8)
!dir$ ignore_tkr(d) x
real(8)::y(1_8:2_8)
end
end interface
interface
pure attributes(device) subroutine __stwb_i4(y,x) bind(c)
integer(4),intent(in),device::y
!dir$ ignore_tkr(d) y
integer(4),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_i8(y,x) bind(c)
integer(8),intent(in),device::y
!dir$ ignore_tkr(d) y
integer(8),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_cd(y,x) bind(c)
import::c_devptr
type(c_devptr),intent(in),device::y
!dir$ ignore_tkr(d) y
type(c_devptr),intent(in),device::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_r2(y,x) bind(c)
real(2),intent(in),device::y
!dir$ ignore_tkr(d) y
real(2),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_r4(y,x) bind(c)
real(4),intent(in),device::y
!dir$ ignore_tkr(d) y
real(4),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_r8(y,x) bind(c)
real(8),intent(in),device::y
!dir$ ignore_tkr(d) y
real(8),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_c4(y,x) bind(c)
complex(4),intent(in),device::y
!dir$ ignore_tkr(d) y
complex(4),intent(in),device::x
!dir$ ignore_tkr(rd) x
end
end interface
interface
pure attributes(device) subroutine __stwb_c8(y,x) bind(c)
complex(8),intent(in),device::y
!dir$ ignore_tkr(d) y
complex(8),intent(in),device::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_i4x4(y,x) bind(c)
integer(4),intent(in),device::y(1_8:4_8)
!dir$ ignore_tkr(d) y
integer(4),intent(in),device::x(1_8:4_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_i8x2(y,x) bind(c)
integer(8),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
integer(8),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_r2x2(y,x) bind(c)
real(2),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
real(2),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_r4x4(y,x) bind(c)
real(4),intent(in),device::y(1_8:4_8)
!dir$ ignore_tkr(d) y
real(4),intent(in),device::x(1_8:4_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwb_r8x2(y,x) bind(c)
real(8),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
real(8),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_i4(y,x) bind(c)
integer(4),intent(in),device::y
!dir$ ignore_tkr(d) y
integer(4),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_i8(y,x) bind(c)
integer(8),intent(in),device::y
!dir$ ignore_tkr(d) y
integer(8),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_cd(y,x) bind(c)
import::c_devptr
type(c_devptr),intent(in),device::y
!dir$ ignore_tkr(d) y
type(c_devptr),intent(in),device::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_r2(y,x) bind(c)
real(2),intent(in),device::y
!dir$ ignore_tkr(d) y
real(2),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_r4(y,x) bind(c)
real(4),intent(in),device::y
!dir$ ignore_tkr(d) y
real(4),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_r8(y,x) bind(c)
real(8),intent(in),device::y
!dir$ ignore_tkr(d) y
real(8),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_c4(y,x) bind(c)
complex(4),intent(in),device::y
!dir$ ignore_tkr(d) y
complex(4),intent(in),device::x
!dir$ ignore_tkr(rd) x
end
end interface
interface
pure attributes(device) subroutine __stcg_c8(y,x) bind(c)
complex(8),intent(in),device::y
!dir$ ignore_tkr(d) y
complex(8),intent(in),device::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_i4x4(y,x) bind(c)
integer(4),intent(in),device::y(1_8:4_8)
!dir$ ignore_tkr(d) y
integer(4),intent(in),device::x(1_8:4_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_i8x2(y,x) bind(c)
integer(8),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
integer(8),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_r2x2(y,x) bind(c)
real(2),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
real(2),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_r4x4(y,x) bind(c)
real(4),intent(in),device::y(1_8:4_8)
!dir$ ignore_tkr(d) y
real(4),intent(in),device::x(1_8:4_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcg_r8x2(y,x) bind(c)
real(8),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
real(8),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_i4(y,x) bind(c)
integer(4),intent(in),device::y
!dir$ ignore_tkr(d) y
integer(4),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_i8(y,x) bind(c)
integer(8),intent(in),device::y
!dir$ ignore_tkr(d) y
integer(8),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_cd(y,x) bind(c)
import::c_devptr
type(c_devptr),intent(in),device::y
!dir$ ignore_tkr(d) y
type(c_devptr),intent(in),device::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_r2(y,x) bind(c)
real(2),intent(in),device::y
!dir$ ignore_tkr(d) y
real(2),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_r4(y,x) bind(c)
real(4),intent(in),device::y
!dir$ ignore_tkr(d) y
real(4),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_r8(y,x) bind(c)
real(8),intent(in),device::y
!dir$ ignore_tkr(d) y
real(8),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_c4(y,x) bind(c)
complex(4),intent(in),device::y
!dir$ ignore_tkr(d) y
complex(4),intent(in),device::x
!dir$ ignore_tkr(rd) x
end
end interface
interface
pure attributes(device) subroutine __stcs_c8(y,x) bind(c)
complex(8),intent(in),device::y
!dir$ ignore_tkr(d) y
complex(8),intent(in),device::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_i4x4(y,x) bind(c)
integer(4),intent(in),device::y(1_8:4_8)
!dir$ ignore_tkr(d) y
integer(4),intent(in),device::x(1_8:4_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_i8x2(y,x) bind(c)
integer(8),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
integer(8),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_r2x2(y,x) bind(c)
real(2),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
real(2),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_r4x4(y,x) bind(c)
real(4),intent(in),device::y(1_8:4_8)
!dir$ ignore_tkr(d) y
real(4),intent(in),device::x(1_8:4_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stcs_r8x2(y,x) bind(c)
real(8),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
real(8),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_i4(y,x) bind(c)
integer(4),intent(in),device::y
!dir$ ignore_tkr(d) y
integer(4),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_i8(y,x) bind(c)
integer(8),intent(in),device::y
!dir$ ignore_tkr(d) y
integer(8),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_cd(y,x) bind(c)
import::c_devptr
type(c_devptr),intent(in),device::y
!dir$ ignore_tkr(d) y
type(c_devptr),intent(in),device::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_r2(y,x) bind(c)
real(2),intent(in),device::y
!dir$ ignore_tkr(d) y
real(2),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_r4(y,x) bind(c)
real(4),intent(in),device::y
!dir$ ignore_tkr(d) y
real(4),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_r8(y,x) bind(c)
real(8),intent(in),device::y
!dir$ ignore_tkr(d) y
real(8),value::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_c4(y,x) bind(c)
complex(4),intent(in),device::y
!dir$ ignore_tkr(d) y
complex(4),intent(in),device::x
!dir$ ignore_tkr(rd) x
end
end interface
interface
pure attributes(device) subroutine __stwt_c8(y,x) bind(c)
complex(8),intent(in),device::y
!dir$ ignore_tkr(d) y
complex(8),intent(in),device::x
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_i4x4(y,x) bind(c)
integer(4),intent(in),device::y(1_8:4_8)
!dir$ ignore_tkr(d) y
integer(4),intent(in),device::x(1_8:4_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_i8x2(y,x) bind(c)
integer(8),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
integer(8),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_r2x2(y,x) bind(c)
real(2),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
real(2),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_r4x4(y,x) bind(c)
real(4),intent(in),device::y(1_8:4_8)
!dir$ ignore_tkr(d) y
real(4),intent(in),device::x(1_8:4_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
pure attributes(device) subroutine __stwt_r8x2(y,x) bind(c)
real(8),intent(in),device::y(1_8:2_8)
!dir$ ignore_tkr(d) y
real(8),intent(in),device::x(1_8:2_8)
!dir$ ignore_tkr(d) x
end
end interface
interface
attributes(host,device) function on_device() bind(c)
logical(4)::on_device
end
end interface
interface
attributes(device) function barrier_arrive(barrier) result(token)
integer(8),shared::barrier
integer(8)::token
end
end interface
interface
attributes(device) function barrier_arrive_cnt(barrier,count) result(token)
integer(8),shared::barrier
integer(4),value::count
integer(8)::token
end
end interface
interface
attributes(device) subroutine barrier_init(barrier,count)
integer(8),shared::barrier
integer(4),value::count
end
end interface
interface
attributes(device) function barrier_try_wait(barrier,token)
integer(8),shared::barrier
integer(8),value::token
integer(4)::barrier_try_wait
end
end interface
interface
attributes(device) function barrier_try_wait_sleep(barrier,token,ns)
integer(8),shared::barrier
integer(8),value::token
integer(4),value::ns
integer(4)::barrier_try_wait_sleep
end
end interface
interface
attributes(device) subroutine fence_proxy_async()
end
end interface
interface
attributes(device) subroutine tma_bulk_commit_group()
end
end interface
interface
attributes(device) subroutine tma_bulk_wait_group()
end
end interface
interface
attributes(device) subroutine tma_bulk_g2s(barrier,src,dst,nbytes)
integer(8),shared::barrier
integer(4),device::src(1_8:*)
!dir$ ignore_tkr(tkrdm) src
integer(4),shared::dst(1_8:*)
!dir$ ignore_tkr(tkrdm) dst
integer(4),value::nbytes
end
end interface
interface
attributes(device) subroutine tma_bulk_ldc4(barrier,src,dst,nelems)
integer(8),shared::barrier
complex(4),device::src(1_8:*)
!dir$ ignore_tkr(r) src
complex(4),shared::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_ldc8(barrier,src,dst,nelems)
integer(8),shared::barrier
complex(8),device::src(1_8:*)
!dir$ ignore_tkr(r) src
complex(8),shared::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_ldi4(barrier,src,dst,nelems)
integer(8),shared::barrier
integer(4),device::src(1_8:*)
!dir$ ignore_tkr(r) src
integer(4),shared::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_ldi8(barrier,src,dst,nelems)
integer(8),shared::barrier
integer(8),device::src(1_8:*)
!dir$ ignore_tkr(r) src
integer(8),shared::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_ldr2(barrier,src,dst,nelems)
integer(8),shared::barrier
real(2),device::src(1_8:*)
!dir$ ignore_tkr(r) src
real(2),shared::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_ldr4(barrier,src,dst,nelems)
integer(8),shared::barrier
real(4),device::src(1_8:*)
!dir$ ignore_tkr(r) src
real(4),shared::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_ldr8(barrier,src,dst,nelems)
integer(8),shared::barrier
real(8),device::src(1_8:*)
!dir$ ignore_tkr(r) src
real(8),shared::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_s2g(src,dst,nbytes)
integer(4),shared::src(1_8:*)
!dir$ ignore_tkr(tkrdm) src
integer(4),device::dst(1_8:*)
!dir$ ignore_tkr(tkrdm) dst
integer(4),value::nbytes
end
end interface
interface
attributes(device) subroutine tma_bulk_store_c4(src,dst,nelems)
complex(4),shared::src(1_8:*)
!dir$ ignore_tkr(r) src
complex(4),device::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_store_c8(src,dst,nelems)
complex(8),shared::src(1_8:*)
!dir$ ignore_tkr(r) src
complex(8),device::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_store_i4(src,dst,nelems)
integer(4),shared::src(1_8:*)
!dir$ ignore_tkr(r) src
integer(4),device::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_store_i8(src,dst,nelems)
integer(8),shared::src(1_8:*)
!dir$ ignore_tkr(r) src
integer(8),device::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_store_r2(src,dst,nelems)
real(2),shared::src(1_8:*)
!dir$ ignore_tkr(r) src
real(2),device::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_store_r4(src,dst,nelems)
real(4),shared::src(1_8:*)
!dir$ ignore_tkr(r) src
real(4),device::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface
attributes(device) subroutine tma_bulk_store_r8(src,dst,nelems)
real(8),shared::src(1_8:*)
!dir$ ignore_tkr(r) src
real(8),device::dst(1_8:*)
!dir$ ignore_tkr(r) dst
integer(4),value::nelems
end
end interface
interface syncthreads
procedure::syncthreads
end interface
interface syncthreads_and
procedure::syncthreads_and_i4
procedure::syncthreads_and_l4
end interface
interface syncthreads_count
procedure::syncthreads_count_i4
procedure::syncthreads_count_l4
end interface
interface syncthreads_or
procedure::syncthreads_or_i4
procedure::syncthreads_or_l4
end interface
interface __fadd_rn
procedure::__fadd_rn
end interface
interface __fadd_rz
procedure::__fadd_rz
end interface
interface __fmul_rn
procedure::__fmul_rn
end interface
interface __fmul_rz
procedure::__fmul_rz
end interface
interface __fmul_ru
procedure::__fmul_ru
end interface
interface __fmul_rd
procedure::__fmul_rd
end interface
interface __fmaf_rn
procedure::__fmaf_rn
end interface
interface __fmaf_rz
procedure::__fmaf_rz
end interface
interface __fmaf_ru
procedure::__fmaf_ru
end interface
interface __fmaf_rd
procedure::__fmaf_rd
end interface
interface __frcp_rn
procedure::__frcp_rn
end interface
interface __frcp_rz
procedure::__frcp_rz
end interface
interface __frcp_ru
procedure::__frcp_ru
end interface
interface __frcp_rd
procedure::__frcp_rd
end interface
interface __fsqrt_rn
procedure::__fsqrt_rn
end interface
interface __fsqrt_rz
procedure::__fsqrt_rz
end interface
interface __fsqrt_ru
procedure::__fsqrt_ru
end interface
interface __fsqrt_rd
procedure::__fsqrt_rd
end interface
interface __fdiv_rn
procedure::__fdiv_rn
end interface
interface __fdiv_rz
procedure::__fdiv_rz
end interface
interface __fdiv_ru
procedure::__fdiv_ru
end interface
interface __fdiv_rd
procedure::__fdiv_rd
end interface
interface __dadd_rn
procedure::__dadd_rn
end interface
interface __dadd_rz
procedure::__dadd_rz
end interface
interface __dadd_ru
procedure::__dadd_ru
end interface
interface __dadd_rd
procedure::__dadd_rd
end interface
interface __dmul_rn
procedure::__dmul_rn
end interface
interface __dmul_rz
procedure::__dmul_rz
end interface
interface __dmul_ru
procedure::__dmul_ru
end interface
interface __dmul_rd
procedure::__dmul_rd
end interface
interface __fma_rn
procedure::__fma_rn
end interface
interface __fma_rz
procedure::__fma_rz
end interface
interface __fma_ru
procedure::__fma_ru
end interface
interface __fma_rd
procedure::__fma_rd
end interface
interface rsqrt
procedure::rsqrtf
procedure::rsqrt
end interface
interface saturate
procedure::__saturatef
end interface
interface __sad
procedure::__sad
end interface
interface __usad
procedure::__usad
end interface
interface signbit
procedure::signbitf
procedure::signbit
end interface
interface sincos
procedure::sincosf
procedure::sincos
end interface
interface sincospi
procedure::sincospif
procedure::sincospi
end interface
interface __exp10f
procedure::__exp10f
end interface
interface __expf
procedure::__expf
end interface
interface __fdividef
procedure::__fdividef
end interface
interface __log10f
procedure::__log10f
end interface
interface __log2f
procedure::__log2f
end interface
interface __logf
procedure::__logf
end interface
interface __sincosf
procedure::__sincosf
end interface
interface __sinf
procedure::__sinf
end interface
interface __tanf
procedure::__tanf
end interface
interface cospi
procedure::cospif
procedure::cospi
end interface
interface sinpi
procedure::sinpif
procedure::sinpi
end interface
interface mulhi
procedure::__mulhi
end interface
interface umulhi
procedure::__umulhi
end interface
interface mul64hi
procedure::__mul64hi
end interface
interface umul64hi
procedure::__umul64hi
end interface
interface int_as_float
procedure::__int_as_float
end interface
interface float_as_int
procedure::__float_as_int
end interface
interface __float2half_rn
procedure::__float2half_rn
end interface
interface __float2int_rd
procedure::__float2int_rd
end interface
interface __float2int_rn
procedure::__float2int_rn
end interface
interface __float2int_ru
procedure::__float2int_ru
end interface
interface __float2int_rz
procedure::__float2int_rz
end interface
interface __float2uint_rd
procedure::__float2uint_rd
end interface
interface __float2uint_rn
procedure::__float2uint_rn
end interface
interface __float2uint_ru
procedure::__float2uint_ru
end interface
interface __float2uint_rz
procedure::__float2uint_rz
end interface
interface __float2ll_rd
procedure::__float2ll_rd
end interface
interface __float2ll_rn
procedure::__float2ll_rn
end interface
interface __float2ll_ru
procedure::__float2ll_ru
end interface
interface __float2ll_rz
procedure::__float2ll_rz
end interface
interface __float2ull_rd
procedure::__float2ull_rd
end interface
interface __float2ull_rn
procedure::__float2ull_rn
end interface
interface __float2ull_ru
procedure::__float2ull_ru
end interface
interface __float2ull_rz
procedure::__float2ull_rz
end interface
interface __half2float
procedure::__half2float
end interface
interface double_as_longlong
procedure::__double_as_longlong
end interface
interface longlong_as_double
procedure::__longlong_as_double
end interface
interface __double2int_rd
procedure::__double2int_rd
end interface
interface __double2int_rn
procedure::__double2int_rn
end interface
interface __double2int_ru
procedure::__double2int_ru
end interface
interface __double2int_rz
procedure::__double2int_rz
end interface
interface __double2uint_rd
procedure::__double2uint_rd
end interface
interface __double2uint_rn
procedure::__double2uint_rn
end interface
interface __double2uint_ru
procedure::__double2uint_ru
end interface
interface __double2uint_rz
procedure::__double2uint_rz
end interface
interface __double2float_rn
procedure::__double2float_rn
end interface
interface __double2float_rz
procedure::__double2float_rz
end interface
interface __double2float_ru
procedure::__double2float_ru
end interface
interface __double2float_rd
procedure::__double2float_rd
end interface
interface __double2loint
procedure::__double2loint
end interface
interface __double2hiint
procedure::__double2hiint
end interface
interface __hiloint2double
procedure::__hiloint2double
end interface
interface __int2float_rd
procedure::__int2float_rd
end interface
interface __int2float_rn
procedure::__int2float_rn
end interface
interface __int2float_ru
procedure::__int2float_ru
end interface
interface __int2float_rz
procedure::__int2float_rz
end interface
interface __int2double_rn
procedure::__int2double_rn
end interface
interface __uint2float_rd
procedure::__uint2float_rd
end interface
interface __uint2float_rn
procedure::__uint2float_rn
end interface
interface __uint2float_ru
procedure::__uint2float_ru
end interface
interface __uint2float_rz
procedure::__uint2float_rz
end interface
interface __uint2double_rn
procedure::__uint2double_rn
end interface
interface __double2ll_rd
procedure::__double2ll_rd
end interface
interface __double2ll_rn
procedure::__double2ll_rn
end interface
interface __double2ll_ru
procedure::__double2ll_ru
end interface
interface __double2ll_rz
procedure::__double2ll_rz
end interface
interface __double2ull_rd
procedure::__double2ull_rd
end interface
interface __double2ull_rn
procedure::__double2ull_rn
end interface
interface __double2ull_ru
procedure::__double2ull_ru
end interface
interface __double2ull_rz
procedure::__double2ull_rz
end interface
interface __ll2float_rd
procedure::__ll2float_rd
end interface
interface __ll2float_rn
procedure::__ll2float_rn
end interface
interface __ll2float_ru
procedure::__ll2float_ru
end interface
interface __ll2float_rz
procedure::__ll2float_rz
end interface
interface __ll2double_rd
procedure::__ll2double_rd
end interface
interface __ll2double_rn
procedure::__ll2double_rn
end interface
interface __ll2double_ru
procedure::__ll2double_ru
end interface
interface __ll2double_rz
procedure::__ll2double_rz
end interface
interface __ull2double_rd
procedure::__ull2double_rd
end interface
interface __ull2double_rn
procedure::__ull2double_rn
end interface
interface __ull2double_ru
procedure::__ull2double_ru
end interface
interface __ull2double_rz
procedure::__ull2double_rz
end interface
interface __ull2float_rd
procedure::__ull2float_rd
end interface
interface __ull2float_rn
procedure::__ull2float_rn
end interface
interface __ull2float_ru
procedure::__ull2float_ru
end interface
interface __ull2float_rz
procedure::__ull2float_rz
end interface
interface __mul24
procedure::__mul24
end interface
interface __umul24
procedure::__umul24
end interface
interface __drcp_rd
procedure::__drcp_rd
end interface
interface __drcp_rn
procedure::__drcp_rn
end interface
interface __drcp_ru
procedure::__drcp_ru
end interface
interface __drcp_rz
procedure::__drcp_rz
end interface
interface __dsqrt_rd
procedure::__dsqrt_rd
end interface
interface __dsqrt_rn
procedure::__dsqrt_rn
end interface
interface __dsqrt_ru
procedure::__dsqrt_ru
end interface
interface __dsqrt_rz
procedure::__dsqrt_rz
end interface
interface __ddiv_rn
procedure::__ddiv_rn
end interface
interface __ddiv_rz
procedure::__ddiv_rz
end interface
interface __ddiv_ru
procedure::__ddiv_ru
end interface
interface __ddiv_rd
procedure::__ddiv_rd
end interface
interface __clz
procedure::__clz
procedure::__clzll
end interface
interface __ffs
procedure::__ffs
procedure::__ffsll
end interface
interface __popc
procedure::__popc
procedure::__popcll
end interface
interface __brev
procedure::__brev
procedure::__brevll
end interface
interface atomicadd
procedure::atomicaddi
procedure::atomicaddf
procedure::atomicaddd
procedure::atomicaddl
procedure::atomicaddr2
end interface
interface atomicaddvector
procedure::atomicaddvector_r2x2
procedure::atomicaddvector_r4x2
end interface
interface atomicaddreal4x2
procedure::atomicadd_r4x2
end interface
interface atomicaddreal4x4
procedure::atomicadd_r4x4
end interface
interface atomicsub
procedure::atomicsubi
procedure::atomicsubf
procedure::atomicsubd
procedure::atomicsubl
end interface
interface atomicmax
procedure::atomicmaxi
procedure::atomicmaxf
procedure::atomicmaxd
procedure::atomicmaxl
end interface
interface atomicmin
procedure::atomicmini
procedure::atomicminf
procedure::atomicmind
procedure::atomicminl
end interface
interface atomicand
procedure::atomicandi
end interface
interface atomicor
procedure::atomicori
end interface
interface atomicinc
procedure::atomicinci
end interface
interface atomicdec
procedure::atomicdeci
end interface
interface atomiccas
procedure::atomiccasi
procedure::atomiccasul
procedure::atomiccasf
procedure::atomiccasd
end interface
interface atomicexch
procedure::atomicexchi
procedure::atomicexchul
procedure::atomicexchf
procedure::atomicexchd
end interface
interface atomicxor
procedure::atomicxori
end interface
interface match_all_sync
procedure::match_all_syncjj
procedure::match_all_syncjx
procedure::match_all_syncjf
procedure::match_all_syncjd
end interface
interface match_any_sync
procedure::match_any_syncjj
procedure::match_any_syncjx
procedure::match_any_syncjf
procedure::match_any_syncjd
end interface
interface all_sync
procedure::all_sync
end interface
interface any_sync
procedure::any_sync
end interface
interface ballot_sync
procedure::ballot_sync
end interface
interface __ldcg
procedure::__ldcg_i4
procedure::__ldcg_i8
procedure::__ldcg_cd
procedure::__ldcg_r2
procedure::__ldcg_r4
procedure::__ldcg_r8
procedure::__ldcg_c4
procedure::__ldcg_c8
procedure::__ldcg_i4x4
procedure::__ldcg_i8x2
procedure::__ldcg_r2x2
procedure::__ldcg_r4x4
procedure::__ldcg_r8x2
end interface
interface __ldca
procedure::__ldca_i4
procedure::__ldca_i8
procedure::__ldca_cd
procedure::__ldca_r2
procedure::__ldca_r4
procedure::__ldca_r8
procedure::__ldca_c4
procedure::__ldca_c8
procedure::__ldca_i4x4
procedure::__ldca_i8x2
procedure::__ldca_r2x2
procedure::__ldca_r4x4
procedure::__ldca_r8x2
end interface
interface __ldcs
procedure::__ldcs_i4
procedure::__ldcs_i8
procedure::__ldcs_cd
procedure::__ldcs_r2
procedure::__ldcs_r4
procedure::__ldcs_r8
procedure::__ldcs_c4
procedure::__ldcs_c8
procedure::__ldcs_i4x4
procedure::__ldcs_i8x2
procedure::__ldcs_r2x2
procedure::__ldcs_r4x4
procedure::__ldcs_r8x2
end interface
interface __ldlu
procedure::__ldlu_i4
procedure::__ldlu_i8
procedure::__ldlu_cd
procedure::__ldlu_r2
procedure::__ldlu_r4
procedure::__ldlu_r8
procedure::__ldlu_c4
procedure::__ldlu_c8
procedure::__ldlu_i4x4
procedure::__ldlu_i8x2
procedure::__ldlu_r2x2
procedure::__ldlu_r4x4
procedure::__ldlu_r8x2
end interface
interface __ldcv
procedure::__ldcv_i4
procedure::__ldcv_i8
procedure::__ldcv_cd
procedure::__ldcv_r2
procedure::__ldcv_r4
procedure::__ldcv_r8
procedure::__ldcv_c4
procedure::__ldcv_c8
procedure::__ldcv_i4x4
procedure::__ldcv_i8x2
procedure::__ldcv_r2x2
procedure::__ldcv_r4x4
procedure::__ldcv_r8x2
end interface
interface __stwb
procedure::__stwb_i4
procedure::__stwb_i8
procedure::__stwb_cd
procedure::__stwb_r2
procedure::__stwb_r4
procedure::__stwb_r8
procedure::__stwb_c4
procedure::__stwb_c8
procedure::__stwb_i4x4
procedure::__stwb_i8x2
procedure::__stwb_r2x2
procedure::__stwb_r4x4
procedure::__stwb_r8x2
end interface
interface __stcg
procedure::__stcg_i4
procedure::__stcg_i8
procedure::__stcg_cd
procedure::__stcg_r2
procedure::__stcg_r4
procedure::__stcg_r8
procedure::__stcg_c4
procedure::__stcg_c8
procedure::__stcg_i4x4
procedure::__stcg_i8x2
procedure::__stcg_r2x2
procedure::__stcg_r4x4
procedure::__stcg_r8x2
end interface
interface __stcs
procedure::__stcs_i4
procedure::__stcs_i8
procedure::__stcs_cd
procedure::__stcs_r2
procedure::__stcs_r4
procedure::__stcs_r8
procedure::__stcs_c4
procedure::__stcs_c8
procedure::__stcs_i4x4
procedure::__stcs_i8x2
procedure::__stcs_r2x2
procedure::__stcs_r4x4
procedure::__stcs_r8x2
end interface
interface __stwt
procedure::__stwt_i4
procedure::__stwt_i8
procedure::__stwt_cd
procedure::__stwt_r2
procedure::__stwt_r4
procedure::__stwt_r8
procedure::__stwt_c4
procedure::__stwt_c8
procedure::__stwt_i4x4
procedure::__stwt_i8x2
procedure::__stwt_r2x2
procedure::__stwt_r4x4
procedure::__stwt_r8x2
end interface
interface barrier_arrive
procedure::barrier_arrive
procedure::barrier_arrive_cnt
end interface
interface tma_bulk_load
procedure::tma_bulk_ldc4
procedure::tma_bulk_ldc8
procedure::tma_bulk_ldi4
procedure::tma_bulk_ldi8
procedure::tma_bulk_ldr2
procedure::tma_bulk_ldr4
procedure::tma_bulk_ldr8
end interface
interface tma_bulk_store
procedure::tma_bulk_store_c4
procedure::tma_bulk_store_c8
procedure::tma_bulk_store_i4
procedure::tma_bulk_store_i8
procedure::tma_bulk_store_r2
procedure::tma_bulk_store_r4
procedure::tma_bulk_store_r8
end interface
contains
attributes(device) subroutine syncthreads()
end
end
