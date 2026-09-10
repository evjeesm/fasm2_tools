VTEXT_DEFAULT_SIZE = 1024*1024 ;; 1MB

purge VTEXT_PREALLOC
macro VTEXT_PREALLOC name*, cap:VTEXT_DEFAULT_SIZE
  virtual at 0
    name::
      rb cap
  end virtual
    name#._capacity = cap
    name#.len = 0
end macro

purge VTEXT_RESET
macro VTEXT_RESET name*
   name#.len = 0
end macro

;; Creates a text in a virtual address space
purge VTEXT
macro VTEXT name*, arg*&
  virtual at 0
    name::
      db arg
    name#.len = $
    name#._capacity = $
  end virtual
end macro

;; Read entire file into a virtual address space
purge VTEXT_FILE
macro VTEXT_FILE name*, filepath*
  virtual at 0
    name:: file filepath
    name#.len = $
    name#._capacity = $
  end virtual
end macro

;; Append to a previously defined VTEXT
;; IMPORTANT: expensive operation, which deplete memory fast.
;;            if you append single byte sequencially
;;            54 Kb is max you can reach before memory runs out
;;            with max memory setting (-m 2000000) 2G
purge VTEXT_APPEND_
macro VTEXT_APPEND_ name*, arg*&
  STATIC_ASSERT 0, "deprecated!"
  virtual name
    db arg
    name#.len = $
  end virtual
end macro

purge VTEXT_ASSEMBLE
macro VTEXT_ASSEMBLE name*, line*&
  virtual at 0
  local b,offset,size
  iterate l, line
    l
  end iterate
  offset = 0
  size = $-$$
  VTEXT_RESERVE name, name#.len + size
  ;; STATIC_ASSERT name#.len + size <= name#._capacity, "VTEXT overflow!"
  while offset < size
    load b byte from offset
    store byte b at name:name#.len+offset
    offset = offset + 1
  end while
  name#.len = name#.len + size
  end virtual
end macro

purge VTEXT_APPEND
macro VTEXT_APPEND name*, arg*&
  virtual at 0
  local b,offset
    iterate argument, arg
      db arg
    end iterate
    STATIC_ASSERT name#.len + $-$$ <= name#._capacity, \
        "VTEXT: name# overflow!"
    offset = 0
    while offset < $-$$
      load b byte from offset
      store byte b at name:name#.len+offset
      offset = offset + 1
    end while
    name#.len = name#.len + $-$$
  end virtual
end macro

purge VTEXT_APPEND_AS_IS
macro VTEXT_APPEND_AS_IS name*, arg*&
  iterate a, arg
    VTEXT_APPEND name, a
  end iterate
end macro

;; Enlarge vtext with additional 'capacity'
purge VTEXT_EXPAND
macro VTEXT_EXPAND name*, capacity*
  virtual name
    rb capacity
      name#._capacity = $
  end virtual
end macro

;; Reserve certain amount of bytes
purge VTEXT_RESERVE
macro VTEXT_RESERVE name*, capacity*
    if name._capacity < capacity
      VTEXT_EXPAND name, capacity - name._capacity
    end if
end macro

;; Generic bake that takes user baking strategy.
;; Param BAKE_CHUNK - is a macro of form:
;;   `BAKE_CHUNK dest, vsrc, size, offset`
;;
purge VTEXT_BAKE
macro VTEXT_BAKE dest*, vsrc*, BAKE_CHUNK*, until:?, init_offset:0, init_chunk_sz:8
  local offset, chunk_sz, _until
  offset = init_offset
  chunk_sz = init_chunk_sz

  match ?, until
    _until = vsrc#.len
  else
    _until = until
  end match
  STATIC_ASSERT _until <= vsrc#.len, "Size should not exceed vtext bounds!"

  while offset < _until
    repeat (_until - offset) / chunk_sz
      BAKE_CHUNK dest, vsrc, chunk_sz, offset
      offset = offset + chunk_sz
    end repeat
    chunk_sz = chunk_sz / 2
  end while
end macro

;; Generate immediate movs of the virutal text to a destination
purge VTEXT_BAKE_MOVS
macro VTEXT_BAKE_MOVS dest*, vsrc*
  local n, d, text_len, _STORE
  n = 0
  d = 4
  text_len = vsrc#.len
  while n < text_len
    repeat (text_len-n)/d
      purge _STORE
      macro _STORE dest, src, size, offset
       local chunk
        load chunk size from src:offset
        mov size [dest+offset], chunk
      end macro _STORE
      if d = 4
        _STORE dest, vsrc, dword, n
      else if d = 2
        _STORE dest, vsrc, word, n
      else if d = 1
        _STORE dest, vsrc, byte, n
      end if
      n = n + d
    end repeat
    d = d / 2
  end while
  purge _STORE
end macro

;; Generate immediate stack pushes of the virtual text
purge VTEXT_BAKE_STACK
macro VTEXT_BAKE_STACK vsrc*
  local n, d, text_len, _STORE
  text_len = vsrc#.len
  n = text_len
  d = 4
  while n > 0
    repeat n / d
      purge _STORE
      macro _STORE src, size, offset
        local chunk
        if size = 8
          load chunk qword from src:offset
          mov rax, chunk
          push rax
        else if size = 4
          load chunk word from src:offset+2
          push word chunk
          load chunk word from src:offset
          push word chunk
        else if size = 2
          load chunk word from src:offset
          push word chunk
        else if size = 1
          load chunk byte from src:offset
          sub rsp, 1
          mov byte [rsp], chunk
        end if
      end macro
      _STORE vsrc, d, n-d
      n = n - d
    end repeat
    d = d / 2
  end while
  purge _STORE
end macro

purge VTEXT_SLICE
macro VTEXT_SLICE vsrc*, start_offset:*, end_offset*
  local i, b
  i = 0
  STATIC_ASSERT start_offset < end_offset, "slice bounds should come in asc order!"
  STATIC_ASSERT end_offset <= vsrc#.len, "vsrc overflow!"
  while start_offset + i < end_offset
    load b byte from vsrc:start_offset+i
    store byte b at vsrc:i
    i = i + 1
  end while
end macro

purge VTEXT_SET
macro VTEXT_SET vdest*, arg*&
  virtual at 0
  local tmp_size, i, b
    db arg
    tmp_size = $
    VTEXT_RESERVE vdest, tmp_size
    i = 0
    while i < tmp_size
      load b byte from i
      store byte b at vdest:i
      i = i + 1
    end while
  end virtual
  vdest#.len = tmp_size
end macro

purge VTEXT_CMP
macro VTEXT_CMP vtext*, vref*, vref_offset*, size*, output*
  local i, c1, c2, equal
  equal = 1
  if vtext#.len <> size
    output = 0
  else
    i = 0
    while i < size
      VTEXT_LOAD_CHAR vtext, i, c1
      VTEXT_LOAD_CHAR vref, i + vref_offset, c2
      if c1 <> c2
        equal = 0
        break
      end if
      i = i + 1
    end while
    output = equal
  end if
end macro

purge VTEXT_RANGE_CHAR_EQ
macro VTEXT_RANGE_CHAR_EQ vtext*, vtext_offset*, size*, char*, output*
  STATIC_ASSERT vtext_offset + size <= vtext#.len, "vtext overflow"
  local i, c, equal
  equal = 1
  i = 0
  while i < size
    VTEXT_LOAD_CHAR vtext, vtext_offset + i, c
    if c <> char
      equal = 0
      break
    end if
    i = i + 1
  end while
  output = equal
end macro

purge VTEXT_COPY
macro VTEXT_COPY vdest*, vsrc*
  local i,b
  VTEXT_RESERVE vdest, vsrc#.len
  i = 0
  while i < vsrc#.len
    load b byte from vsrc:i
    store byte b at vdest:i
    i = i + 1
  end while
  vdest#.len = vsrc#.len
end macro

purge VTEXT_COPY_RANGE
macro VTEXT_COPY_RANGE vdest*, vdest_offset*, vsrc*, vsrc_offset*, size*
  local i,b
  STATIC_ASSERT vsrc_offset + size <= vsrc#.len, "Out of vsrc bounds"
  VTEXT_RESERVE vdest, vdest_offset + size
  i = 0
  while i < size
    load b byte from vsrc:vsrc_offset+i
    store byte b at vdest:vdest_offset+i
    i = i + 1
  end while
end macro

purge VTEXT_WRITE
macro VTEXT_WRITE vdest*, vdest_offset*, vsrc*
  local i,b
  VTEXT_RESERVE vdest, vdest_offset + vsrc#.len
  i = 0
  while i < vsrc#.len
    load b byte from vsrc:i
    store byte b at vdest:vdest_offset+i
    i = i + 1
  end while
end macro

purge VTEXT_READ
macro VTEXT_READ vdest*, vsrc*, vsrc_offset*, size*
  local i,b
  STATIC_ASSERT vsrc_offset + size <= vsrc#.len, "Out of vsrc bounds"
  VTEXT_RESERVE vdest, size
  i = 0
  while i < size
    load b byte from vsrc:vsrc_offset+i
    store byte b at vdest:i
    i = i + 1
  end while
  vdest#.len = size
end macro

purge VTEXT_DISPLAY
macro VTEXT_DISPLAY vsrc*, offset:0, until:?
  local _PRINT, _until
  purge _PRINT
  macro _PRINT dest, src, _size, _offset
    local chunk,i
    if _size = 8
      load chunk qword from src:_offset
    else if _size = 4
      load chunk dword from src:_offset
    else if _size = 2
      load chunk word from src:_offset
    else if _size = 1
      load chunk byte from src:_offset
    end if
    STATIC_ASSERT _size <> 0, "Chunk size should never be zero!"
    i = 0
    while i < _size
      display (chunk shr (8*i)) and 0xFF
      i = i + 1
    end while
  end macro
  match ?, until
    _until = vsrc#.len
  else
    _until = until
  end match

  VTEXT_BAKE _, vsrc, _PRINT, _until, offset
  purge _PRINT
end macro

purge VTEXT_DISPLAY_NULLTERM
macro VTEXT_DISPLAY_NULLTERM vsrc*, offset:0
  local char,_offset
  _offset = offset
  while 1
    VTEXT_LOAD_CHAR vsrc, _offset, char
    if char = 0
      break
    end if
    display char and 0xFF
    _offset = _offset + 1
  end while
end macro

purge VTEXT_HEX
macro VTEXT_HEX vsrc*, offset:0, until:?
  local _PRINT, _HEX, _until
  purge _PRINT
  macro _PRINT dest, src, _size, _offset
    local chunk
    load chunk byte from src:_offset
    if chunk < 16
      display '0'
    end if
    _HEX chunk
  end macro
  purge _HEX
  macro _HEX num*
   local n,nib,i,shft,d
   n = (num) and 0xffff_ffff_ffff_ffff
   if n <> 0
     bit = bsr n
   else
     bit = 0
   end if
   nib = bit/4
   i = nib + 1
   while i <> 0
     shft = (i-1)*4
     d = '0' + (n and (0xF shl shft)) shr shft
     i = i-1
     if d > '9'
       d = d+'A'-'9'-1
     end if
     display d
   end while
  end macro

  match ?, until
    _until = vsrc#.len
  else
    _until = until
  end match

  VTEXT_BAKE _, vsrc, _PRINT, _until, offset, 1
  purge _PRINT
  purge _HEX
end macro

purge VTEXT_LOAD_CHAR
macro VTEXT_LOAD_CHAR vsrc*, offset*, output*
  load output byte from vsrc:offset
end macro

purge VTEXT_STORE_CHAR
macro VTEXT_STORE_CHAR vdest*, offset*, char*
  store byte char at vdest:offset
end macro

purge VTEXT_READ_FIELD
macro VTEXT_READ_FIELD vsrc*, offset*, size*, output*
  if size = 1
    load output byte from vsrc:offset
  else if size = 2
    load output word from vsrc:offset
  else if size = 4
    load output dword from vsrc:offset
  else if size = 8
    load output qword from vsrc:offset
  end if
end macro


purge VTEXT_READ_TO_VALUE
macro VTEXT_READ_TO_VALUE vsrc*, offset*, size*, output*
  local char
  STATIC_ASSERT size+offset <= vsrc#.len, "vsrc overflow"
  STATIC_ASSERT size <= 8, "Value don't fit in 64 bit variable"
  output = 0
  i = 0
  while i < size
    VTEXT_LOAD_CHAR vsrc, i+offset, char
    output = (char shl (i*8)) or output
    i = i + 1
  end while
end macro

purge VALUE_TO_VTEXT
macro VALUE_TO_VTEXT vdest*, value*, size*
  local pad
  VTEXT_SET vdest, value
  pad = size - vdest#.len
  if pad >= 0
    VTEXT_ASSEMBLE vdest, db pad dup 0
  else ;; pad is negative
    STATIC_ASSERT pad >= 0, "arg '", value, "' has larger size than designed"
  end if
end macro

purge VTEXT_FILL
macro VTEXT_FILL vdest*, vdest_offset*, size*, char*&
  local i
  VTEXT_RESERVE vdest, vdest_offset+size
  i = 0
  while i < size
    iterate c, char
      if i >= size
        break
      end if
      store byte c at vdest:vdest_offset+i
      i = i + 1
    end iterate
  end while
  if vdest_offset+size > vdest#.len
    vdest#.len = vdest_offset+size
  end if
end macro

purge VTEXT_PAD_WITH
macro VTEXT_PAD_WITH vdest*, required_size*, char*&
  local pad_size, i
  pad_size = required_size - vdest#.len
  if pad_size < 0
    pad_size = 0
  end if
  VTEXT_RESERVE vdest, required_size
  i = 0
  while i < pad_size
    iterate c, char
      if i >= pad_size
        break
      end if
      VTEXT_APPEND vdest, c
      i = i + 1
    end iterate
  end while
end macro

purge VTEXT_XOR
macro VTEXT_XOR vsrc*, key*
  local b
  repeat vsrc#.len
    load b byte from vsrc:%-1
    store byte b xor key at vsrc:%-1
  end repeat
end macro

purge ASSEMBLE
macro ASSEMBLE dest*, vsrc*, size*, offset*
  local chunk
  if size = 8
    load chunk qword from vsrc:offset
    dq chunk
  else if size = 4
    load chunk dword from vsrc:offset
    dd chunk
  else if size = 2
    load chunk word from vsrc:offset
    dw chunk
  else if size = 1
    load chunk byte from vsrc:offset
    db chunk
  end if
end macro
