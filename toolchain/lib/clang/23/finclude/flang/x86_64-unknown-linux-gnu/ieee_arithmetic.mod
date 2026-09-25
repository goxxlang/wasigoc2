!mod$ v1 sum:f4d850a1bd30892b
!need$ a0a7a405c46ba153 i __fortran_ieee_exceptions
!need$ d676699be0556005 i __fortran_builtins
module ieee_arithmetic
use,intrinsic::__fortran_builtins,only:ieee_away=>__builtin_ieee_away
use,intrinsic::__fortran_builtins,only:ieee_down=>__builtin_ieee_down
use,intrinsic::__fortran_builtins,only:ieee_fma=>__builtin_fma
use,intrinsic::__fortran_builtins,only:ieee_int=>__builtin_ieee_int
use,intrinsic::__fortran_builtins,only:ieee_is_nan=>__builtin_ieee_is_nan
use,intrinsic::__fortran_builtins,only:ieee_is_negative=>__builtin_ieee_is_negative
use,intrinsic::__fortran_builtins,only:ieee_is_normal=>__builtin_ieee_is_normal
use,intrinsic::__fortran_builtins,only:ieee_nearest=>__builtin_ieee_nearest
use,intrinsic::__fortran_builtins,only:ieee_next_after=>__builtin_ieee_next_after
use,intrinsic::__fortran_builtins,only:ieee_next_down=>__builtin_ieee_next_down
use,intrinsic::__fortran_builtins,only:ieee_next_up=>__builtin_ieee_next_up
use,intrinsic::__fortran_builtins,only:ieee_other=>__builtin_ieee_other
use,intrinsic::__fortran_builtins,only:ieee_real=>__builtin_ieee_real
use,intrinsic::__fortran_builtins,only:ieee_round_type=>__builtin_ieee_round_type
use,intrinsic::__fortran_builtins,only:ieee_scalb=>scale
use,intrinsic::__fortran_builtins,only:ieee_selected_real_kind=>__builtin_ieee_selected_real_kind
use,intrinsic::__fortran_builtins,only:ieee_support_datatype=>__builtin_ieee_support_datatype
use,intrinsic::__fortran_builtins,only:ieee_support_denormal=>__builtin_ieee_support_denormal
use,intrinsic::__fortran_builtins,only:ieee_support_divide=>__builtin_ieee_support_divide
use,intrinsic::__fortran_builtins,only:ieee_support_inf=>__builtin_ieee_support_inf
use,intrinsic::__fortran_builtins,only:ieee_support_io=>__builtin_ieee_support_io
use,intrinsic::__fortran_builtins,only:ieee_support_nan=>__builtin_ieee_support_nan
use,intrinsic::__fortran_builtins,only:ieee_support_rounding=>__builtin_ieee_support_rounding
use,intrinsic::__fortran_builtins,only:ieee_support_sqrt=>__builtin_ieee_support_sqrt
use,intrinsic::__fortran_builtins,only:ieee_support_standard=>__builtin_ieee_support_standard
use,intrinsic::__fortran_builtins,only:ieee_support_subnormal=>__builtin_ieee_support_subnormal
use,intrinsic::__fortran_builtins,only:ieee_support_underflow_control=>__builtin_ieee_support_underflow_control
use,intrinsic::__fortran_builtins,only:ieee_to_zero=>__builtin_ieee_to_zero
use,intrinsic::__fortran_builtins,only:ieee_up=>__builtin_ieee_up
use,intrinsic::__fortran_ieee_exceptions,only:ieee_flag_type
use,intrinsic::__fortran_ieee_exceptions,only:ieee_support_flag
use,intrinsic::__fortran_ieee_exceptions,only:ieee_support_halting
use,intrinsic::__fortran_ieee_exceptions,only:ieee_invalid
use,intrinsic::__fortran_ieee_exceptions,only:ieee_overflow
use,intrinsic::__fortran_ieee_exceptions,only:ieee_divide_by_zero
use,intrinsic::__fortran_ieee_exceptions,only:ieee_underflow
use,intrinsic::__fortran_ieee_exceptions,only:ieee_inexact
use,intrinsic::__fortran_ieee_exceptions,only:ieee_denorm
use,intrinsic::__fortran_ieee_exceptions,only:ieee_usual
use,intrinsic::__fortran_ieee_exceptions,only:ieee_all
use,intrinsic::__fortran_ieee_exceptions,only:ieee_modes_type
use,intrinsic::__fortran_ieee_exceptions,only:ieee_status_type
use,intrinsic::__fortran_ieee_exceptions,only:ieee_get_flag
use,intrinsic::__fortran_ieee_exceptions,only:ieee_get_halting_mode
use,intrinsic::__fortran_ieee_exceptions,only:ieee_get_modes
use,intrinsic::__fortran_ieee_exceptions,only:ieee_get_status
use,intrinsic::__fortran_ieee_exceptions,only:ieee_set_flag
use,intrinsic::__fortran_ieee_exceptions,only:ieee_set_halting_mode
use,intrinsic::__fortran_ieee_exceptions,only:ieee_set_modes
use,intrinsic::__fortran_ieee_exceptions,only:ieee_set_status
type::ieee_class_type
integer(1),private::which=0_1
end type
type(ieee_class_type),parameter::ieee_signaling_nan=ieee_class_type(which=1_1)
type(ieee_class_type),parameter::ieee_quiet_nan=ieee_class_type(which=2_1)
type(ieee_class_type),parameter::ieee_negative_inf=ieee_class_type(which=3_1)
type(ieee_class_type),parameter::ieee_negative_normal=ieee_class_type(which=4_1)
type(ieee_class_type),parameter::ieee_negative_subnormal=ieee_class_type(which=5_1)
type(ieee_class_type),parameter::ieee_negative_zero=ieee_class_type(which=6_1)
type(ieee_class_type),parameter::ieee_positive_zero=ieee_class_type(which=7_1)
type(ieee_class_type),parameter::ieee_positive_subnormal=ieee_class_type(which=8_1)
type(ieee_class_type),parameter::ieee_positive_normal=ieee_class_type(which=9_1)
type(ieee_class_type),parameter::ieee_positive_inf=ieee_class_type(which=10_1)
type(ieee_class_type),parameter::ieee_other_value=ieee_class_type(which=11_1)
type(ieee_class_type),parameter::ieee_negative_denormal=ieee_class_type(which=5_1)
type(ieee_class_type),parameter::ieee_positive_denormal=ieee_class_type(which=8_1)
private::ieee_class_eq
interface
elemental function ieee_class_eq(x,y)
import::ieee_class_type
type(ieee_class_type),intent(in)::x
type(ieee_class_type),intent(in)::y
logical(4)::ieee_class_eq
end
end interface
private::ieee_round_eq
interface
elemental function ieee_round_eq(x,y)
import::ieee_round_type
type(ieee_round_type),intent(in)::x
type(ieee_round_type),intent(in)::y
logical(4)::ieee_round_eq
end
end interface
private::ieee_class_ne
interface
elemental function ieee_class_ne(x,y)
import::ieee_class_type
type(ieee_class_type),intent(in)::x
type(ieee_class_type),intent(in)::y
logical(4)::ieee_class_ne
end
end interface
private::ieee_round_ne
interface
elemental function ieee_round_ne(x,y)
import::ieee_round_type
type(ieee_round_type),intent(in)::x
type(ieee_round_type),intent(in)::y
logical(4)::ieee_round_ne
end
end interface
private::ieee_class_a2
interface
elemental function ieee_class_a2(x)
import::ieee_class_type
real(2),intent(in)::x
type(ieee_class_type)::ieee_class_a2
end
end interface
private::ieee_class_a3
interface
elemental function ieee_class_a3(x)
import::ieee_class_type
real(3),intent(in)::x
type(ieee_class_type)::ieee_class_a3
end
end interface
private::ieee_class_a4
interface
elemental function ieee_class_a4(x)
import::ieee_class_type
real(4),intent(in)::x
type(ieee_class_type)::ieee_class_a4
end
end interface
private::ieee_class_a8
interface
elemental function ieee_class_a8(x)
import::ieee_class_type
real(8),intent(in)::x
type(ieee_class_type)::ieee_class_a8
end
end interface
private::ieee_class_a10
interface
elemental function ieee_class_a10(x)
import::ieee_class_type
real(10),intent(in)::x
type(ieee_class_type)::ieee_class_a10
end
end interface
private::ieee_copy_sign_a2_a2
interface
elemental function ieee_copy_sign_a2_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_copy_sign_a2_a2
end
end interface
private::ieee_copy_sign_a2_a3
interface
elemental function ieee_copy_sign_a2_a3(x,y)
real(2),intent(in)::x
real(3),intent(in)::y
real(2)::ieee_copy_sign_a2_a3
end
end interface
private::ieee_copy_sign_a2_a4
interface
elemental function ieee_copy_sign_a2_a4(x,y)
real(2),intent(in)::x
real(4),intent(in)::y
real(2)::ieee_copy_sign_a2_a4
end
end interface
private::ieee_copy_sign_a2_a8
interface
elemental function ieee_copy_sign_a2_a8(x,y)
real(2),intent(in)::x
real(8),intent(in)::y
real(2)::ieee_copy_sign_a2_a8
end
end interface
private::ieee_copy_sign_a2_a10
interface
elemental function ieee_copy_sign_a2_a10(x,y)
real(2),intent(in)::x
real(10),intent(in)::y
real(2)::ieee_copy_sign_a2_a10
end
end interface
private::ieee_copy_sign_a3_a2
interface
elemental function ieee_copy_sign_a3_a2(x,y)
real(3),intent(in)::x
real(2),intent(in)::y
real(3)::ieee_copy_sign_a3_a2
end
end interface
private::ieee_copy_sign_a3_a3
interface
elemental function ieee_copy_sign_a3_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_copy_sign_a3_a3
end
end interface
private::ieee_copy_sign_a3_a4
interface
elemental function ieee_copy_sign_a3_a4(x,y)
real(3),intent(in)::x
real(4),intent(in)::y
real(3)::ieee_copy_sign_a3_a4
end
end interface
private::ieee_copy_sign_a3_a8
interface
elemental function ieee_copy_sign_a3_a8(x,y)
real(3),intent(in)::x
real(8),intent(in)::y
real(3)::ieee_copy_sign_a3_a8
end
end interface
private::ieee_copy_sign_a3_a10
interface
elemental function ieee_copy_sign_a3_a10(x,y)
real(3),intent(in)::x
real(10),intent(in)::y
real(3)::ieee_copy_sign_a3_a10
end
end interface
private::ieee_copy_sign_a4_a2
interface
elemental function ieee_copy_sign_a4_a2(x,y)
real(4),intent(in)::x
real(2),intent(in)::y
real(4)::ieee_copy_sign_a4_a2
end
end interface
private::ieee_copy_sign_a4_a3
interface
elemental function ieee_copy_sign_a4_a3(x,y)
real(4),intent(in)::x
real(3),intent(in)::y
real(4)::ieee_copy_sign_a4_a3
end
end interface
private::ieee_copy_sign_a4_a4
interface
elemental function ieee_copy_sign_a4_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_copy_sign_a4_a4
end
end interface
private::ieee_copy_sign_a4_a8
interface
elemental function ieee_copy_sign_a4_a8(x,y)
real(4),intent(in)::x
real(8),intent(in)::y
real(4)::ieee_copy_sign_a4_a8
end
end interface
private::ieee_copy_sign_a4_a10
interface
elemental function ieee_copy_sign_a4_a10(x,y)
real(4),intent(in)::x
real(10),intent(in)::y
real(4)::ieee_copy_sign_a4_a10
end
end interface
private::ieee_copy_sign_a8_a2
interface
elemental function ieee_copy_sign_a8_a2(x,y)
real(8),intent(in)::x
real(2),intent(in)::y
real(8)::ieee_copy_sign_a8_a2
end
end interface
private::ieee_copy_sign_a8_a3
interface
elemental function ieee_copy_sign_a8_a3(x,y)
real(8),intent(in)::x
real(3),intent(in)::y
real(8)::ieee_copy_sign_a8_a3
end
end interface
private::ieee_copy_sign_a8_a4
interface
elemental function ieee_copy_sign_a8_a4(x,y)
real(8),intent(in)::x
real(4),intent(in)::y
real(8)::ieee_copy_sign_a8_a4
end
end interface
private::ieee_copy_sign_a8_a8
interface
elemental function ieee_copy_sign_a8_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_copy_sign_a8_a8
end
end interface
private::ieee_copy_sign_a8_a10
interface
elemental function ieee_copy_sign_a8_a10(x,y)
real(8),intent(in)::x
real(10),intent(in)::y
real(8)::ieee_copy_sign_a8_a10
end
end interface
private::ieee_copy_sign_a10_a2
interface
elemental function ieee_copy_sign_a10_a2(x,y)
real(10),intent(in)::x
real(2),intent(in)::y
real(10)::ieee_copy_sign_a10_a2
end
end interface
private::ieee_copy_sign_a10_a3
interface
elemental function ieee_copy_sign_a10_a3(x,y)
real(10),intent(in)::x
real(3),intent(in)::y
real(10)::ieee_copy_sign_a10_a3
end
end interface
private::ieee_copy_sign_a10_a4
interface
elemental function ieee_copy_sign_a10_a4(x,y)
real(10),intent(in)::x
real(4),intent(in)::y
real(10)::ieee_copy_sign_a10_a4
end
end interface
private::ieee_copy_sign_a10_a8
interface
elemental function ieee_copy_sign_a10_a8(x,y)
real(10),intent(in)::x
real(8),intent(in)::y
real(10)::ieee_copy_sign_a10_a8
end
end interface
private::ieee_copy_sign_a10_a10
interface
elemental function ieee_copy_sign_a10_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_copy_sign_a10_a10
end
end interface
private::ieee_get_rounding_mode_0
interface
subroutine ieee_get_rounding_mode_0(round_value)
import::ieee_round_type
type(ieee_round_type),intent(out)::round_value
end
end interface
private::ieee_get_rounding_mode_i1
interface
subroutine ieee_get_rounding_mode_i1(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(out)::round_value
integer(1),intent(in)::radix
end
end interface
private::ieee_get_rounding_mode_i2
interface
subroutine ieee_get_rounding_mode_i2(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(out)::round_value
integer(2),intent(in)::radix
end
end interface
private::ieee_get_rounding_mode_i4
interface
subroutine ieee_get_rounding_mode_i4(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(out)::round_value
integer(4),intent(in)::radix
end
end interface
private::ieee_get_rounding_mode_i8
interface
subroutine ieee_get_rounding_mode_i8(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(out)::round_value
integer(8),intent(in)::radix
end
end interface
private::ieee_get_rounding_mode_i16
interface
subroutine ieee_get_rounding_mode_i16(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(out)::round_value
integer(16),intent(in)::radix
end
end interface
private::ieee_get_underflow_mode_l1
interface
subroutine ieee_get_underflow_mode_l1(gradual)
logical(1),intent(out)::gradual
end
end interface
private::ieee_get_underflow_mode_l2
interface
subroutine ieee_get_underflow_mode_l2(gradual)
logical(2),intent(out)::gradual
end
end interface
private::ieee_get_underflow_mode_l4
interface
subroutine ieee_get_underflow_mode_l4(gradual)
logical(4),intent(out)::gradual
end
end interface
private::ieee_get_underflow_mode_l8
interface
subroutine ieee_get_underflow_mode_l8(gradual)
logical(8),intent(out)::gradual
end
end interface
private::ieee_is_finite_a2
interface
elemental function ieee_is_finite_a2(x)
real(2),intent(in)::x
!dir$ ignore_tkr(d) x
logical(4)::ieee_is_finite_a2
end
end interface
private::ieee_is_finite_a3
interface
elemental function ieee_is_finite_a3(x)
real(3),intent(in)::x
!dir$ ignore_tkr(d) x
logical(4)::ieee_is_finite_a3
end
end interface
private::ieee_is_finite_a4
interface
elemental function ieee_is_finite_a4(x)
real(4),intent(in)::x
!dir$ ignore_tkr(d) x
logical(4)::ieee_is_finite_a4
end
end interface
private::ieee_is_finite_a8
interface
elemental function ieee_is_finite_a8(x)
real(8),intent(in)::x
!dir$ ignore_tkr(d) x
logical(4)::ieee_is_finite_a8
end
end interface
private::ieee_is_finite_a10
interface
elemental function ieee_is_finite_a10(x)
real(10),intent(in)::x
!dir$ ignore_tkr(d) x
logical(4)::ieee_is_finite_a10
end
end interface
private::ieee_logb_a2
interface
elemental function ieee_logb_a2(x)
real(2),intent(in)::x
real(2)::ieee_logb_a2
end
end interface
private::ieee_logb_a3
interface
elemental function ieee_logb_a3(x)
real(3),intent(in)::x
real(3)::ieee_logb_a3
end
end interface
private::ieee_logb_a4
interface
elemental function ieee_logb_a4(x)
real(4),intent(in)::x
real(4)::ieee_logb_a4
end
end interface
private::ieee_logb_a8
interface
elemental function ieee_logb_a8(x)
real(8),intent(in)::x
real(8)::ieee_logb_a8
end
end interface
private::ieee_logb_a10
interface
elemental function ieee_logb_a10(x)
real(10),intent(in)::x
real(10)::ieee_logb_a10
end
end interface
private::ieee_max_a2
interface
elemental function ieee_max_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_max_a2
end
end interface
private::ieee_max_a3
interface
elemental function ieee_max_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_max_a3
end
end interface
private::ieee_max_a4
interface
elemental function ieee_max_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_max_a4
end
end interface
private::ieee_max_a8
interface
elemental function ieee_max_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_max_a8
end
end interface
private::ieee_max_a10
interface
elemental function ieee_max_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_max_a10
end
end interface
private::ieee_max_mag_a2
interface
elemental function ieee_max_mag_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_max_mag_a2
end
end interface
private::ieee_max_mag_a3
interface
elemental function ieee_max_mag_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_max_mag_a3
end
end interface
private::ieee_max_mag_a4
interface
elemental function ieee_max_mag_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_max_mag_a4
end
end interface
private::ieee_max_mag_a8
interface
elemental function ieee_max_mag_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_max_mag_a8
end
end interface
private::ieee_max_mag_a10
interface
elemental function ieee_max_mag_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_max_mag_a10
end
end interface
private::ieee_max_num_a2
interface
elemental function ieee_max_num_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_max_num_a2
end
end interface
private::ieee_max_num_a3
interface
elemental function ieee_max_num_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_max_num_a3
end
end interface
private::ieee_max_num_a4
interface
elemental function ieee_max_num_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_max_num_a4
end
end interface
private::ieee_max_num_a8
interface
elemental function ieee_max_num_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_max_num_a8
end
end interface
private::ieee_max_num_a10
interface
elemental function ieee_max_num_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_max_num_a10
end
end interface
private::ieee_max_num_mag_a2
interface
elemental function ieee_max_num_mag_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_max_num_mag_a2
end
end interface
private::ieee_max_num_mag_a3
interface
elemental function ieee_max_num_mag_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_max_num_mag_a3
end
end interface
private::ieee_max_num_mag_a4
interface
elemental function ieee_max_num_mag_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_max_num_mag_a4
end
end interface
private::ieee_max_num_mag_a8
interface
elemental function ieee_max_num_mag_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_max_num_mag_a8
end
end interface
private::ieee_max_num_mag_a10
interface
elemental function ieee_max_num_mag_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_max_num_mag_a10
end
end interface
private::ieee_min_a2
interface
elemental function ieee_min_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_min_a2
end
end interface
private::ieee_min_a3
interface
elemental function ieee_min_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_min_a3
end
end interface
private::ieee_min_a4
interface
elemental function ieee_min_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_min_a4
end
end interface
private::ieee_min_a8
interface
elemental function ieee_min_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_min_a8
end
end interface
private::ieee_min_a10
interface
elemental function ieee_min_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_min_a10
end
end interface
private::ieee_min_mag_a2
interface
elemental function ieee_min_mag_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_min_mag_a2
end
end interface
private::ieee_min_mag_a3
interface
elemental function ieee_min_mag_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_min_mag_a3
end
end interface
private::ieee_min_mag_a4
interface
elemental function ieee_min_mag_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_min_mag_a4
end
end interface
private::ieee_min_mag_a8
interface
elemental function ieee_min_mag_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_min_mag_a8
end
end interface
private::ieee_min_mag_a10
interface
elemental function ieee_min_mag_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_min_mag_a10
end
end interface
private::ieee_min_num_a2
interface
elemental function ieee_min_num_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_min_num_a2
end
end interface
private::ieee_min_num_a3
interface
elemental function ieee_min_num_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_min_num_a3
end
end interface
private::ieee_min_num_a4
interface
elemental function ieee_min_num_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_min_num_a4
end
end interface
private::ieee_min_num_a8
interface
elemental function ieee_min_num_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_min_num_a8
end
end interface
private::ieee_min_num_a10
interface
elemental function ieee_min_num_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_min_num_a10
end
end interface
private::ieee_min_num_mag_a2
interface
elemental function ieee_min_num_mag_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_min_num_mag_a2
end
end interface
private::ieee_min_num_mag_a3
interface
elemental function ieee_min_num_mag_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_min_num_mag_a3
end
end interface
private::ieee_min_num_mag_a4
interface
elemental function ieee_min_num_mag_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_min_num_mag_a4
end
end interface
private::ieee_min_num_mag_a8
interface
elemental function ieee_min_num_mag_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_min_num_mag_a8
end
end interface
private::ieee_min_num_mag_a10
interface
elemental function ieee_min_num_mag_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_min_num_mag_a10
end
end interface
private::ieee_quiet_eq_a2
interface
elemental function ieee_quiet_eq_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_quiet_eq_a2
end
end interface
private::ieee_quiet_eq_a3
interface
elemental function ieee_quiet_eq_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_quiet_eq_a3
end
end interface
private::ieee_quiet_eq_a4
interface
elemental function ieee_quiet_eq_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_quiet_eq_a4
end
end interface
private::ieee_quiet_eq_a8
interface
elemental function ieee_quiet_eq_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_quiet_eq_a8
end
end interface
private::ieee_quiet_eq_a10
interface
elemental function ieee_quiet_eq_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_quiet_eq_a10
end
end interface
private::ieee_quiet_ge_a2
interface
elemental function ieee_quiet_ge_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_quiet_ge_a2
end
end interface
private::ieee_quiet_ge_a3
interface
elemental function ieee_quiet_ge_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_quiet_ge_a3
end
end interface
private::ieee_quiet_ge_a4
interface
elemental function ieee_quiet_ge_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_quiet_ge_a4
end
end interface
private::ieee_quiet_ge_a8
interface
elemental function ieee_quiet_ge_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_quiet_ge_a8
end
end interface
private::ieee_quiet_ge_a10
interface
elemental function ieee_quiet_ge_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_quiet_ge_a10
end
end interface
private::ieee_quiet_gt_a2
interface
elemental function ieee_quiet_gt_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_quiet_gt_a2
end
end interface
private::ieee_quiet_gt_a3
interface
elemental function ieee_quiet_gt_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_quiet_gt_a3
end
end interface
private::ieee_quiet_gt_a4
interface
elemental function ieee_quiet_gt_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_quiet_gt_a4
end
end interface
private::ieee_quiet_gt_a8
interface
elemental function ieee_quiet_gt_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_quiet_gt_a8
end
end interface
private::ieee_quiet_gt_a10
interface
elemental function ieee_quiet_gt_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_quiet_gt_a10
end
end interface
private::ieee_quiet_le_a2
interface
elemental function ieee_quiet_le_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_quiet_le_a2
end
end interface
private::ieee_quiet_le_a3
interface
elemental function ieee_quiet_le_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_quiet_le_a3
end
end interface
private::ieee_quiet_le_a4
interface
elemental function ieee_quiet_le_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_quiet_le_a4
end
end interface
private::ieee_quiet_le_a8
interface
elemental function ieee_quiet_le_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_quiet_le_a8
end
end interface
private::ieee_quiet_le_a10
interface
elemental function ieee_quiet_le_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_quiet_le_a10
end
end interface
private::ieee_quiet_lt_a2
interface
elemental function ieee_quiet_lt_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_quiet_lt_a2
end
end interface
private::ieee_quiet_lt_a3
interface
elemental function ieee_quiet_lt_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_quiet_lt_a3
end
end interface
private::ieee_quiet_lt_a4
interface
elemental function ieee_quiet_lt_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_quiet_lt_a4
end
end interface
private::ieee_quiet_lt_a8
interface
elemental function ieee_quiet_lt_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_quiet_lt_a8
end
end interface
private::ieee_quiet_lt_a10
interface
elemental function ieee_quiet_lt_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_quiet_lt_a10
end
end interface
private::ieee_quiet_ne_a2
interface
elemental function ieee_quiet_ne_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_quiet_ne_a2
end
end interface
private::ieee_quiet_ne_a3
interface
elemental function ieee_quiet_ne_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_quiet_ne_a3
end
end interface
private::ieee_quiet_ne_a4
interface
elemental function ieee_quiet_ne_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_quiet_ne_a4
end
end interface
private::ieee_quiet_ne_a8
interface
elemental function ieee_quiet_ne_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_quiet_ne_a8
end
end interface
private::ieee_quiet_ne_a10
interface
elemental function ieee_quiet_ne_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_quiet_ne_a10
end
end interface
private::ieee_rem_a2_a2
interface
elemental function ieee_rem_a2_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_rem_a2_a2
end
end interface
private::ieee_rem_a2_a3
interface
elemental function ieee_rem_a2_a3(x,y)
real(2),intent(in)::x
real(3),intent(in)::y
real(2)::ieee_rem_a2_a3
end
end interface
private::ieee_rem_a2_a4
interface
elemental function ieee_rem_a2_a4(x,y)
real(2),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_rem_a2_a4
end
end interface
private::ieee_rem_a2_a8
interface
elemental function ieee_rem_a2_a8(x,y)
real(2),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_rem_a2_a8
end
end interface
private::ieee_rem_a2_a10
interface
elemental function ieee_rem_a2_a10(x,y)
real(2),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_rem_a2_a10
end
end interface
private::ieee_rem_a3_a2
interface
elemental function ieee_rem_a3_a2(x,y)
real(3),intent(in)::x
real(2),intent(in)::y
real(2)::ieee_rem_a3_a2
end
end interface
private::ieee_rem_a3_a3
interface
elemental function ieee_rem_a3_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
real(3)::ieee_rem_a3_a3
end
end interface
private::ieee_rem_a3_a4
interface
elemental function ieee_rem_a3_a4(x,y)
real(3),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_rem_a3_a4
end
end interface
private::ieee_rem_a3_a8
interface
elemental function ieee_rem_a3_a8(x,y)
real(3),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_rem_a3_a8
end
end interface
private::ieee_rem_a3_a10
interface
elemental function ieee_rem_a3_a10(x,y)
real(3),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_rem_a3_a10
end
end interface
private::ieee_rem_a4_a2
interface
elemental function ieee_rem_a4_a2(x,y)
real(4),intent(in)::x
real(2),intent(in)::y
real(4)::ieee_rem_a4_a2
end
end interface
private::ieee_rem_a4_a3
interface
elemental function ieee_rem_a4_a3(x,y)
real(4),intent(in)::x
real(3),intent(in)::y
real(4)::ieee_rem_a4_a3
end
end interface
private::ieee_rem_a4_a4
interface
elemental function ieee_rem_a4_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
real(4)::ieee_rem_a4_a4
end
end interface
private::ieee_rem_a4_a8
interface
elemental function ieee_rem_a4_a8(x,y)
real(4),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_rem_a4_a8
end
end interface
private::ieee_rem_a4_a10
interface
elemental function ieee_rem_a4_a10(x,y)
real(4),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_rem_a4_a10
end
end interface
private::ieee_rem_a8_a2
interface
elemental function ieee_rem_a8_a2(x,y)
real(8),intent(in)::x
real(2),intent(in)::y
real(8)::ieee_rem_a8_a2
end
end interface
private::ieee_rem_a8_a3
interface
elemental function ieee_rem_a8_a3(x,y)
real(8),intent(in)::x
real(3),intent(in)::y
real(8)::ieee_rem_a8_a3
end
end interface
private::ieee_rem_a8_a4
interface
elemental function ieee_rem_a8_a4(x,y)
real(8),intent(in)::x
real(4),intent(in)::y
real(8)::ieee_rem_a8_a4
end
end interface
private::ieee_rem_a8_a8
interface
elemental function ieee_rem_a8_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
real(8)::ieee_rem_a8_a8
end
end interface
private::ieee_rem_a8_a10
interface
elemental function ieee_rem_a8_a10(x,y)
real(8),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_rem_a8_a10
end
end interface
private::ieee_rem_a10_a2
interface
elemental function ieee_rem_a10_a2(x,y)
real(10),intent(in)::x
real(2),intent(in)::y
real(10)::ieee_rem_a10_a2
end
end interface
private::ieee_rem_a10_a3
interface
elemental function ieee_rem_a10_a3(x,y)
real(10),intent(in)::x
real(3),intent(in)::y
real(10)::ieee_rem_a10_a3
end
end interface
private::ieee_rem_a10_a4
interface
elemental function ieee_rem_a10_a4(x,y)
real(10),intent(in)::x
real(4),intent(in)::y
real(10)::ieee_rem_a10_a4
end
end interface
private::ieee_rem_a10_a8
interface
elemental function ieee_rem_a10_a8(x,y)
real(10),intent(in)::x
real(8),intent(in)::y
real(10)::ieee_rem_a10_a8
end
end interface
private::ieee_rem_a10_a10
interface
elemental function ieee_rem_a10_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
real(10)::ieee_rem_a10_a10
end
end interface
private::ieee_rint_a2
interface
elemental function ieee_rint_a2(x,round)
import::ieee_round_type
real(2),intent(in)::x
type(ieee_round_type),intent(in),optional::round
real(2)::ieee_rint_a2
end
end interface
private::ieee_rint_a3
interface
elemental function ieee_rint_a3(x,round)
import::ieee_round_type
real(3),intent(in)::x
type(ieee_round_type),intent(in),optional::round
real(3)::ieee_rint_a3
end
end interface
private::ieee_rint_a4
interface
elemental function ieee_rint_a4(x,round)
import::ieee_round_type
real(4),intent(in)::x
type(ieee_round_type),intent(in),optional::round
real(4)::ieee_rint_a4
end
end interface
private::ieee_rint_a8
interface
elemental function ieee_rint_a8(x,round)
import::ieee_round_type
real(8),intent(in)::x
type(ieee_round_type),intent(in),optional::round
real(8)::ieee_rint_a8
end
end interface
private::ieee_rint_a10
interface
elemental function ieee_rint_a10(x,round)
import::ieee_round_type
real(10),intent(in)::x
type(ieee_round_type),intent(in),optional::round
real(10)::ieee_rint_a10
end
end interface
private::ieee_set_rounding_mode_0
interface
subroutine ieee_set_rounding_mode_0(round_value)
import::ieee_round_type
type(ieee_round_type),intent(in)::round_value
end
end interface
private::ieee_set_rounding_mode_i1
interface
subroutine ieee_set_rounding_mode_i1(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(in)::round_value
integer(1),intent(in)::radix
end
end interface
private::ieee_set_rounding_mode_i2
interface
subroutine ieee_set_rounding_mode_i2(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(in)::round_value
integer(2),intent(in)::radix
end
end interface
private::ieee_set_rounding_mode_i4
interface
subroutine ieee_set_rounding_mode_i4(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(in)::round_value
integer(4),intent(in)::radix
end
end interface
private::ieee_set_rounding_mode_i8
interface
subroutine ieee_set_rounding_mode_i8(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(in)::round_value
integer(8),intent(in)::radix
end
end interface
private::ieee_set_rounding_mode_i16
interface
subroutine ieee_set_rounding_mode_i16(round_value,radix)
import::ieee_round_type
type(ieee_round_type),intent(in)::round_value
integer(16),intent(in)::radix
end
end interface
private::ieee_set_underflow_mode_l1
interface
subroutine ieee_set_underflow_mode_l1(gradual)
logical(1),intent(in)::gradual
end
end interface
private::ieee_set_underflow_mode_l2
interface
subroutine ieee_set_underflow_mode_l2(gradual)
logical(2),intent(in)::gradual
end
end interface
private::ieee_set_underflow_mode_l4
interface
subroutine ieee_set_underflow_mode_l4(gradual)
logical(4),intent(in)::gradual
end
end interface
private::ieee_set_underflow_mode_l8
interface
subroutine ieee_set_underflow_mode_l8(gradual)
logical(8),intent(in)::gradual
end
end interface
private::ieee_signaling_eq_a2
interface
elemental function ieee_signaling_eq_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_signaling_eq_a2
end
end interface
private::ieee_signaling_eq_a3
interface
elemental function ieee_signaling_eq_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_signaling_eq_a3
end
end interface
private::ieee_signaling_eq_a4
interface
elemental function ieee_signaling_eq_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_signaling_eq_a4
end
end interface
private::ieee_signaling_eq_a8
interface
elemental function ieee_signaling_eq_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_signaling_eq_a8
end
end interface
private::ieee_signaling_eq_a10
interface
elemental function ieee_signaling_eq_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_signaling_eq_a10
end
end interface
private::ieee_signaling_ge_a2
interface
elemental function ieee_signaling_ge_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_signaling_ge_a2
end
end interface
private::ieee_signaling_ge_a3
interface
elemental function ieee_signaling_ge_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_signaling_ge_a3
end
end interface
private::ieee_signaling_ge_a4
interface
elemental function ieee_signaling_ge_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_signaling_ge_a4
end
end interface
private::ieee_signaling_ge_a8
interface
elemental function ieee_signaling_ge_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_signaling_ge_a8
end
end interface
private::ieee_signaling_ge_a10
interface
elemental function ieee_signaling_ge_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_signaling_ge_a10
end
end interface
private::ieee_signaling_gt_a2
interface
elemental function ieee_signaling_gt_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_signaling_gt_a2
end
end interface
private::ieee_signaling_gt_a3
interface
elemental function ieee_signaling_gt_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_signaling_gt_a3
end
end interface
private::ieee_signaling_gt_a4
interface
elemental function ieee_signaling_gt_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_signaling_gt_a4
end
end interface
private::ieee_signaling_gt_a8
interface
elemental function ieee_signaling_gt_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_signaling_gt_a8
end
end interface
private::ieee_signaling_gt_a10
interface
elemental function ieee_signaling_gt_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_signaling_gt_a10
end
end interface
private::ieee_signaling_le_a2
interface
elemental function ieee_signaling_le_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_signaling_le_a2
end
end interface
private::ieee_signaling_le_a3
interface
elemental function ieee_signaling_le_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_signaling_le_a3
end
end interface
private::ieee_signaling_le_a4
interface
elemental function ieee_signaling_le_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_signaling_le_a4
end
end interface
private::ieee_signaling_le_a8
interface
elemental function ieee_signaling_le_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_signaling_le_a8
end
end interface
private::ieee_signaling_le_a10
interface
elemental function ieee_signaling_le_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_signaling_le_a10
end
end interface
private::ieee_signaling_lt_a2
interface
elemental function ieee_signaling_lt_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_signaling_lt_a2
end
end interface
private::ieee_signaling_lt_a3
interface
elemental function ieee_signaling_lt_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_signaling_lt_a3
end
end interface
private::ieee_signaling_lt_a4
interface
elemental function ieee_signaling_lt_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_signaling_lt_a4
end
end interface
private::ieee_signaling_lt_a8
interface
elemental function ieee_signaling_lt_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_signaling_lt_a8
end
end interface
private::ieee_signaling_lt_a10
interface
elemental function ieee_signaling_lt_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_signaling_lt_a10
end
end interface
private::ieee_signaling_ne_a2
interface
elemental function ieee_signaling_ne_a2(a,b)
real(2),intent(in)::a
real(2),intent(in)::b
logical(4)::ieee_signaling_ne_a2
end
end interface
private::ieee_signaling_ne_a3
interface
elemental function ieee_signaling_ne_a3(a,b)
real(3),intent(in)::a
real(3),intent(in)::b
logical(4)::ieee_signaling_ne_a3
end
end interface
private::ieee_signaling_ne_a4
interface
elemental function ieee_signaling_ne_a4(a,b)
real(4),intent(in)::a
real(4),intent(in)::b
logical(4)::ieee_signaling_ne_a4
end
end interface
private::ieee_signaling_ne_a8
interface
elemental function ieee_signaling_ne_a8(a,b)
real(8),intent(in)::a
real(8),intent(in)::b
logical(4)::ieee_signaling_ne_a8
end
end interface
private::ieee_signaling_ne_a10
interface
elemental function ieee_signaling_ne_a10(a,b)
real(10),intent(in)::a
real(10),intent(in)::b
logical(4)::ieee_signaling_ne_a10
end
end interface
private::ieee_signbit_a2
interface
elemental function ieee_signbit_a2(x)
real(2),intent(in)::x
logical(4)::ieee_signbit_a2
end
end interface
private::ieee_signbit_a3
interface
elemental function ieee_signbit_a3(x)
real(3),intent(in)::x
logical(4)::ieee_signbit_a3
end
end interface
private::ieee_signbit_a4
interface
elemental function ieee_signbit_a4(x)
real(4),intent(in)::x
logical(4)::ieee_signbit_a4
end
end interface
private::ieee_signbit_a8
interface
elemental function ieee_signbit_a8(x)
real(8),intent(in)::x
logical(4)::ieee_signbit_a8
end
end interface
private::ieee_signbit_a10
interface
elemental function ieee_signbit_a10(x)
real(10),intent(in)::x
logical(4)::ieee_signbit_a10
end
end interface
private::ieee_unordered_a2_a2
interface
elemental function ieee_unordered_a2_a2(x,y)
real(2),intent(in)::x
real(2),intent(in)::y
logical(4)::ieee_unordered_a2_a2
end
end interface
private::ieee_unordered_a2_a3
interface
elemental function ieee_unordered_a2_a3(x,y)
real(2),intent(in)::x
real(3),intent(in)::y
logical(4)::ieee_unordered_a2_a3
end
end interface
private::ieee_unordered_a2_a4
interface
elemental function ieee_unordered_a2_a4(x,y)
real(2),intent(in)::x
real(4),intent(in)::y
logical(4)::ieee_unordered_a2_a4
end
end interface
private::ieee_unordered_a2_a8
interface
elemental function ieee_unordered_a2_a8(x,y)
real(2),intent(in)::x
real(8),intent(in)::y
logical(4)::ieee_unordered_a2_a8
end
end interface
private::ieee_unordered_a2_a10
interface
elemental function ieee_unordered_a2_a10(x,y)
real(2),intent(in)::x
real(10),intent(in)::y
logical(4)::ieee_unordered_a2_a10
end
end interface
private::ieee_unordered_a3_a2
interface
elemental function ieee_unordered_a3_a2(x,y)
real(3),intent(in)::x
real(2),intent(in)::y
logical(4)::ieee_unordered_a3_a2
end
end interface
private::ieee_unordered_a3_a3
interface
elemental function ieee_unordered_a3_a3(x,y)
real(3),intent(in)::x
real(3),intent(in)::y
logical(4)::ieee_unordered_a3_a3
end
end interface
private::ieee_unordered_a3_a4
interface
elemental function ieee_unordered_a3_a4(x,y)
real(3),intent(in)::x
real(4),intent(in)::y
logical(4)::ieee_unordered_a3_a4
end
end interface
private::ieee_unordered_a3_a8
interface
elemental function ieee_unordered_a3_a8(x,y)
real(3),intent(in)::x
real(8),intent(in)::y
logical(4)::ieee_unordered_a3_a8
end
end interface
private::ieee_unordered_a3_a10
interface
elemental function ieee_unordered_a3_a10(x,y)
real(3),intent(in)::x
real(10),intent(in)::y
logical(4)::ieee_unordered_a3_a10
end
end interface
private::ieee_unordered_a4_a2
interface
elemental function ieee_unordered_a4_a2(x,y)
real(4),intent(in)::x
real(2),intent(in)::y
logical(4)::ieee_unordered_a4_a2
end
end interface
private::ieee_unordered_a4_a3
interface
elemental function ieee_unordered_a4_a3(x,y)
real(4),intent(in)::x
real(3),intent(in)::y
logical(4)::ieee_unordered_a4_a3
end
end interface
private::ieee_unordered_a4_a4
interface
elemental function ieee_unordered_a4_a4(x,y)
real(4),intent(in)::x
real(4),intent(in)::y
logical(4)::ieee_unordered_a4_a4
end
end interface
private::ieee_unordered_a4_a8
interface
elemental function ieee_unordered_a4_a8(x,y)
real(4),intent(in)::x
real(8),intent(in)::y
logical(4)::ieee_unordered_a4_a8
end
end interface
private::ieee_unordered_a4_a10
interface
elemental function ieee_unordered_a4_a10(x,y)
real(4),intent(in)::x
real(10),intent(in)::y
logical(4)::ieee_unordered_a4_a10
end
end interface
private::ieee_unordered_a8_a2
interface
elemental function ieee_unordered_a8_a2(x,y)
real(8),intent(in)::x
real(2),intent(in)::y
logical(4)::ieee_unordered_a8_a2
end
end interface
private::ieee_unordered_a8_a3
interface
elemental function ieee_unordered_a8_a3(x,y)
real(8),intent(in)::x
real(3),intent(in)::y
logical(4)::ieee_unordered_a8_a3
end
end interface
private::ieee_unordered_a8_a4
interface
elemental function ieee_unordered_a8_a4(x,y)
real(8),intent(in)::x
real(4),intent(in)::y
logical(4)::ieee_unordered_a8_a4
end
end interface
private::ieee_unordered_a8_a8
interface
elemental function ieee_unordered_a8_a8(x,y)
real(8),intent(in)::x
real(8),intent(in)::y
logical(4)::ieee_unordered_a8_a8
end
end interface
private::ieee_unordered_a8_a10
interface
elemental function ieee_unordered_a8_a10(x,y)
real(8),intent(in)::x
real(10),intent(in)::y
logical(4)::ieee_unordered_a8_a10
end
end interface
private::ieee_unordered_a10_a2
interface
elemental function ieee_unordered_a10_a2(x,y)
real(10),intent(in)::x
real(2),intent(in)::y
logical(4)::ieee_unordered_a10_a2
end
end interface
private::ieee_unordered_a10_a3
interface
elemental function ieee_unordered_a10_a3(x,y)
real(10),intent(in)::x
real(3),intent(in)::y
logical(4)::ieee_unordered_a10_a3
end
end interface
private::ieee_unordered_a10_a4
interface
elemental function ieee_unordered_a10_a4(x,y)
real(10),intent(in)::x
real(4),intent(in)::y
logical(4)::ieee_unordered_a10_a4
end
end interface
private::ieee_unordered_a10_a8
interface
elemental function ieee_unordered_a10_a8(x,y)
real(10),intent(in)::x
real(8),intent(in)::y
logical(4)::ieee_unordered_a10_a8
end
end interface
private::ieee_unordered_a10_a10
interface
elemental function ieee_unordered_a10_a10(x,y)
real(10),intent(in)::x
real(10),intent(in)::y
logical(4)::ieee_unordered_a10_a10
end
end interface
private::ieee_value_a2
interface
elemental function ieee_value_a2(x,class)
import::ieee_class_type
real(2),intent(in)::x
type(ieee_class_type),intent(in)::class
real(2)::ieee_value_a2
end
end interface
private::ieee_value_a3
interface
elemental function ieee_value_a3(x,class)
import::ieee_class_type
real(3),intent(in)::x
type(ieee_class_type),intent(in)::class
real(3)::ieee_value_a3
end
end interface
private::ieee_value_a4
interface
elemental function ieee_value_a4(x,class)
import::ieee_class_type
real(4),intent(in)::x
type(ieee_class_type),intent(in)::class
real(4)::ieee_value_a4
end
end interface
private::ieee_value_a8
interface
elemental function ieee_value_a8(x,class)
import::ieee_class_type
real(8),intent(in)::x
type(ieee_class_type),intent(in)::class
real(8)::ieee_value_a8
end
end interface
private::ieee_value_a10
interface
elemental function ieee_value_a10(x,class)
import::ieee_class_type
real(10),intent(in)::x
type(ieee_class_type),intent(in)::class
real(10)::ieee_value_a10
end
end interface
interface operator(==)
procedure::ieee_class_eq
procedure::ieee_round_eq
end interface
interface operator(/=)
procedure::ieee_class_ne
procedure::ieee_round_ne
end interface
interface ieee_class
procedure::ieee_class_a2
procedure::ieee_class_a3
procedure::ieee_class_a4
procedure::ieee_class_a8
procedure::ieee_class_a10
end interface
interface ieee_copy_sign
procedure::ieee_copy_sign_a2_a2
procedure::ieee_copy_sign_a2_a3
procedure::ieee_copy_sign_a2_a4
procedure::ieee_copy_sign_a2_a8
procedure::ieee_copy_sign_a2_a10
procedure::ieee_copy_sign_a3_a2
procedure::ieee_copy_sign_a3_a3
procedure::ieee_copy_sign_a3_a4
procedure::ieee_copy_sign_a3_a8
procedure::ieee_copy_sign_a3_a10
procedure::ieee_copy_sign_a4_a2
procedure::ieee_copy_sign_a4_a3
procedure::ieee_copy_sign_a4_a4
procedure::ieee_copy_sign_a4_a8
procedure::ieee_copy_sign_a4_a10
procedure::ieee_copy_sign_a8_a2
procedure::ieee_copy_sign_a8_a3
procedure::ieee_copy_sign_a8_a4
procedure::ieee_copy_sign_a8_a8
procedure::ieee_copy_sign_a8_a10
procedure::ieee_copy_sign_a10_a2
procedure::ieee_copy_sign_a10_a3
procedure::ieee_copy_sign_a10_a4
procedure::ieee_copy_sign_a10_a8
procedure::ieee_copy_sign_a10_a10
end interface
interface ieee_get_rounding_mode
procedure::ieee_get_rounding_mode_0
procedure::ieee_get_rounding_mode_i1
procedure::ieee_get_rounding_mode_i2
procedure::ieee_get_rounding_mode_i4
procedure::ieee_get_rounding_mode_i8
procedure::ieee_get_rounding_mode_i16
end interface
interface ieee_get_underflow_mode
procedure::ieee_get_underflow_mode_l1
procedure::ieee_get_underflow_mode_l2
procedure::ieee_get_underflow_mode_l4
procedure::ieee_get_underflow_mode_l8
end interface
interface ieee_is_finite
procedure::ieee_is_finite_a2
procedure::ieee_is_finite_a3
procedure::ieee_is_finite_a4
procedure::ieee_is_finite_a8
procedure::ieee_is_finite_a10
end interface
interface ieee_logb
procedure::ieee_logb_a2
procedure::ieee_logb_a3
procedure::ieee_logb_a4
procedure::ieee_logb_a8
procedure::ieee_logb_a10
end interface
interface ieee_max
procedure::ieee_max_a2
procedure::ieee_max_a3
procedure::ieee_max_a4
procedure::ieee_max_a8
procedure::ieee_max_a10
end interface
interface ieee_max_mag
procedure::ieee_max_mag_a2
procedure::ieee_max_mag_a3
procedure::ieee_max_mag_a4
procedure::ieee_max_mag_a8
procedure::ieee_max_mag_a10
end interface
interface ieee_max_num
procedure::ieee_max_num_a2
procedure::ieee_max_num_a3
procedure::ieee_max_num_a4
procedure::ieee_max_num_a8
procedure::ieee_max_num_a10
end interface
interface ieee_max_num_mag
procedure::ieee_max_num_mag_a2
procedure::ieee_max_num_mag_a3
procedure::ieee_max_num_mag_a4
procedure::ieee_max_num_mag_a8
procedure::ieee_max_num_mag_a10
end interface
interface ieee_min
procedure::ieee_min_a2
procedure::ieee_min_a3
procedure::ieee_min_a4
procedure::ieee_min_a8
procedure::ieee_min_a10
end interface
interface ieee_min_mag
procedure::ieee_min_mag_a2
procedure::ieee_min_mag_a3
procedure::ieee_min_mag_a4
procedure::ieee_min_mag_a8
procedure::ieee_min_mag_a10
end interface
interface ieee_min_num
procedure::ieee_min_num_a2
procedure::ieee_min_num_a3
procedure::ieee_min_num_a4
procedure::ieee_min_num_a8
procedure::ieee_min_num_a10
end interface
interface ieee_min_num_mag
procedure::ieee_min_num_mag_a2
procedure::ieee_min_num_mag_a3
procedure::ieee_min_num_mag_a4
procedure::ieee_min_num_mag_a8
procedure::ieee_min_num_mag_a10
end interface
interface ieee_quiet_eq
procedure::ieee_quiet_eq_a2
procedure::ieee_quiet_eq_a3
procedure::ieee_quiet_eq_a4
procedure::ieee_quiet_eq_a8
procedure::ieee_quiet_eq_a10
end interface
interface ieee_quiet_ge
procedure::ieee_quiet_ge_a2
procedure::ieee_quiet_ge_a3
procedure::ieee_quiet_ge_a4
procedure::ieee_quiet_ge_a8
procedure::ieee_quiet_ge_a10
end interface
interface ieee_quiet_gt
procedure::ieee_quiet_gt_a2
procedure::ieee_quiet_gt_a3
procedure::ieee_quiet_gt_a4
procedure::ieee_quiet_gt_a8
procedure::ieee_quiet_gt_a10
end interface
interface ieee_quiet_le
procedure::ieee_quiet_le_a2
procedure::ieee_quiet_le_a3
procedure::ieee_quiet_le_a4
procedure::ieee_quiet_le_a8
procedure::ieee_quiet_le_a10
end interface
interface ieee_quiet_lt
procedure::ieee_quiet_lt_a2
procedure::ieee_quiet_lt_a3
procedure::ieee_quiet_lt_a4
procedure::ieee_quiet_lt_a8
procedure::ieee_quiet_lt_a10
end interface
interface ieee_quiet_ne
procedure::ieee_quiet_ne_a2
procedure::ieee_quiet_ne_a3
procedure::ieee_quiet_ne_a4
procedure::ieee_quiet_ne_a8
procedure::ieee_quiet_ne_a10
end interface
interface ieee_rem
procedure::ieee_rem_a2_a2
procedure::ieee_rem_a2_a3
procedure::ieee_rem_a2_a4
procedure::ieee_rem_a2_a8
procedure::ieee_rem_a2_a10
procedure::ieee_rem_a3_a2
procedure::ieee_rem_a3_a3
procedure::ieee_rem_a3_a4
procedure::ieee_rem_a3_a8
procedure::ieee_rem_a3_a10
procedure::ieee_rem_a4_a2
procedure::ieee_rem_a4_a3
procedure::ieee_rem_a4_a4
procedure::ieee_rem_a4_a8
procedure::ieee_rem_a4_a10
procedure::ieee_rem_a8_a2
procedure::ieee_rem_a8_a3
procedure::ieee_rem_a8_a4
procedure::ieee_rem_a8_a8
procedure::ieee_rem_a8_a10
procedure::ieee_rem_a10_a2
procedure::ieee_rem_a10_a3
procedure::ieee_rem_a10_a4
procedure::ieee_rem_a10_a8
procedure::ieee_rem_a10_a10
end interface
interface ieee_rint
procedure::ieee_rint_a2
procedure::ieee_rint_a3
procedure::ieee_rint_a4
procedure::ieee_rint_a8
procedure::ieee_rint_a10
end interface
interface ieee_set_rounding_mode
procedure::ieee_set_rounding_mode_0
procedure::ieee_set_rounding_mode_i1
procedure::ieee_set_rounding_mode_i2
procedure::ieee_set_rounding_mode_i4
procedure::ieee_set_rounding_mode_i8
procedure::ieee_set_rounding_mode_i16
end interface
interface ieee_set_underflow_mode
procedure::ieee_set_underflow_mode_l1
procedure::ieee_set_underflow_mode_l2
procedure::ieee_set_underflow_mode_l4
procedure::ieee_set_underflow_mode_l8
end interface
interface ieee_signaling_eq
procedure::ieee_signaling_eq_a2
procedure::ieee_signaling_eq_a3
procedure::ieee_signaling_eq_a4
procedure::ieee_signaling_eq_a8
procedure::ieee_signaling_eq_a10
end interface
interface ieee_signaling_ge
procedure::ieee_signaling_ge_a2
procedure::ieee_signaling_ge_a3
procedure::ieee_signaling_ge_a4
procedure::ieee_signaling_ge_a8
procedure::ieee_signaling_ge_a10
end interface
interface ieee_signaling_gt
procedure::ieee_signaling_gt_a2
procedure::ieee_signaling_gt_a3
procedure::ieee_signaling_gt_a4
procedure::ieee_signaling_gt_a8
procedure::ieee_signaling_gt_a10
end interface
interface ieee_signaling_le
procedure::ieee_signaling_le_a2
procedure::ieee_signaling_le_a3
procedure::ieee_signaling_le_a4
procedure::ieee_signaling_le_a8
procedure::ieee_signaling_le_a10
end interface
interface ieee_signaling_lt
procedure::ieee_signaling_lt_a2
procedure::ieee_signaling_lt_a3
procedure::ieee_signaling_lt_a4
procedure::ieee_signaling_lt_a8
procedure::ieee_signaling_lt_a10
end interface
interface ieee_signaling_ne
procedure::ieee_signaling_ne_a2
procedure::ieee_signaling_ne_a3
procedure::ieee_signaling_ne_a4
procedure::ieee_signaling_ne_a8
procedure::ieee_signaling_ne_a10
end interface
interface ieee_signbit
procedure::ieee_signbit_a2
procedure::ieee_signbit_a3
procedure::ieee_signbit_a4
procedure::ieee_signbit_a8
procedure::ieee_signbit_a10
end interface
interface ieee_unordered
procedure::ieee_unordered_a2_a2
procedure::ieee_unordered_a2_a3
procedure::ieee_unordered_a2_a4
procedure::ieee_unordered_a2_a8
procedure::ieee_unordered_a2_a10
procedure::ieee_unordered_a3_a2
procedure::ieee_unordered_a3_a3
procedure::ieee_unordered_a3_a4
procedure::ieee_unordered_a3_a8
procedure::ieee_unordered_a3_a10
procedure::ieee_unordered_a4_a2
procedure::ieee_unordered_a4_a3
procedure::ieee_unordered_a4_a4
procedure::ieee_unordered_a4_a8
procedure::ieee_unordered_a4_a10
procedure::ieee_unordered_a8_a2
procedure::ieee_unordered_a8_a3
procedure::ieee_unordered_a8_a4
procedure::ieee_unordered_a8_a8
procedure::ieee_unordered_a8_a10
procedure::ieee_unordered_a10_a2
procedure::ieee_unordered_a10_a3
procedure::ieee_unordered_a10_a4
procedure::ieee_unordered_a10_a8
procedure::ieee_unordered_a10_a10
end interface
interface ieee_value
procedure::ieee_value_a2
procedure::ieee_value_a3
procedure::ieee_value_a4
procedure::ieee_value_a8
procedure::ieee_value_a10
end interface
end
