!mod$ v1 sum:3efb521881bde91d
!need$ a0a7a405c46ba153 i __fortran_ieee_exceptions
module ieee_exceptions
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
end
