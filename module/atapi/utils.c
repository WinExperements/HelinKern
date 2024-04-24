#include <atapi/atapi.h>
void ata_convertIdentify(ata_identify_t *ident) {
// Convert the identify_t structure to normal view.
	uint8_t *ptr = (uint8_t *)ident->model;
	for (int i = 0; i < 39; i+=2) {
		uint8_t tmp = ptr[i+1];
		ptr[i+1] = ptr[i];
		ptr[i] = tmp;
	}
}