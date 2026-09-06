;;
;; Classical compile time assert with message.
;;
purge STATIC_ASSERT
macro STATIC_ASSERT cond*, msg*&
  if ~(cond)
    display \
      "[31m", \
      "STATIC ASSERT: ", msg, \
      "[0m"
    err
  end if
end macro

purge TODO
macro TODO msg*&
  display \
    "[31m", \
    "Unimplemented: ", msg, \
    "[0m"
  err
end macro

purge CRITICAL
macro CRITICAL msg*&
  display \
    "[31m", \
    "Critical: ", msg, \
    "[0m"
  err
end macro

purge INFO
macro INFO msg*&
  display \
    "[36m", \
    "INFO: ", msg, \
    "[0m"
end macro

purge DEBUG
macro DEBUG msg*&
  START_DEBUG
    display \
      "[34m", \
      msg, \
      "[0m"
  END_DEBUG
end macro

purge WARNING
macro WARNING msg*&
  display \
    "[33m", \
    "Warning: ", msg, \
    "[0m"
end macro

purge DISP_LINE_NUM
macro DISP_LINE_NUM line*
  local orig,n,dv,dg
  orig = line
  n = orig
  dv = 1
  while n > 0
    dv = dv*10
    n = n/10
  end while
  n = orig
  while dv > 1
    dv = dv/10
    dg = '0' + ((n/dv) mod 10)
    display dg
  end while
end macro

;; Format code as it is.
;; Enclose in angular brackets
purge DISP_CODE_LINE
macro DISP_CODE_LINE line*&
 forward
  irps e, line
  \{
   display \`e, ' '
  \}
  display 10
end macro

;; Assemble line and print.
;; Enclose in angular brackets
purge TRACE
macro TRACE line*
  display 'TRACE ',__FILE__,'['
  DISP_LINE_NUM __LINE__
  display ']: '
  match l, <line> \{ DISP_CODE_LINE l \}
  line
end macro

purge DEBUG_TRACE
macro DEBUG_TRACE line*
  START_DEBUG
    display 'TRACE ',__FILE__,'['
    DISP_LINE_NUM __LINE__
    display ']: '
    match l, <line> \{ DISP_CODE_LINE l \}
  END_DEBUG
  line
end macro
