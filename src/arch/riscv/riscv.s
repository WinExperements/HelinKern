# RISC-V Assembly. 
# Used source: https://github.com/cccriscv/mini-riscv-os/blob/master/10-SystemCall/src/sys.s 
# Define macro for context register saving.
.macro reg_save base
        # save the registers.
        sd ra, 0(\base)	#1
        sd sp, 8(\base)	#2
        sd gp, 16(\base) #3
        sd tp, 24(\base) #4
        sd t0, 32(\base) #5
        sd t1, 40(\base) #6
        sd t2, 48(\base) #7
        sd s0, 56(\base) #8
        sd s1, 64(\base) #9
        sd a0, 72(\base) #10
        sd a1, 80(\base) #11
        sd a2, 88(\base) #12
        sd a3, 96(\base) #13
        sd a4, 104(\base) #14
        sd a5, 112(\base) #15
        sd a6, 120(\base) #16
        sd a7, 128(\base) #17
        sd s2, 136(\base) #18
        sd s3, 144(\base) #19
        sd s4, 152(\base) #20
        sd s5, 160(\base) #21
        sd s6, 168(\base) #22
        sd s7, 176(\base) #23
        sd s8, 184(\base) #24
        sd s9, 192(\base) #26
        sd s10, 200(\base) #27
        sd s11, 208(\base) #28
        sd t3, 216(\base) #29
        sd t4, 224(\base) # 30
        sd t5, 232(\base) # 31
	sd t6, 240(\base) # 32
	# save return address
	csrr t5,sepc
	sd t5,248(\base)
	# save spp.
	csrr t5,sstatus
	srli t5,t5,8
	andi t5,t5,0xF
	sd t5,256(\base)
.endm

.macro reg_load base
        # restore registers.
        ld ra, 0(\base)
	ld sp,8(\base)
        ld gp, 16(\base)
        ld tp, 24(\base)
        ld t0, 32(\base)
        ld t1, 40(\base)
        ld t2, 48(\base)
        ld s0, 56(\base)
        ld s1, 64(\base)
        ld a0, 72(\base)
        ld a1, 80(\base)
        ld a2, 88(\base)
        ld a3, 96(\base)
        ld a4, 104(\base)
        ld a5, 112(\base)
        ld a6, 120(\base)
        ld a7, 128(\base)
        ld s2, 136(\base)
        ld s3, 144(\base)
        ld s4, 152(\base)
        ld s5, 160(\base)
        ld s6, 168(\base)
        ld s7, 176(\base)
        ld s8, 184(\base)
        ld s9, 192(\base)
        ld s10, 200(\base)
        ld s11, 208(\base)
        ld t3, 216(\base)
        ld t4, 224(\base)
        ld t5, 232(\base)
        ld t6, 240(\base)
.endm

.global riscv_mtrap_handler
.extern intStack_ptr
riscv_mtrap_handler:
  # Supervisor mode trap handler.
  reg_save tp
  mv a2,tp
  csrr a0,sepc
  csrr a1,scause
  call riscv_trap_handler
  csrw sepc,a0
  reg_load tp
  sret

.global riscv_switchCntx
riscv_switchCntx:
	# Save the context
	la t0,runningTask
	ld t1,0(t0)
	ld t1,0(t1)
	sd s0,0(t1)
	sd s1,8(t1)
	sd s2,16(t1)
	sd s3,24(t1)
	sd s4,32(t1)
	sd s5,40(t1)
	sd s6,48(t1)
	sd s7,56(t1)
	sd s8,64(t1)
	sd s9,72(t1)
	sd s10,80(t1)
	sd s11,88(t1)
	sd ra,96(t1)
	sd a0,0(t0)
	# Load new context
	ld t1,0(t0)
	ld t1,0(t1)
	ld s0,0(t1)
	ld s1,8(t1)
	ld s2,16(t1)
	ld s3,24(t1)
	ld s4,32(t1)
	ld s5,40(t1)
	ld s6,48(t1)
	ld s7,56(t1)
	ld s8,64(t1)
	ld s9,72(t1)
	ld s10,80(t1)
	ld s11,88(t1)
	ld ra,96(t1)
	ret
