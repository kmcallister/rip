bits 64


;; IO routines, which must use registers per system call ABI.
;; These are not involved in pure computation.

; Call write(1, &out_byte, 1) and return to no-regs code.
syscall_output:
    mov rax, 1
    mov rdi, 1
    mov rsi, out_byte
    mov rdx, 1
    syscall
    jmp [cont_zero]

; Call read(0, &finish_input+8, 1) and return to no-regs code.
syscall_input:
    mov rax, 0
    mov rdi, 0
    mov rsi, finish_input+8  ; = that inst's immediate operand
    mov rdx, 1
    syscall
    jmp finish_input

; Exit the program.
syscall_exit:
    mov rax, 60
    mov rdi, 0
    syscall


;; Use of registers is forbidden past this point.
;; regclobber looks for this symbol in the executable.
FORBID_REGS:


test_cell:
    ; Operand modified by move_*
    jmp [head]

inc_cell:
    ; Operand modified by move_*
    add qword [head], 16
    jmp test_cell

dec_cell:
    ; Operand modified by move_*
    sub qword [head], 16
    jmp test_cell

move_right:
    add dword [inc_cell+4],  8
    add dword [dec_cell+4],  8
    add dword [test_cell+3], 8

    add dword [finish_input+4],    8
    add dword [finish_input_i2+4], 8
    add dword [finish_input_i3+4], 8

    jmp [cont_zero]

move_left:
    sub dword [inc_cell+4],  8
    sub dword [dec_cell+4],  8
    sub dword [test_cell+3], 8

    sub dword [finish_input+4],    8
    sub dword [finish_input_i2+4], 8
    sub dword [finish_input_i3+4], 8

    jmp [cont_zero]

input:
    ; If read() returns EOF it should be like we read zero
    mov byte [finish_input+8], 0
    jmp syscall_input
finish_input:
    ; Memory operand modified by move_*
    ; Immediate operand modified by syscall_input
    ; This code self-modifies by calling read().  The future is here.
    mov qword [head], 0
finish_input_i2:
    ; Operand modified by move_*
    shl qword [head], 4  ; multiply by 16
finish_input_i3:
    ; Operand modified by move_*
    add qword [head], cell0
    jmp [cont_zero]


;; The compiler outputs an invocation of one of these macros for
;; each Brainfuck command.

%macro do_inc 1
    mov qword [cont_zero],    %1
    mov qword [cont_nonzero], %1
    jmp inc_cell
%endmacro

%macro do_dec 1
    mov qword [cont_zero],    %1
    mov qword [cont_nonzero], %1
    jmp dec_cell
%endmacro

%macro do_left 1
    mov qword [cont_zero],    %1
    jmp move_left
%endmacro

%macro do_right 1
    mov qword [cont_zero],    %1
    jmp move_right
%endmacro

%macro do_branch 2
    mov qword [cont_zero],    %1
    mov qword [cont_nonzero], %2
    jmp test_cell
%endmacro

%macro do_output 1
    mov qword [cont_zero],    %%k
    mov qword [cont_nonzero], %%k
    jmp test_cell
; Macro-local label.  We test in order to load out_byte.
%%k:
    mov qword [cont_zero],    %1
    jmp syscall_output
%endmacro

%macro do_input 1
    mov qword [cont_zero],    %1
    jmp input
%endmacro


;; Each tape cell is represented by a pointer to one of these
;; routines.

align 16
cell_underflow:
    jmp inc_cell

align 16
cell0:
    mov byte [out_byte], 0
    jmp [cont_zero]

%assign cellval 1
%rep 255
    align 16
    mov byte [out_byte], cellval
    jmp [cont_nonzero]
    %assign cellval cellval+1
%endrep

align 16
cell_overflow:
    jmp dec_cell


;; Entry point

global _start
_start:
    ; Include the compiler output
    %include "out.asm"


;; Temporary variables

cont_zero:     dq 0
cont_nonzero:  dq 0
out_byte:      db 0


;; The tape.  This size limit matches Urban Müller's original
;; compiler, according to http://esolangs.org/wiki/Brainfuck

TAPE_SIZE equ 30000

tape_start:
    times TAPE_SIZE dq cell0

; The head starts in the middle.
head equ tape_start + (TAPE_SIZE / 2)

; vim: ft=tasm
