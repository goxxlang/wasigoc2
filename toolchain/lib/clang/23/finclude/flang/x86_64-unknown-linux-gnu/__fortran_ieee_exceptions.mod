!mod$ v1 sum:a0a7a405c46ba153
!need$ d676699be0556005 i __fortran_builtins
module __fortran_ieee_exceptions
use,intrinsic::__fortran_builtins,only:ieee_flag_type=>__builtin_ieee_flag_type
use,intrinsic::__fortran_builtins,only:ieee_support_flag=>__builtin_ieee_support_flag
use,intrinsic::__fortran_builtins,only:ieee_support_halting=>__builtin_ieee_support_halting
use,intrinsic::__fortran_builtins,only:ieee_invalid=>__builtin_ieee_invalid
use,intrinsic::__fortran_builtins,only:ieee_overflow=>__builtin_ieee_overflow
use,intrinsic::__fortran_builtins,only:ieee_divide_by_zero=>__builtin_ieee_divide_by_zero
use,intrinsic::__fortran_builtins,only:ieee_underflow=>__builtin_ieee_underflow
use,intrinsic::__fortran_builtins,only:ieee_inexact=>__builtin_ieee_inexact
use,intrinsic::__fortran_builtins,only:ieee_denorm=>__builtin_ieee_denorm
type(ieee_flag_type),parameter::ieee_usual(1_8:*)=[ieee_flag_type::ieee_flag_type(flag=8_1),ieee_flag_type(flag=4_1),ieee_flag_type(flag=1_1)]
type(ieee_flag_type),parameter::ieee_all(1_8:*)=[ieee_flag_type::ieee_flag_type(flag=8_1),ieee_flag_type(flag=4_1),ieee_flag_type(flag=1_1),ieee_flag_type(flag=16_1),ieee_flag_type(flag=32_1)]
type::ieee_modes_type
integer(4),private::__data(1_8:2_8)
integer(1),allocatable,private::__allocatable_data(:)
end type
type::ieee_status_type
integer(4),private::__data(1_8:8_8)
integer(1),allocatable,private::__allocatable_data(:)
end type
private::ieee_get_flag_l1
interface
elemental subroutine ieee_get_flag_l1(flag,flag_value)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(1),intent(out)::flag_value
end
end interface
private::ieee_get_flag_l2
interface
elemental subroutine ieee_get_flag_l2(flag,flag_value)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(2),intent(out)::flag_value
end
end interface
private::ieee_get_flag_l4
interface
elemental subroutine ieee_get_flag_l4(flag,flag_value)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(4),intent(out)::flag_value
end
end interface
private::ieee_get_flag_l8
interface
elemental subroutine ieee_get_flag_l8(flag,flag_value)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(8),intent(out)::flag_value
end
end interface
private::ieee_get_halting_mode_l1
interface
elemental subroutine ieee_get_halting_mode_l1(flag,halting)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(1),intent(out)::halting
end
end interface
private::ieee_get_halting_mode_l2
interface
elemental subroutine ieee_get_halting_mode_l2(flag,halting)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(2),intent(out)::halting
end
end interface
private::ieee_get_halting_mode_l4
interface
elemental subroutine ieee_get_halting_mode_l4(flag,halting)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(4),intent(out)::halting
end
end interface
private::ieee_get_halting_mode_l8
interface
elemental subroutine ieee_get_halting_mode_l8(flag,halting)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(8),intent(out)::halting
end
end interface
private::ieee_get_modes_0
interface
pure subroutine ieee_get_modes_0(modes)
import::ieee_modes_type
type(ieee_modes_type),intent(out)::modes
end
end interface
private::ieee_get_status_0
interface
pure subroutine ieee_get_status_0(status)
import::ieee_status_type
type(ieee_status_type),intent(out)::status
end
end interface
private::ieee_set_flag_l1
interface
elemental subroutine ieee_set_flag_l1(flag,flag_value)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(1),intent(in)::flag_value
end
end interface
private::ieee_set_flag_l2
interface
elemental subroutine ieee_set_flag_l2(flag,flag_value)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(2),intent(in)::flag_value
end
end interface
private::ieee_set_flag_l4
interface
elemental subroutine ieee_set_flag_l4(flag,flag_value)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(4),intent(in)::flag_value
end
end interface
private::ieee_set_flag_l8
interface
elemental subroutine ieee_set_flag_l8(flag,flag_value)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(8),intent(in)::flag_value
end
end interface
private::ieee_set_halting_mode_l1
interface
elemental subroutine ieee_set_halting_mode_l1(flag,halting)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(1),intent(in)::halting
end
end interface
private::ieee_set_halting_mode_l2
interface
elemental subroutine ieee_set_halting_mode_l2(flag,halting)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(2),intent(in)::halting
end
end interface
private::ieee_set_halting_mode_l4
interface
elemental subroutine ieee_set_halting_mode_l4(flag,halting)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(4),intent(in)::halting
end
end interface
private::ieee_set_halting_mode_l8
interface
elemental subroutine ieee_set_halting_mode_l8(flag,halting)
import::ieee_flag_type
type(ieee_flag_type),intent(in)::flag
logical(8),intent(in)::halting
end
end interface
private::ieee_set_modes_0
interface
subroutine ieee_set_modes_0(modes)
import::ieee_modes_type
type(ieee_modes_type),intent(in)::modes
end
end interface
private::ieee_set_status_0
interface
pure subroutine ieee_set_status_0(status)
import::ieee_status_type
type(ieee_status_type),intent(in)::status
end
end interface
interface ieee_get_flag
procedure::ieee_get_flag_l1
procedure::ieee_get_flag_l2
procedure::ieee_get_flag_l4
procedure::ieee_get_flag_l8
end interface
interface ieee_get_halting_mode
procedure::ieee_get_halting_mode_l1
procedure::ieee_get_halting_mode_l2
procedure::ieee_get_halting_mode_l4
procedure::ieee_get_halting_mode_l8
end interface
interface ieee_get_modes
procedure::ieee_get_modes_0
end interface
interface ieee_get_status
procedure::ieee_get_status_0
end interface
interface ieee_set_flag
procedure::ieee_set_flag_l1
procedure::ieee_set_flag_l2
procedure::ieee_set_flag_l4
procedure::ieee_set_flag_l8
end interface
interface ieee_set_halting_mode
procedure::ieee_set_halting_mode_l1
procedure::ieee_set_halting_mode_l2
procedure::ieee_set_halting_mode_l4
procedure::ieee_set_halting_mode_l8
end interface
interface ieee_set_modes
procedure::ieee_set_modes_0
end interface
interface ieee_set_status
procedure::ieee_set_status_0
end interface
end
