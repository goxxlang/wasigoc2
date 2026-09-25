!mod$ v1 sum:59dd6e750ee982b7
!need$ d676699be0556005 i __fortran_builtins
module __fortran_type_info
use,intrinsic::__fortran_builtins,only:__builtin_c_ptr
use,intrinsic::__fortran_builtins,only:__builtin_c_devptr
use,intrinsic::__fortran_builtins,only:__builtin_c_funptr
private::__builtin_c_ptr
private::__builtin_c_devptr
private::__builtin_c_funptr
integer(4),parameter,private::int64=8_4
intrinsic::selected_int_kind
private::selected_int_kind
type,private::derivedtype
type(binding),contiguous,pointer::binding(:)
character(:,1),pointer::name
integer(8)::sizeinbytes
type(derivedtype),pointer::uninstantiated
integer(8),contiguous,pointer::kindparameter(:)
integer(1),contiguous,pointer::lenparameterkind(:)
type(component),contiguous,pointer::component(:)
type(procptrcomponent),contiguous,pointer::procptr(:)
type(specialbinding),contiguous,pointer::special(:)
integer(4)::specialbitset
integer(1)::hasparent
integer(1)::noinitializationneeded
integer(1)::nodestructionneeded
integer(1)::nofinalizationneeded
integer(1)::nodefinedassignment
integer(1)::__padding0(1_8:3_8)
end type
type,private::binding
type(__builtin_c_funptr)::proc
character(:,1),pointer::name
end type
integer(4),parameter,private::deferred=1_4
integer(4),parameter,private::explicit=2_4
integer(4),parameter,private::lenparameter=3_4
type,bind(c),private::value
integer(1)::genre
integer(1)::__padding0(1_8:7_8)
integer(8)::value
end type
integer(4),parameter,private::data=1_4
integer(4),parameter,private::pointer=2_4
integer(4),parameter,private::allocatable=3_4
integer(4),parameter,private::automatic=4_4
integer(4),parameter,private::host=0_4
integer(4),parameter,private::device=1_4
integer(4),parameter,private::managed=2_4
integer(4),parameter,private::unified=3_4
integer(4),parameter,private::categoryinteger=0_4
integer(4),parameter,private::categoryreal=1_4
integer(4),parameter,private::categorycomplex=2_4
integer(4),parameter,private::categorycharacter=3_4
integer(4),parameter,private::categorylogical=4_4
integer(4),parameter,private::categoryderived=5_4
type,private::component
character(:,1),pointer::name
integer(1)::genre
integer(1)::category
integer(1)::kind
integer(1)::rank
integer(1)::memoryspace
integer(1)::__padding0(1_8:3_8)
integer(8)::offset
type(value)::characterlen
type(derivedtype),pointer::derived
type(value),contiguous,pointer::lenvalue(:)
type(value),contiguous,pointer::bounds(:,:)
type(__builtin_c_ptr)::initialization
end type
type,private::procptrcomponent
character(:,1),pointer::name
integer(8)::offset
type(__builtin_c_funptr)::initialization
end type
integer(4),parameter,private::scalarassignment=1_4
integer(4),parameter,private::elementalassignment=2_4
integer(4),parameter,private::readformatted=3_4
integer(4),parameter,private::readunformatted=4_4
integer(4),parameter,private::writeformatted=5_4
integer(4),parameter,private::writeunformatted=6_4
integer(4),parameter,private::elementalfinal=7_4
integer(4),parameter,private::assumedrankfinal=8_4
integer(4),parameter,private::scalarfinal=9_4
type,bind(c),private::specialbinding
integer(1)::which
integer(1)::isargdescriptorset
integer(1)::istypebound
integer(1)::specialcaseflag
integer(1)::__padding0(1_8:4_8)
type(__builtin_c_funptr)::proc
end type
end
