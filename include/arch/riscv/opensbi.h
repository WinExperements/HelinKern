#pragma once
struct SbiRet {
  long error;
  long value;
};
struct SbiRet sbi_ecall(int ext,int fid,unsigned long arg0,unsigned long arg1,unsigned long arg2,unsigned long arg3,unsigned long arg4,unsigned long arg5);
enum sbi_calls {
	SBI_EXT_SRST = 0x53525354,
};
