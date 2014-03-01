IFDEF RAX

api_task_getcontext PROTO STDCALL :QWORD
api_task_setcontext PROTO STDCALL :QWORD

; ToDo: replace magic numbers with CONTEXT structure field offset calculations
offset_rip equ 252
offset_rsp equ 156

.code

api_task_swapcontext_native PROC
	push [rsp + 8]
	call api_task_getcontext

	; correct rip
	lea rax, [rsp + 8]
	add rax, offset_rip
	lea rdx, done
	mov [rax], rdx

	; correct rsp
	lea rax, [rsp + 8]
	add rax, offset_rsp
	mov [rax], rsp

	push [rsp + 16]
	call api_task_setcontext
done:
	add rsp, 16
	ret
api_task_swapcontext_native ENDP

ENDIF

end