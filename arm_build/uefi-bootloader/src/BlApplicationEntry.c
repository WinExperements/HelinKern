#include <efi.h>
#include <efilib.h>

#include "application.h"

// Boot Manager Application Entrypoint
// Bootstraps environment and transfer control to EFI Application entry point.
NTSTATUS BlApplicationEntry(
	_In_ PBOOT_APPLICATION_PARAMETER_BLOCK BootAppParameters,
	_In_ PBL_LIBRARY_PARAMETERS LibraryParameters
)
{

	// Get excited now
	PBL_LIBRARY_PARAMETERS	LibraryParams;
	PBL_FIRMWARE_DESCRIPTOR FirmwareDescriptor;
	uint32_t ParamPointer;

	if (!BootAppParameters)
	{
		// Invalid parameter
		return STATUS_INVALID_PARAMETER;
	}

	LibraryParams = LibraryParameters;
	ParamPointer = (uint32_t) BootAppParameters;
	FirmwareDescriptor = (PBL_FIRMWARE_DESCRIPTOR) (ParamPointer + BootAppParameters->FirmwareParametersOffset);

	// Switch mode
	SwitchToRealModeContext(FirmwareDescriptor);

	// Do what ever you want now
	if (FirmwareDescriptor->SystemTable)
	{
		EFIApp_Main(
			FirmwareDescriptor->ImageHandle,
			FirmwareDescriptor->SystemTable,
			BootAppParameters
		);
	}

	// We are done
	return STATUS_SUCCESS;

}

