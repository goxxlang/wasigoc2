!mod$ v1 sum:d676699be0556005
module __fortran_builtins
intrinsic::__builtin_c_loc
intrinsic::__builtin_c_devloc
intrinsic::__builtin_c_f_pointer
intrinsic::__builtin_f_c_string
intrinsic::__builtin_c_f_strpointer
intrinsic::__builtin_show_descriptor
intrinsic::sizeof
intrinsic::selected_int_kind
private::selected_int_kind
integer(4),parameter,private::int64=8_4
type,bind(c)::__builtin_c_ptr
integer(8),private::__address
end type
type,bind(c)::__builtin_c_funptr
integer(8),private::__address
end type
type::__builtin_event_type
integer(8),private::__count=-1_8
end type
type::__builtin_notify_type
integer(8),private::__count=-1_8
end type
type::__builtin_lock_type
integer(8),private::__count=-1_8
end type
type::__builtin_ieee_flag_type
integer(1),private::flag=0_1
end type
type(__builtin_ieee_flag_type),parameter::__builtin_ieee_invalid=__builtin_ieee_flag_type(flag=1_1)
type(__builtin_ieee_flag_type),parameter::__builtin_ieee_overflow=__builtin_ieee_flag_type(flag=8_1)
type(__builtin_ieee_flag_type),parameter::__builtin_ieee_divide_by_zero=__builtin_ieee_flag_type(flag=4_1)
type(__builtin_ieee_flag_type),parameter::__builtin_ieee_underflow=__builtin_ieee_flag_type(flag=16_1)
type(__builtin_ieee_flag_type),parameter::__builtin_ieee_inexact=__builtin_ieee_flag_type(flag=32_1)
type(__builtin_ieee_flag_type),parameter::__builtin_ieee_denorm=__builtin_ieee_flag_type(flag=2_1)
type::__builtin_ieee_round_type
integer(1),private::mode=0_1
end type
type(__builtin_ieee_round_type),parameter::__builtin_ieee_to_zero=__builtin_ieee_round_type(mode=0_1)
type(__builtin_ieee_round_type),parameter::__builtin_ieee_nearest=__builtin_ieee_round_type(mode=1_1)
type(__builtin_ieee_round_type),parameter::__builtin_ieee_up=__builtin_ieee_round_type(mode=2_1)
type(__builtin_ieee_round_type),parameter::__builtin_ieee_down=__builtin_ieee_round_type(mode=3_1)
type(__builtin_ieee_round_type),parameter::__builtin_ieee_away=__builtin_ieee_round_type(mode=4_1)
type(__builtin_ieee_round_type),parameter::__builtin_ieee_other=__builtin_ieee_round_type(mode=5_1)
type,private::__builtin_dummy_team_descriptor_type
integer(8),private::__placeholder=-1_8
end type
type::__builtin_team_type
type(__builtin_dummy_team_descriptor_type),pointer,private::info=>NULL()
end type
intrinsic::null
private::null
type,bind(c)::__builtin_prif_coarray_handle_type
type(__builtin_c_ptr),private::info
end type
integer(4),parameter::__builtin_atomic_int_kind=8_4
integer(4),parameter::__builtin_atomic_logical_kind=8_4
type::__builtin_dim3
integer(4)::x=1_4
integer(4)::y=1_4
integer(4)::z=1_4
end type
type(__builtin_dim3)::__builtin_threadidx
type(__builtin_dim3)::__builtin_blockdim
type(__builtin_dim3)::__builtin_blockidx
type(__builtin_dim3)::__builtin_griddim
integer(4),parameter::__builtin_warpsize=32_4
type,bind(c)::__builtin_c_devptr
type(__builtin_c_ptr)::cptr
end type
intrinsic::__builtin_fma
intrinsic::__builtin_ieee_int
intrinsic::__builtin_ieee_is_nan
intrinsic::__builtin_ieee_is_negative
intrinsic::__builtin_ieee_is_normal
intrinsic::__builtin_ieee_next_after
intrinsic::__builtin_ieee_next_down
intrinsic::__builtin_ieee_next_up
intrinsic::scale
intrinsic::__builtin_ieee_real
intrinsic::__builtin_ieee_selected_real_kind
intrinsic::__builtin_ieee_support_datatype
intrinsic::__builtin_ieee_support_denormal
intrinsic::__builtin_ieee_support_divide
intrinsic::__builtin_ieee_support_flag
intrinsic::__builtin_ieee_support_halting
intrinsic::__builtin_ieee_support_inf
intrinsic::__builtin_ieee_support_io
intrinsic::__builtin_ieee_support_nan
intrinsic::__builtin_ieee_support_rounding
intrinsic::__builtin_ieee_support_sqrt
intrinsic::__builtin_ieee_support_standard
intrinsic::__builtin_ieee_support_subnormal
intrinsic::__builtin_ieee_support_underflow_control
type,private::__force_derived_type_instantiations
type(__builtin_c_ptr)::c_ptr
type(__builtin_c_devptr)::c_devptr
type(__builtin_c_funptr)::c_funptr
type(__builtin_event_type)::event_type
type(__builtin_lock_type)::lock_type
type(__builtin_team_type)::team_type
end type
intrinsic::__builtin_compiler_options
intrinsic::__builtin_compiler_version
type(__builtin_c_ptr),parameter::__builtin_c_null_ptr=__builtin_c_ptr(__address=0_8)
type(__builtin_c_devptr),parameter::__builtin_c_null_devptr=__builtin_c_devptr(cptr=__builtin_c_ptr(__address=0_8))
type(__builtin_c_funptr),parameter::__builtin_c_null_funptr=__builtin_c_funptr(__address=0_8)
private::c_associated_c_ptr
private::c_associated_c_funptr
interface operator(==)
procedure::__builtin_c_ptr_eq
procedure::__builtin_c_devptr_eq
end interface
interface operator(/=)
procedure::__builtin_c_ptr_ne
procedure::__builtin_c_devptr_ne
end interface
interface __builtin_c_associated
procedure::c_associated_c_ptr
procedure::c_associated_c_funptr
end interface
contains
elemental function __builtin_c_ptr_eq(x,y)
type(__builtin_c_ptr),intent(in)::x
type(__builtin_c_ptr),intent(in)::y
logical(4)::__builtin_c_ptr_eq
end
elemental function __builtin_c_ptr_ne(x,y)
type(__builtin_c_ptr),intent(in)::x
type(__builtin_c_ptr),intent(in)::y
logical(4)::__builtin_c_ptr_ne
end
elemental function __builtin_c_devptr_eq(x,y)
type(__builtin_c_devptr),intent(in)::x
type(__builtin_c_devptr),intent(in)::y
logical(4)::__builtin_c_devptr_eq
end
elemental function __builtin_c_devptr_ne(x,y)
type(__builtin_c_devptr),intent(in)::x
type(__builtin_c_devptr),intent(in)::y
logical(4)::__builtin_c_devptr_ne
end
pure function __builtin_c_funloc(x)
procedure()::x
type(__builtin_c_funptr)::__builtin_c_funloc
end
pure function c_associated_c_ptr(c_ptr_1,c_ptr_2)
type(__builtin_c_ptr),intent(in)::c_ptr_1
type(__builtin_c_ptr),intent(in),optional::c_ptr_2
logical(4)::c_associated_c_ptr
end
pure function c_associated_c_funptr(c_ptr_1,c_ptr_2)
type(__builtin_c_funptr),intent(in)::c_ptr_1
type(__builtin_c_funptr),intent(in),optional::c_ptr_2
logical(4)::c_associated_c_funptr
end
end
